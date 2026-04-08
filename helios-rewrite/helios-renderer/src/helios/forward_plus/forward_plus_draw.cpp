// helios-renderer/src/helios/forward_plus/forward_plus_draw.cpp
//
// Draw system for Forward+ pipeline: skybox + PBR mesh rendering.
// Resolves assets per-entity through GPUResourceCache when available,
// falls back to legacy PBRRenderState mesh/textures otherwise.
#include "helios/forward_plus/forward_plus_draw.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/render_plugin.h"
#include "helios/assets/mesh_asset.h"
#include "helios/assets/material_asset.h"

namespace helios {

void forward_plus_draw(ResMut<RenderContext> ctx,
                       Res<renderer::FramePacket> packet,
                       ResMut<SkyboxState> skybox,
                       ResMut<PBRRenderState> pbr,
                       ResMut<GPUResourceCache> cache,
                       Res<std::shared_ptr<AssetServer>> server) {
    if (!ctx->frame_active) return;

    const float w = static_cast<float>(ctx->swapchain->width());
    const float h = static_cast<float>(ctx->swapchain->height());

    // --- Skybox ---
    if (skybox->valid && skybox->pipeline && skybox->ds) {
        auto& cmd = *ctx->cmd;

        // Update skybox UBO (rotation-only view matrix)
        SkyboxUBOData skybox_data;
        skybox_data.camera_view = packet->camera.view;
        skybox_data.camera_projection = packet->camera.projection;
        skybox_data.brightness = 1.0f;
        skybox->ubo->set_data(&skybox_data, sizeof(skybox_data));

        cmd.bind_pipeline(*skybox->pipeline);
        cmd.set_viewport(0.0f, 0.0f, w, h);
        cmd.set_scissor(0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        cmd.bind_descriptor_set(0, *skybox->ds);
        cmd.bind_vertex_buffer(*skybox->cube_vbo);
        cmd.draw(skybox->vertex_count);
    }

    // --- PBR mesh rendering ---
    if (!pbr->valid || !pbr->pipeline || packet->mesh_draws.empty()) return;

    auto& cmd = *ctx->cmd;
    auto& device = *ctx->device;
    AssetServer* asset_server = server->get();  // may be null if AssetPlugin not used

    // Update PBR camera UBO
    PBRCameraUBO cam_data;
    cam_data.view       = packet->camera.view;
    cam_data.projection = packet->camera.projection;
    cam_data.camera_pos = packet->camera.position;
    cam_data._pad0 = 0.0f;

    if (!packet->dir_lights.empty()) {
        cam_data.light_dir   = packet->dir_lights[0].direction;
        cam_data.light_color = packet->dir_lights[0].color;
        cam_data.light_intensity = packet->dir_lights[0].intensity;
    } else {
        cam_data.light_dir   = glm::vec3(0.0f, -1.0f, -0.5f);
        cam_data.light_color = glm::vec3(1.0f, 0.95f, 0.8f);
        cam_data.light_intensity = 2.0f;
    }
    cam_data._pad1 = 0.0f;
    pbr->camera_ubo->set_data(&cam_data, sizeof(cam_data));

    cmd.bind_pipeline(*pbr->pipeline);
    cmd.set_viewport(0.0f, 0.0f, w, h);
    cmd.set_scissor(0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    cmd.bind_descriptor_set(0, *pbr->camera_ds);

    // Environment cubemap for IBL (from skybox if available)
    rhi::Texture* env_cubemap = skybox->env_cubemap ? skybox->env_cubemap.get() : nullptr;

    for (const auto& draw : packet->mesh_draws) {
        auto mesh_handle = AssetHandle::from_packed(draw.mesh);

        // Try asset-based path (MeshAsset in AssetServer)
        const MeshAsset* mesh_asset = asset_server
            ? asset_server->get<MeshAsset>(mesh_handle)
            : nullptr;

        if (mesh_asset) {
            // Upload mesh to GPU
            const GPUMesh* gpu_mesh = cache->get_or_upload_mesh(mesh_handle, *mesh_asset, device);
            if (!gpu_mesh || !gpu_mesh->vbo || !gpu_mesh->ibo) continue;

            // Resolve material: entity override or mesh default
            auto mat_handle = AssetHandle::from_packed(draw.material);
            if (!mat_handle && mesh_asset->default_material) {
                mat_handle = mesh_asset->default_material.untyped();
            }

            const rhi::DescriptorSet* material_ds = nullptr;

            if (mat_handle && asset_server) {
                const MaterialAsset* mat_asset = asset_server->get<MaterialAsset>(mat_handle);
                if (mat_asset && pbr->material_layout) {
                    const GPUMaterial* gpu_mat = cache->get_or_upload_material(
                        mat_handle, *mat_asset, device, *asset_server,
                        *pbr->material_layout,
                        *cache->get_or_create_default_white(device),
                        *cache->get_or_create_default_blue(device),
                        *cache->get_or_create_default_black(device),
                        env_cubemap);
                    if (gpu_mat && gpu_mat->ds) {
                        material_ds = gpu_mat->ds.get();
                    }
                }
            }

            // Fallback: use legacy material descriptor set
            if (!material_ds && pbr->material_ds) {
                material_ds = pbr->material_ds.get();
            }

            if (!material_ds) continue;

            cmd.bind_descriptor_set(1, *material_ds);
            cmd.bind_vertex_buffer(*gpu_mesh->vbo);
            cmd.bind_index_buffer(*gpu_mesh->ibo);

            PushConstantData pc;
            pc.transform = draw.transform;
            cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                               sizeof(PushConstantData), &pc);
            cmd.draw_indexed(gpu_mesh->index_count);
        } else {
            // Legacy path: single mesh/material from PBRRenderState
            if (pbr->mesh_vbo && pbr->mesh_ibo && pbr->material_ds) {
                cmd.bind_descriptor_set(1, *pbr->material_ds);
                cmd.bind_vertex_buffer(*pbr->mesh_vbo);
                cmd.bind_index_buffer(*pbr->mesh_ibo);

                PushConstantData pc;
                pc.transform = draw.transform;
                cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                                   sizeof(PushConstantData), &pc);
                cmd.draw_indexed(pbr->index_count);
            }
        }
    }
}

} // namespace helios

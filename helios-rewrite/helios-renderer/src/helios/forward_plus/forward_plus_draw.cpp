// helios-renderer/src/helios/forward_plus/forward_plus_draw.cpp
//
// Draw system for Forward+ pipeline: skybox + PBR mesh rendering.
// Resolves assets per-entity through GPUResourceCache when available,
// falls back to legacy PBRRenderState mesh/textures otherwise.
//
// Multi-camera: iterates over all CameraViews in the FramePacket,
// setting viewport/scissor and uploading view/projection per camera.
#include "helios/forward_plus/forward_plus_draw.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/render_plugin.h"
#include "helios/assets/mesh_asset.h"
#include "helios/assets/material_asset.h"

namespace helios {

// Helper: draw skybox for a single camera view
static void draw_skybox_for_view(
    rhi::CommandBuffer& cmd,
    SkyboxState& skybox,
    const renderer::CameraView& view,
    float vp_x, float vp_y, float vp_w, float vp_h)
{
    if (!skybox.valid || !skybox.pipeline || !skybox.ds) return;

    SkyboxUBOData skybox_data;
    skybox_data.camera_view = view.camera.view;
    skybox_data.camera_projection = view.camera.projection;
    skybox_data.brightness = 1.0f;
    skybox.ubo->set_data(&skybox_data, sizeof(skybox_data));

    cmd.bind_pipeline(*skybox.pipeline);
    cmd.set_viewport(vp_x, vp_y, vp_w, vp_h);
    cmd.set_scissor(
        static_cast<uint32_t>(vp_x), static_cast<uint32_t>(vp_y),
        static_cast<uint32_t>(vp_w), static_cast<uint32_t>(vp_h));
    cmd.bind_descriptor_set(0, *skybox.ds);
    cmd.bind_vertex_buffer(*skybox.cube_vbo);
    cmd.draw(skybox.vertex_count);
}

// Helper: draw all PBR meshes for a single camera view
static void draw_meshes_for_view(
    rhi::CommandBuffer& cmd,
    rhi::Device& device,
    PBRRenderState& pbr,
    SkyboxState& skybox,
    GPUResourceCache& cache,
    AssetServer* asset_server,
    const renderer::FramePacket& packet,
    const renderer::CameraView& view,
    float vp_x, float vp_y, float vp_w, float vp_h,
    PBRRenderState::CameraSlot* slot = nullptr)
{
    if (!pbr.valid || !pbr.pipeline || packet.mesh_draws.empty()) return;

    PBRCameraUBO cam_data;
    cam_data.view       = view.camera.view;
    cam_data.projection = view.camera.projection;
    cam_data.camera_pos = view.camera.position;
    cam_data._pad0 = 0.0f;

    if (!packet.dir_lights.empty()) {
        cam_data.light_dir   = packet.dir_lights[0].direction;
        cam_data.light_color = packet.dir_lights[0].color;
        cam_data.light_intensity = packet.dir_lights[0].intensity;
    } else {
        cam_data.light_dir   = glm::vec3(0.0f, -1.0f, -0.5f);
        cam_data.light_color = glm::vec3(1.0f, 0.95f, 0.8f);
        cam_data.light_intensity = 2.0f;
    }
    cam_data._pad1 = 0.0f;

    // Use per-camera UBO slot if available, otherwise fall back to shared
    rhi::Buffer* ubo = slot ? slot->ubo.get() : pbr.camera_ubo.get();
    rhi::DescriptorSet* ds = slot ? slot->ds.get() : pbr.camera_ds.get();
    ubo->set_data(&cam_data, sizeof(cam_data));

    cmd.bind_pipeline(*pbr.pipeline);
    cmd.set_viewport(vp_x, vp_y, vp_w, vp_h);
    cmd.set_scissor(
        static_cast<uint32_t>(vp_x), static_cast<uint32_t>(vp_y),
        static_cast<uint32_t>(vp_w), static_cast<uint32_t>(vp_h));
    cmd.bind_descriptor_set(0, *ds);

    // Environment cubemap for IBL (from skybox if available)
    rhi::Texture* env_cubemap = skybox.env_cubemap ? skybox.env_cubemap.get() : nullptr;

    for (const auto& draw : packet.mesh_draws) {
        auto mesh_handle = AssetHandle::from_packed(draw.mesh);

        // Try asset-based path (MeshAsset in AssetServer)
        const MeshAsset* mesh_asset = asset_server
            ? asset_server->get<MeshAsset>(mesh_handle)
            : nullptr;

        if (mesh_asset) {
            const GPUMesh* gpu_mesh = cache.get_or_upload_mesh(mesh_handle, *mesh_asset, device);
            if (!gpu_mesh || !gpu_mesh->vbo || !gpu_mesh->ibo) continue;

            auto mat_handle = AssetHandle::from_packed(draw.material);
            if (!mat_handle && mesh_asset->default_material) {
                mat_handle = mesh_asset->default_material.untyped();
            }

            const rhi::DescriptorSet* material_ds = nullptr;

            if (mat_handle && asset_server) {
                const MaterialAsset* mat_asset = asset_server->get<MaterialAsset>(mat_handle);
                if (mat_asset && pbr.material_layout) {
                    const GPUMaterial* gpu_mat = cache.get_or_upload_material(
                        mat_handle, *mat_asset, device, *asset_server,
                        *pbr.material_layout,
                        *cache.get_or_create_default_white(device),
                        *cache.get_or_create_default_blue(device),
                        *cache.get_or_create_default_black(device),
                        env_cubemap);
                    if (gpu_mat && gpu_mat->ds) {
                        material_ds = gpu_mat->ds.get();
                    }
                }
            }

            if (!material_ds && pbr.material_ds) {
                material_ds = pbr.material_ds.get();
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
            if (pbr.mesh_vbo && pbr.mesh_ibo && pbr.material_ds) {
                cmd.bind_descriptor_set(1, *pbr.material_ds);
                cmd.bind_vertex_buffer(*pbr.mesh_vbo);
                cmd.bind_index_buffer(*pbr.mesh_ibo);

                PushConstantData pc;
                pc.transform = draw.transform;
                cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                                   sizeof(PushConstantData), &pc);
                cmd.draw_indexed(pbr.index_count);
            }
        }
    }
}

void forward_plus_draw(ResMut<RenderContext> ctx,
                       Res<renderer::FramePacket> packet,
                       ResMut<SkyboxState> skybox,
                       ResMut<PBRRenderState> pbr,
                       ResMut<GPUResourceCache> cache,
                       Res<std::shared_ptr<AssetServer>> server) {
    if (!ctx->frame_active) return;

    const float sw_w = static_cast<float>(ctx->swapchain->width());
    const float sw_h = static_cast<float>(ctx->swapchain->height());
    auto& cmd = *ctx->cmd;
    auto& device = *ctx->device;
    AssetServer* asset_server = server->get();

    // If no camera_views were extracted, fall back to the legacy single-camera
    // path using packet->camera (covers backward compat with old scenes).
    if (packet->camera_views.empty()) {
        renderer::CameraView fallback;
        fallback.camera = packet->camera;
        // full window viewport
        draw_skybox_for_view(cmd, *skybox, fallback, 0.0f, 0.0f, sw_w, sw_h);
        draw_meshes_for_view(cmd, device, *pbr, *skybox, *cache,
                             asset_server, *packet, fallback,
                             0.0f, 0.0f, sw_w, sw_h);
        return;
    }

    // Multi-camera path: ensure we have enough per-camera UBO slots.
    while (pbr->camera_slots.size() < packet->camera_views.size()) {
        PBRRenderState::CameraSlot slot;
        rhi::BufferDesc ubo_desc;
        ubo_desc.size = sizeof(PBRCameraUBO);
        ubo_desc.usage = rhi::BufferUsage::Uniform;
        ubo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
        ubo_desc.debug_name = "PBRCameraUBO_" + std::to_string(pbr->camera_slots.size());
        slot.ubo = device.create_buffer(ubo_desc);

        slot.ds = device.allocate_descriptor_set(*pbr->camera_layout);
        device.update_descriptor_set(*slot.ds, {
            rhi::DescriptorWrite{
                .binding = 0,
                .type = rhi::DescriptorType::UniformBuffer,
                .buffer_handle = slot.ubo.get(),
                .range = sizeof(PBRCameraUBO),
            },
        });
        pbr->camera_slots.push_back(std::move(slot));
    }

    // Iterate over each camera view in order.
    for (size_t i = 0; i < packet->camera_views.size(); i++) {
        const auto& view = packet->camera_views[i];
        const float vp_x = view.viewport_x * sw_w;
        const float vp_y = view.viewport_y * sw_h;
        const float vp_w = view.viewport_w * sw_w;
        const float vp_h = view.viewport_h * sw_h;

        draw_skybox_for_view(cmd, *skybox, view, vp_x, vp_y, vp_w, vp_h);

        // Use this camera's dedicated UBO slot
        draw_meshes_for_view(cmd, device, *pbr, *skybox, *cache,
                             asset_server, *packet, view,
                             vp_x, vp_y, vp_w, vp_h,
                             &pbr->camera_slots[i]);
    }
}

} // namespace helios

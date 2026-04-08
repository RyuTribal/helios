// helios-renderer/src/helios/forward_plus/forward_plus_draw.cpp
//
// Draw system for Forward+ pipeline: skybox + PBR mesh rendering.
// Extracted from the old present_frame monolith in render_plugin.cpp.
#include "helios/forward_plus/forward_plus_draw.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/render_plugin.h"

namespace helios {

void forward_plus_draw(ResMut<RenderContext> ctx,
                       Res<renderer::FramePacket> packet,
                       ResMut<SkyboxState> skybox,
                       ResMut<PBRRenderState> pbr) {
    if (!ctx->swapchain) return;

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
    if (pbr->valid && !packet->mesh_draws.empty()) {
        auto& cmd = *ctx->cmd;

        // Update PBR camera UBO
        PBRCameraUBO cam_data;
        cam_data.view       = packet->camera.view;
        cam_data.projection = packet->camera.projection;
        cam_data.camera_pos = packet->camera.position;
        cam_data._pad0 = 0.0f;

        // Use first directional light if available, otherwise default sun
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
        cmd.bind_descriptor_set(1, *pbr->material_ds);

        cmd.bind_vertex_buffer(*pbr->mesh_vbo);
        cmd.bind_index_buffer(*pbr->mesh_ibo);

        for (const auto& draw : packet->mesh_draws) {
            PushConstantData pc;
            pc.transform = draw.transform;
            cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                               sizeof(PushConstantData), &pc);
            cmd.draw_indexed(pbr->index_count);
        }
    }
}

} // namespace helios

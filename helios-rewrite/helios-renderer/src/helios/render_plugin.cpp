// helios-renderer/src/helios/render_plugin.cpp
#include "helios/render_plugin.h"
#include "helios/forward_plus/simple_render_state.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/graph/frame_packet.h"
#include "helios/rhi/rhi.h"
#include "helios/rhi/rhi_factory.h"
#include "helios/window/windows.h"
#include "helios/window/window_events.h"
#include "helios/ecs/app.h"
#include "helios/ecs/system_params.h"
#include "helios/core/assert.h"
#include "helios/core/log_macros.h"

#include <GLFW/glfw3.h>

HELIOS_DEFINE_LOG_CHANNEL(Render);

namespace helios {

// --- System: present each frame ---
void present_frame(ResMut<RenderContext> ctx,
                   Res<renderer::FramePacket> packet,
                   ResMut<SimpleRenderState> simple) {
    HELIOS_ASSERT(ctx->device != nullptr, "RenderContext::device must be valid");
    HELIOS_ASSERT(ctx->cmd != nullptr, "RenderContext::cmd must be valid");
    if (!ctx->swapchain) return;

    if (!ctx->swapchain->acquire_next_image()) {
        HELIOS_LOG(Render, Debug, "Swapchain acquire failed, skipping frame");
        return;
    }

    ctx->cmd->begin();

    rhi::ClearValues clear;
    clear.color[0] = 0.12f;
    clear.color[1] = 0.14f;
    clear.color[2] = 0.18f;
    clear.color[3] = 1.0f;
    ctx->swapchain->begin_rendering(*ctx->cmd, clear);

    // --- Simple flat-color mesh rendering ---
    if (simple->valid && !packet->mesh_draws.empty()) {
        auto& cmd = *ctx->cmd;
        const float w = static_cast<float>(ctx->swapchain->width());
        const float h = static_cast<float>(ctx->swapchain->height());

        // Update camera UBO
        CameraUBOData cam_data;
        cam_data.view       = packet->camera.view;
        cam_data.projection = packet->camera.projection;
        simple->camera_ubo->set_data(&cam_data, sizeof(cam_data));

        // Bind pipeline, viewport, scissor
        cmd.bind_pipeline(*simple->pipeline);
        cmd.set_viewport(0.0f, 0.0f, w, h);
        cmd.set_scissor(0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h));

        // Bind camera descriptor set
        cmd.bind_descriptor_set(0, *simple->camera_ds);

        // Bind cube geometry
        cmd.bind_vertex_buffer(*simple->cube_vbo);
        cmd.bind_index_buffer(*simple->cube_ibo);

        // Draw each mesh from the frame packet
        for (const auto& draw : packet->mesh_draws) {
            PushConstantData pc;
            pc.transform = draw.transform;
            cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                               sizeof(PushConstantData), &pc);
            cmd.draw_indexed(simple->cube_index_count);
        }
    }

    ctx->swapchain->end_rendering(*ctx->cmd);

    ctx->cmd->end();

    ctx->device->submit_for_present(*ctx->cmd, *ctx->swapchain);
    ctx->swapchain->present();
}

// --- System: handle window resize → recreate swapchain ---
void handle_swapchain_resize(
    ResMut<RenderContext> ctx,
    Res<Windows> windows,
    EventReader<WindowResized> resize_events)
{
    for (const auto& e : resize_events) {
        if (e.width == 0 || e.height == 0) continue;
        if (!windows->has_primary()) continue;
        if (e.window_id != windows->primary_id()) continue;

        // Skip if size hasn't actually changed (Wayland sends duplicates)
        if (ctx->swapchain &&
            ctx->swapchain->width() == e.width &&
            ctx->swapchain->height() == e.height) {
            continue;
        }

        HELIOS_LOG(Render, Info, "Swapchain resize: {}x{}", e.width, e.height);
        ctx->device->wait_idle();

        rhi::SwapchainDesc desc;
        desc.width = e.width;
        desc.height = e.height;
        auto new_swapchain = ctx->device->create_swapchain(desc);
        if (new_swapchain) {
            ctx->swapchain = std::move(new_swapchain);
        } else {
            HELIOS_LOG(Render, Warn, "Swapchain recreation failed for {}x{}, will retry", e.width, e.height);
        }
    }
}

// --- Plugin build ---
void RenderPlugin::build(App& app) {
    HELIOS_LOG(Render, Info, "Initializing RenderPlugin");

    auto& windows = app.world().resource<Windows>();
    HELIOS_ASSERT(windows.has_primary(), "RenderPlugin requires WindowPlugin to be added first");

    auto* native = static_cast<GLFWwindow*>(windows.primary().native_handle());

    auto device = rhi::create_device(backend, app_name.c_str(), native, gpu_index, enable_validation);
    HELIOS_ASSERT(device != nullptr, "Failed to create RHI device");

    rhi::SwapchainDesc sc_desc;
    sc_desc.width = windows.primary().width();
    sc_desc.height = windows.primary().height();
    auto swapchain = device->create_swapchain(sc_desc);
    auto cmd = device->create_command_buffer();

    RenderContext ctx;
    ctx.device = std::move(device);
    ctx.swapchain = std::move(swapchain);
    ctx.cmd = std::move(cmd);
    app.insert_resource(std::move(ctx));

    // Insert default resources so present_frame's parameter resolution
    // succeeds even without ForwardPlusPlugin.  If ForwardPlusPlugin IS
    // added, it will overwrite these with properly populated versions.
    if (!app.world().has_resource<renderer::FramePacket>())
        app.insert_resource(renderer::FramePacket{});
    if (!app.world().has_resource<SimpleRenderState>())
        app.insert_resource(SimpleRenderState{});

    app.add_system(Schedule::PreRender, present_frame, "present_frame");
    app.add_system(Schedule::PreUpdate, handle_swapchain_resize, "handle_swapchain_resize");

    HELIOS_LOG(Render, Info, "RenderPlugin ready");
}

} // namespace helios

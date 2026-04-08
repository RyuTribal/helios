// helios-renderer/src/helios/render_plugin.cpp
#include "helios/render_plugin.h"
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

// --- System: begin frame (acquire + begin command buffer + begin rendering) ---
void frame_begin(ResMut<RenderContext> ctx) {
    HELIOS_ASSERT(ctx->device != nullptr, "RenderContext::device must be valid");
    HELIOS_ASSERT(ctx->cmd != nullptr, "RenderContext::cmd must be valid");
    if (!ctx->swapchain) return;

    if (!ctx->swapchain->acquire_next_image()) {
        HELIOS_LOG(Render, Debug, "Swapchain acquire failed, skipping frame");
        return;
    }

    ctx->cmd->begin();

    rhi::ClearValues clear;
    clear.color[0] = ctx->clear_color[0];
    clear.color[1] = ctx->clear_color[1];
    clear.color[2] = ctx->clear_color[2];
    clear.color[3] = ctx->clear_color[3];
    clear.depth = 1.0f;

    // Begin rendering with depth attachment if available
    rhi::Texture* depth_ptr = ctx->depth_texture ? ctx->depth_texture.get() : nullptr;
    ctx->swapchain->begin_rendering(*ctx->cmd, clear, depth_ptr);
}

// --- System: end frame (end rendering + submit + present) ---
void frame_end(ResMut<RenderContext> ctx) {
    if (!ctx->swapchain) return;
    // If acquire failed in frame_begin, the command buffer was never started.
    // The swapchain tracks this internally; end_rendering / present are safe no-ops.

    ctx->swapchain->end_rendering(*ctx->cmd);
    ctx->cmd->end();
    ctx->device->submit_for_present(*ctx->cmd, *ctx->swapchain);
    ctx->swapchain->present();
}

// --- System: handle window resize -> recreate swapchain + depth buffer ---
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

            // Recreate depth buffer to match the new swapchain size
            if (ctx->depth_texture) {
                rhi::TextureDesc depth_desc;
                depth_desc.width = e.width;
                depth_desc.height = e.height;
                depth_desc.format = rhi::TextureFormat::Depth32F;
                depth_desc.usage = rhi::TextureUsage::DepthAttachment;
                depth_desc.debug_name = "DepthBuffer";
                ctx->depth_texture = ctx->device->create_texture(depth_desc);
            }
        } else {
            HELIOS_LOG(Render, Warn, "Swapchain recreation failed for {}x{}, will retry", e.width, e.height);
        }
    }
}

// --- System: flush GPU on shutdown ---
void gpu_shutdown(ResMut<RenderContext> ctx) {
    if (ctx->device) {
        HELIOS_LOG(Render, Info, "Flushing GPU before shutdown");
        ctx->device->wait_idle();
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
    ctx.clear_color[0] = clear_color[0];
    ctx.clear_color[1] = clear_color[1];
    ctx.clear_color[2] = clear_color[2];
    ctx.clear_color[3] = clear_color[3];
    app.insert_resource(std::move(ctx));

    // Register frame_begin and frame_end with explicit ordering.
    // Other plugins insert draw systems between them using id_of().
    auto begin_id = app.add_system(Schedule::PreRender, frame_begin, "frame_begin").id();
    app.add_system(Schedule::PreRender, frame_end, "frame_end").after(begin_id);

    app.add_system(Schedule::PreUpdate, handle_swapchain_resize, "handle_swapchain_resize");
    app.add_system(Schedule::Shutdown, gpu_shutdown, "gpu_shutdown");

    HELIOS_LOG(Render, Info, "RenderPlugin ready");
}

} // namespace helios

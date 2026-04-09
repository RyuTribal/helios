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

// Helper: recreate swapchain + depth buffer at current framebuffer size.
// Called from frame_begin when acquire fails (same pattern as old engine).
static void recreate_swapchain(RenderContext& ctx, GLFWwindow* glfw_win) {
    int fb_w, fb_h;
    glfwGetFramebufferSize(glfw_win, &fb_w, &fb_h);
    if (fb_w <= 0 || fb_h <= 0) return;

    uint32_t w = static_cast<uint32_t>(fb_w);
    uint32_t h = static_cast<uint32_t>(fb_h);

    ctx.device->wait_idle();

    rhi::SwapchainDesc desc;
    desc.width = w;
    desc.height = h;
    auto new_sc = ctx.device->create_swapchain(desc);

    // Retry with fresh size if first attempt fails (Wayland TOCTOU race)
    if (!new_sc) {
        glfwGetFramebufferSize(glfw_win, &fb_w, &fb_h);
        if (fb_w > 0 && fb_h > 0) {
            desc.width = static_cast<uint32_t>(fb_w);
            desc.height = static_cast<uint32_t>(fb_h);
            new_sc = ctx.device->create_swapchain(desc);
        }
    }

    if (new_sc) {
        ctx.swapchain = std::move(new_sc);

        if (ctx.depth_texture) {
            rhi::TextureDesc depth_desc;
            depth_desc.width = ctx.swapchain->width();
            depth_desc.height = ctx.swapchain->height();
            depth_desc.format = rhi::TextureFormat::Depth32F;
            depth_desc.usage = rhi::TextureUsage::DepthAttachment;
            depth_desc.debug_name = "DepthBuffer";
            ctx.depth_texture = ctx.device->create_texture(depth_desc);
        }

        HELIOS_LOG(Render, Info, "Swapchain recreated: {}x{}",
                   ctx.swapchain->width(), ctx.swapchain->height());
    }
}

// --- System: begin frame (acquire + begin command buffer + begin rendering) ---
void frame_begin(ResMut<RenderContext> ctx, Res<Windows> windows) {
    ctx->frame_active = false;
    HELIOS_ASSERT(ctx->device != nullptr, "RenderContext::device must be valid");
    HELIOS_ASSERT(ctx->cmd != nullptr, "RenderContext::cmd must be valid");
    if (!ctx->swapchain) return;
    if (!windows->has_primary()) return;

    if (!ctx->swapchain->acquire_next_image()) {
        // Acquire failed — recreate swapchain immediately (like old engine)
        auto* glfw_win = static_cast<GLFWwindow*>(windows->primary().native_handle());
        recreate_swapchain(*ctx, glfw_win);
        return;  // skip this frame, render next frame with new swapchain
    }

    ctx->frame_active = true;
    ctx->cmd->begin();

    rhi::ClearValues clear;
    clear.color[0] = ctx->clear_color[0];
    clear.color[1] = ctx->clear_color[1];
    clear.color[2] = ctx->clear_color[2];
    clear.color[3] = ctx->clear_color[3];
    clear.depth = 1.0f;

    rhi::Texture* depth_ptr = ctx->depth_texture ? ctx->depth_texture.get() : nullptr;
    ctx->swapchain->begin_rendering(*ctx->cmd, clear, depth_ptr);
}

// --- System: end frame (end rendering + submit + present) ---
void frame_end(ResMut<RenderContext> ctx) {
    if (!ctx->frame_active) return;

    ctx->swapchain->end_rendering(*ctx->cmd);
    ctx->cmd->end();
    ctx->device->submit_for_present(*ctx->cmd, *ctx->swapchain);

    if (!ctx->swapchain->present()) {
        // Present failed (out of date). Don't try to recreate here —
        // the surface may be in a transient state. The next frame's
        // acquire will fail and frame_begin will recreate then.
        HELIOS_LOG(Render, Debug, "Present out of date, will recreate on next acquire");
    }

    ctx->frame_active = false;
}

// --- System: handle window resize -> recreate swapchain + depth buffer ---
void handle_swapchain_resize(
    ResMut<RenderContext> ctx,
    Res<Windows> windows,
    EventReader<WindowResized> resize_events)
{
    for (const auto& e : resize_events) {
        if (!windows->has_primary()) continue;
        if (e.window_id != windows->primary_id()) continue;

        // Re-query actual framebuffer size -- the event values can be stale
        // on Wayland (TOCTOU race acknowledged by Khronos).
        auto* glfw_win = static_cast<GLFWwindow*>(windows->primary().native_handle());
        int fb_w, fb_h;
        glfwGetFramebufferSize(glfw_win, &fb_w, &fb_h);

        if (fb_w <= 0 || fb_h <= 0) continue;  // minimized

        uint32_t w = static_cast<uint32_t>(fb_w);
        uint32_t h = static_cast<uint32_t>(fb_h);

        // Skip if size hasn't actually changed (Wayland sends duplicates)
        if (ctx->swapchain &&
            ctx->swapchain->width() == w &&
            ctx->swapchain->height() == h) {
            continue;
        }

        HELIOS_LOG(Render, Info, "Swapchain resize: {}x{}", w, h);
        ctx->device->wait_idle();

        rhi::SwapchainDesc desc;
        desc.width = w;
        desc.height = h;
        auto new_swapchain = ctx->device->create_swapchain(desc);

        // Retry once with fresh dimensions if creation failed
        if (!new_swapchain) {
            glfwGetFramebufferSize(glfw_win, &fb_w, &fb_h);
            if (fb_w > 0 && fb_h > 0) {
                desc.width = static_cast<uint32_t>(fb_w);
                desc.height = static_cast<uint32_t>(fb_h);
                new_swapchain = ctx->device->create_swapchain(desc);
            }
        }

        if (new_swapchain) {
            ctx->swapchain = std::move(new_swapchain);

            // Recreate depth buffer to match the new swapchain size
            if (ctx->depth_texture) {
                rhi::TextureDesc depth_desc;
                depth_desc.width = desc.width;
                depth_desc.height = desc.height;
                depth_desc.format = rhi::TextureFormat::Depth32F;
                depth_desc.usage = rhi::TextureUsage::DepthAttachment;
                depth_desc.debug_name = "DepthBuffer";
                ctx->depth_texture = ctx->device->create_texture(depth_desc);
            }
        } else {
            HELIOS_LOG(Render, Warn, "Swapchain recreation failed for {}x{}, will retry", desc.width, desc.height);
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

    // Poll events once to let Wayland's initial configure settle.
    // Without this, the swapchain is created at the requested size, but
    // the compositor immediately sends a different size on the first frame.
    glfwPollEvents();

    int fb_w, fb_h;
    glfwGetFramebufferSize(native, &fb_w, &fb_h);

    rhi::SwapchainDesc sc_desc;
    sc_desc.width = (fb_w > 0) ? static_cast<uint32_t>(fb_w) : windows.primary().width();
    sc_desc.height = (fb_h > 0) ? static_cast<uint32_t>(fb_h) : windows.primary().height();
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

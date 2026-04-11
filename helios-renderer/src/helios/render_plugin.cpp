#include "helios/render_plugin.h"
#include "helios/render_settings.h"
#include "helios/render_schedule.h"
#include "helios/camera_render_schedule.h"
#include "helios/camera_driver.h"
#include "helios/rhi/rhi_factory.h"
#include "helios/rhi/rhi_swapchain.h"
#include "helios/rhi/rhi_command_buffer.h"
#include "helios/forward_plus/pbr_render_state.h"
#include "helios/forward_plus/skybox_state.h"
#include "helios/forward_plus/gpu_cache.h"
#include "helios/window/windows.h"
#include "helios/ecs/app.h"
#include "helios/ecs/system_params.h"
#include "helios/core/assert.h"
#include "helios/core/log_macros.h"

HELIOS_DEFINE_LOG_CHANNEL(Render);

namespace helios {

// =========================================================================
//  RenderContext methods
// =========================================================================

void RenderContext::resize(uint32_t fb_width, uint32_t fb_height,
                           rhi::PresentMode present_mode, float resolution_scale) {
    if (fb_width == 0 || fb_height == 0) return;

    uint32_t w = fb_width;
    uint32_t h = fb_height;

    device->wait_idle();
    swapchain.reset();

    rhi::SwapchainDesc desc;
    desc.width = w;
    desc.height = h;
    desc.present_mode = present_mode;
    swapchain = device->create_swapchain(desc);
    HELIOS_ASSERT(swapchain != nullptr, "Failed to create swapchain");

    // Scene framebuffer at scaled resolution
    uint32_t sw = static_cast<uint32_t>(static_cast<float>(w) * resolution_scale);
    uint32_t sh = static_cast<uint32_t>(static_cast<float>(h) * resolution_scale);
    if (sw == 0) sw = 1;
    if (sh == 0) sh = 1;
    resize_scene_fb(sw, sh);

    HELIOS_LOG(Render, Info, "Render targets: swapchain={}x{} scene={}x{} present_mode={}",
               w, h, sw, sh, static_cast<int>(present_mode));
}

std::pair<rhi::Texture*, rhi::Texture*> RenderContext::get_or_create_target(
    uint32_t id, uint32_t width, uint32_t height)
{
    auto& entry = camera_targets[id];
    if (entry.width == width && entry.height == height && entry.color) {
        return {entry.color.get(), entry.depth.get()};
    }

    if (entry.color) device->defer_destroy(std::move(entry.color));
    if (entry.depth) device->defer_destroy(std::move(entry.depth));

    rhi::TextureDesc color_desc;
    color_desc.width = width;
    color_desc.height = height;
    color_desc.format = rhi::TextureFormat::BGRA8;
    color_desc.usage = rhi::TextureUsage::ColorAttachment
                     | rhi::TextureUsage::Sampled
                     | rhi::TextureUsage::Transfer;
    color_desc.debug_name = "CameraTarget_" + std::to_string(id) + "_Color";
    entry.color = device->create_texture(color_desc);

    rhi::TextureDesc depth_desc;
    depth_desc.width = width;
    depth_desc.height = height;
    depth_desc.format = rhi::TextureFormat::Depth32F;
    depth_desc.usage = rhi::TextureUsage::DepthAttachment;
    depth_desc.debug_name = "CameraTarget_" + std::to_string(id) + "_Depth";
    entry.depth = device->create_texture(depth_desc);

    entry.width = width;
    entry.height = height;
    return {entry.color.get(), entry.depth.get()};
}

void RenderContext::resize_scene_fb(uint32_t width, uint32_t height) {
    if (width == scene_width && height == scene_height && scene_fbs[0].color) return;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (scene_fbs[i].color) device->defer_destroy(std::move(scene_fbs[i].color));
        if (scene_fbs[i].depth) device->defer_destroy(std::move(scene_fbs[i].depth));

        rhi::TextureDesc color_desc;
        color_desc.width = width;
        color_desc.height = height;
        color_desc.format = rhi::TextureFormat::BGRA8;
        color_desc.usage = rhi::TextureUsage::ColorAttachment
                         | rhi::TextureUsage::Sampled
                         | rhi::TextureUsage::Transfer;
        color_desc.debug_name = "SceneColor_" + std::to_string(i);
        scene_fbs[i].color = device->create_texture(color_desc);

        rhi::TextureDesc depth_desc;
        depth_desc.width = width;
        depth_desc.height = height;
        depth_desc.format = rhi::TextureFormat::Depth32F;
        depth_desc.usage = rhi::TextureUsage::DepthAttachment;
        depth_desc.debug_name = "SceneDepth_" + std::to_string(i);
        scene_fbs[i].depth = device->create_texture(depth_desc);
    }

    scene_width = width;
    scene_height = height;
}

// =========================================================================
//  Frame systems — clean, no defensive null checks
// =========================================================================

void frame_begin(ResMut<RenderContext> ctx, ResMut<RenderSettings> settings, Res<Windows> windows) {
    ctx->frame_active = false;
    ctx->scene_pass_active = false;

    uint32_t fb_w = windows->primary().width();
    uint32_t fb_h = windows->primary().height();

    // Resize if present mode changed or window size changed
    bool needs_resize = has_flag(settings->dirty, RenderDirty::Swapchain);
    if (!needs_resize) {
        needs_resize = (fb_w > 0 && fb_h > 0) &&
            (fb_w != ctx->swapchain->width() || fb_h != ctx->swapchain->height());
    }
    if (needs_resize) {
        ctx->resize(fb_w, fb_h, settings->present_mode, settings->resolution_scale);
        settings->dirty &= ~RenderDirty::Swapchain;
    }

    // Acquire next swapchain image
    if (!ctx->swapchain->acquire_next_image()) {
        ctx->resize(fb_w, fb_h, settings->present_mode, settings->resolution_scale);
        return;
    }

    // Select per-frame command buffer and begin recording
    uint32_t frame_idx = ctx->swapchain->current_frame();
    ctx->cmd = ctx->cmds[frame_idx % RenderContext::MAX_FRAMES_IN_FLIGHT].get();
    ctx->frame_active = true;
    ctx->cmd->begin();

    // Resize scene framebuffer if resolution scale changed
    uint32_t sw = static_cast<uint32_t>(static_cast<float>(ctx->swapchain->width()) * settings->resolution_scale);
    uint32_t sh = static_cast<uint32_t>(static_cast<float>(ctx->swapchain->height()) * settings->resolution_scale);
    if (sw == 0) sw = 1;
    if (sh == 0) sh = 1;
    ctx->resize_scene_fb(sw, sh);

    // Select this frame's scene framebuffer (triple-buffered)
    uint32_t fi = frame_idx % RenderContext::MAX_FRAMES_IN_FLIGHT;
    ctx->current_frame_index = fi;
    ctx->scene_color = ctx->scene_fbs[fi].color.get();
    ctx->scene_depth = ctx->scene_fbs[fi].depth.get();
}

void frame_end(ResMut<RenderContext> ctx) {
    if (!ctx->frame_active) return;

    // scene_color was already transitioned to ShaderReadOnly by camera_driver
    // after the last camera rendered to it.

    // In Offscreen mode, open swapchain pass for ImGui
    if (ctx->mode == RenderMode::Offscreen) {
        rhi::ClearValues clear;
        clear.color[0] = clear.color[1] = clear.color[2] = 0.0f;
        clear.color[3] = 1.0f;
        ctx->swapchain->begin_rendering(*ctx->cmd, clear, nullptr);
    }
}

void frame_present(ResMut<RenderContext> ctx) {
    if (!ctx->frame_active) return;

    if (ctx->mode == RenderMode::Direct) {
        ctx->swapchain->blit_from(*ctx->cmd, *ctx->scene_color,
                                   ctx->scene_width, ctx->scene_height);
    } else {
        ctx->swapchain->end_rendering(*ctx->cmd);
    }

    ctx->cmd->end();
    ctx->device->submit_for_present(*ctx->cmd, *ctx->swapchain);
    ctx->swapchain->present();
    ctx->device->flush_deferred_deletions();
    ctx->frame_active = false;
}

// =========================================================================
//  Shutdown
// =========================================================================

static void gpu_shutdown(World& world) {
    auto& ctx = world.resource<RenderContext>();
    HELIOS_LOG(Render, Info, "Flushing GPU before shutdown");
    ctx.device->wait_idle();

    // Destroy ALL GPU-owning resources before the device.
    // World destructor order is unpredictable — if the device
    // goes first, other destructors crash accessing dead VMA.

    // ForwardPlus pipeline state (textures, buffers, pipelines, descriptor sets)
    if (world.has_resource<PBRRenderState>())
        world.resource<PBRRenderState>() = PBRRenderState{};
    if (world.has_resource<SkyboxState>())
        world.resource<SkyboxState>() = SkyboxState{};
    if (world.has_resource<GPUResourceCache>())
        world.resource<GPUResourceCache>() = GPUResourceCache{};

    // RenderContext resources
    ctx.camera_targets.clear();
    for (auto& fb : ctx.scene_fbs) {
        fb.color.reset();
        fb.depth.reset();
    }
    ctx.scene_color = nullptr;
    ctx.scene_depth = nullptr;
    for (auto& cmd : ctx.cmds) cmd.reset();
    ctx.cmd = nullptr;
    ctx.swapchain.reset();
    ctx.device->flush_deferred_deletions();
    ctx.device.reset();
}

// =========================================================================
//  Plugin build
// =========================================================================

void RenderPlugin::build(App& app) {
    HELIOS_LOG(Render, Info, "Initializing RenderPlugin");

    auto& windows = app.world().resource<Windows>();
    HELIOS_ASSERT(windows.has_primary(), "RenderPlugin requires WindowPlugin");

    auto device = rhi::create_device(backend, app_name.c_str(),
        windows.primary().native_handle(), gpu_index, enable_validation);
    HELIOS_ASSERT(device != nullptr, "Failed to create RHI device");

    // Create swapchain immediately with the correct present mode
    rhi::SwapchainDesc sc_desc;
    sc_desc.width = windows.primary().width();
    sc_desc.height = windows.primary().height();
    sc_desc.present_mode = initial_present_mode;
    auto swapchain = device->create_swapchain(sc_desc);
    HELIOS_ASSERT(swapchain != nullptr, "Failed to create swapchain");

    RenderContext ctx;
    ctx.device = std::move(device);
    ctx.swapchain = std::move(swapchain);
    for (uint32_t i = 0; i < RenderContext::MAX_FRAMES_IN_FLIGHT; i++) {
        ctx.cmds[i] = ctx.device->create_command_buffer();
    }
    ctx.cmd = ctx.cmds[0].get();
    ctx.mode = mode;

    // Create scene framebuffer immediately
    ctx.resize_scene_fb(sc_desc.width, sc_desc.height);

    app.insert_resource(std::move(ctx));
    app.insert_resource(RenderSettings{});
    app.insert_resource(RenderScheduleRegistry{});
    app.insert_resource(CameraRenderSchedules{});

    // System ordering: frame_begin → camera_driver → frame_end → (ImGui) → frame_present
    auto begin_id = app.add_system(Schedule::PreRender, frame_begin, "frame_begin").id();
    auto end_id = app.add_system(Schedule::PreRender, frame_end, "frame_end").after(begin_id).id();
    app.add_system(Schedule::PreRender, frame_present, "frame_present").after(end_id);
    app.add_system(Schedule::PreRender, camera_driver, "camera_driver")
        .after(begin_id).before(end_id);

    app.add_system(Schedule::Shutdown, gpu_shutdown, "gpu_shutdown");

    HELIOS_LOG(Render, Info, "RenderPlugin ready (present_mode={})",
               static_cast<int>(initial_present_mode));
}

} // namespace helios

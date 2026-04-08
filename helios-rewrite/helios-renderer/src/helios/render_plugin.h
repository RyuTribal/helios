// helios-renderer/src/helios/render_plugin.h
#pragma once

#include "helios/ecs/system_params.h"
#include "helios/rhi/rhi_factory.h"
#include "helios/rhi/rhi_types.h"
#include <cstdint>
#include <memory>
#include <string>

namespace helios {

class App;

/// Core rendering infrastructure plugin. Creates the RHI device, swapchain,
/// and command buffer. Handles per-frame acquire/present and swapchain resize.
///
/// This is pure GPU infrastructure — it knows nothing about shading models.
/// Pair with ForwardPlusPlugin, DeferredPlugin, etc. for actual rendering.
///
/// Without a shading plugin, RenderPlugin just clears to the clear color
/// and presents — enough to make the window visible on Wayland.
struct RenderPlugin {
    rhi::Backend backend = rhi::Backend::Vulkan;
    uint32_t gpu_index = UINT32_MAX;  // auto-select best
    std::string app_name = "Helios";
    bool enable_validation = true;
    float clear_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};

    void build(App& app);
};

/// GPU rendering context stored as an ECS resource by RenderPlugin.
/// Shading plugins and systems access this for the device and swapchain.
struct RenderContext {
    std::unique_ptr<rhi::Device> device;
    std::unique_ptr<rhi::Swapchain> swapchain;
    std::unique_ptr<rhi::CommandBuffer> cmd;
    void* surface = nullptr;  // owned, destroyed with device
};

// Forward declarations for system functions (needed for cross-plugin ordering).
namespace renderer { struct FramePacket; }
struct SimpleRenderState;

/// The present_frame system function. Declared here so other plugins can
/// reference it for explicit ordering via app.id_of(present_frame).
void present_frame(ResMut<RenderContext> ctx,
                   Res<renderer::FramePacket> packet,
                   ResMut<SimpleRenderState> simple);

} // namespace helios

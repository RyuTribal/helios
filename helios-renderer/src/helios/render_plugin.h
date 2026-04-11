#pragma once

#include "helios/ecs/system_params.h"
#include "helios/render_settings.h"
#include "helios/rhi/rhi_factory.h"
#include "helios/rhi/rhi_types.h"
#include "helios/window/windows.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace helios {

class App;

enum class RenderMode : uint8_t {
    Direct,    // Blit scene framebuffer to swapchain automatically
    Offscreen, // Leave scene framebuffer for external systems to query
};

/// Core rendering infrastructure plugin.
struct RenderPlugin {
    rhi::Backend backend = rhi::Backend::Vulkan;
    uint32_t gpu_index = UINT32_MAX;
    std::string app_name = "Helios";
    bool enable_validation = true;
    RenderMode mode = RenderMode::Direct;
    rhi::PresentMode initial_present_mode = rhi::PresentMode::Fifo;

    void build(App& app);
};

/// GPU rendering context. All resources are valid after construction.
/// No null checks needed — if RenderContext exists, everything works.
struct RenderContext {
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = rhi::Device::MAX_FRAMES_IN_FLIGHT;

    std::unique_ptr<rhi::Device> device;
    std::unique_ptr<rhi::Swapchain> swapchain;
    std::unique_ptr<rhi::CommandBuffer> cmds[MAX_FRAMES_IN_FLIGHT];
    rhi::CommandBuffer* cmd = nullptr;

    // Per-frame scene framebuffers — triple-buffered so overlapping
    // frames don't race on the same texture.
    struct SceneFramebuffer {
        std::unique_ptr<rhi::Texture> color;
        std::unique_ptr<rhi::Texture> depth;
    };
    SceneFramebuffer scene_fbs[MAX_FRAMES_IN_FLIGHT];
    uint32_t scene_width = 0;
    uint32_t scene_height = 0;
    uint32_t current_frame_index = 0;

    // Convenience: current frame's scene color/depth
    rhi::Texture* scene_color = nullptr;
    rhi::Texture* scene_depth = nullptr;

    RenderMode mode = RenderMode::Direct;
    bool frame_active = false;
    bool scene_pass_active = false;

    /// Per-camera render targets. Key = arbitrary ID (non-zero).
    /// Cameras with target_color == nullptr use scene_color/scene_depth.
    struct CameraTarget {
        std::unique_ptr<rhi::Texture> color;
        std::unique_ptr<rhi::Texture> depth;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::unordered_map<uint32_t, CameraTarget> camera_targets;

    /// Get or create a camera render target by ID. Returns color/depth pointers.
    /// Resizes if the requested size differs from the existing allocation.
    std::pair<rhi::Texture*, rhi::Texture*> get_or_create_target(
        uint32_t id, uint32_t width, uint32_t height);

    /// Recreate swapchain + scene framebuffer. One method, one place.
    void resize(uint32_t fb_width, uint32_t fb_height, rhi::PresentMode present_mode, float resolution_scale = 1.0f);

    /// Create scene framebuffer at the given size if it differs from current.
    void resize_scene_fb(uint32_t width, uint32_t height);
};

void frame_begin(ResMut<RenderContext> ctx, ResMut<RenderSettings> settings, Res<Windows> windows);
void frame_end(ResMut<RenderContext> ctx);
void frame_present(ResMut<RenderContext> ctx);

} // namespace helios

#pragma once

#include "helios/rhi/rhi_types.h"  // for PresentMode, HELIOS_ENUM_FLAGS
#include <glm/glm.hpp>
#include <cstdint>

namespace helios {

// ---------------------------------------------------------------------------
//  Dirty flags — which subsystems need to react to settings changes
// ---------------------------------------------------------------------------

enum class RenderDirty : uint32_t {
    None      = 0,
    Swapchain = 1 << 0,  // present_mode changed -> recreate swapchain
    Quality   = 1 << 1,  // shadow/LOD settings -> lazy realloc
};
HELIOS_ENUM_FLAGS(RenderDirty)

// ---------------------------------------------------------------------------
//  Debug visualization flags
// ---------------------------------------------------------------------------

enum class DebugDraw : uint32_t {
    None          = 0,
    Wireframe     = 1 << 0,
    Colliders     = 1 << 1,
    LightBounds   = 1 << 2,
    Normals       = 1 << 3,
    BoundingBoxes = 1 << 4,
};
HELIOS_ENUM_FLAGS(DebugDraw)

// ---------------------------------------------------------------------------
//  Tonemap operator selection
// ---------------------------------------------------------------------------

enum class TonemapMode : uint8_t { None, Reinhard, ACES, Filmic };

// ---------------------------------------------------------------------------
//  RenderSettings — centralized runtime-changeable render settings
//  Systems write to it; frame_begin reads it to apply changes.
// ---------------------------------------------------------------------------

struct RenderSettings {
    // -- Swapchain --------------------------------------------------------
    rhi::PresentMode present_mode = rhi::PresentMode::Fifo;

    // -- Scene framebuffer ------------------------------------------------
    float resolution_scale = 1.0f;  // 0.25 - 2.0

    // -- Per-frame --------------------------------------------------------
    glm::vec4 clear_color       = {0.1f, 0.1f, 0.1f, 1.0f};
    float     exposure          = 1.0f;
    glm::vec3 ambient_color     = {1.0f, 1.0f, 1.0f};
    float     ambient_intensity = 0.3f;
    TonemapMode tonemap         = TonemapMode::ACES;

    // -- Quality ----------------------------------------------------------
    uint32_t shadow_resolution    = 4096;
    uint32_t shadow_cascades      = 4;
    float    lod_bias             = 1.0f;
    uint32_t max_point_lights     = 1024;
    uint32_t max_dir_lights       = 4;
    uint32_t anisotropic_filtering = 16;

    // -- Debug ------------------------------------------------------------
    DebugDraw debug_draw = DebugDraw::None;

    // -- Dirty flags ------------------------------------------------------
    RenderDirty dirty = RenderDirty::None;

    // -- Convenience methods ----------------------------------------------

    void set_vsync(bool on) {
        present_mode = on ? rhi::PresentMode::Fifo : rhi::PresentMode::Immediate;
        dirty |= RenderDirty::Swapchain;
    }

    bool vsync() const {
        return present_mode != rhi::PresentMode::Immediate;
    }

    void set_present_mode(rhi::PresentMode mode) {
        present_mode = mode;
        dirty |= RenderDirty::Swapchain;
    }

    void set_shadow_resolution(uint32_t res) {
        shadow_resolution = res;
        dirty |= RenderDirty::Quality;
    }

    void toggle_debug(DebugDraw flag) {
        debug_draw = static_cast<DebugDraw>(
            static_cast<uint32_t>(debug_draw) ^ static_cast<uint32_t>(flag));
    }

    bool has_debug(DebugDraw flag) const {
        return has_flag(debug_draw, flag);
    }
};

} // namespace helios

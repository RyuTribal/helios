// helios-renderer/src/helios/forward_plus/gpu_data.h
//
// Shader-matching struct layouts for GPU upload (UBO, SSBO, push constants).
// These structs must match the std140/std430 layouts in the GLSL shaders.
// Do NOT reorder or resize fields without updating the corresponding shaders.
#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace helios {

// -------------------------------------------------------------------------
// GlobalUBO -- set 0, binding 0 in default_static_shader.vert
// -------------------------------------------------------------------------
struct alignas(16) GlobalUBOData {
    glm::mat4 camera_view;              // offset 0
    glm::mat4 camera_projection;        // offset 64
    glm::vec3 camera_pos;               // offset 128
    float     camera_far_plane;         // offset 140
    int       num_directional_lights;   // offset 144
    int       num_tiles_x;              // offset 148
    float     environment_brightness;   // offset 152
    float     _padding;                 // offset 156
};
static_assert(sizeof(GlobalUBOData) == 160, "GlobalUBOData must be 160 bytes to match std140 layout");

// -------------------------------------------------------------------------
// Push constants -- depth_pre_pass.vert, default_static_shader.vert
// -------------------------------------------------------------------------
struct PushConstantData {
    glm::mat4 transform;
};

// -------------------------------------------------------------------------
// PointLightInfo -- light_culling_shader.comp, default_static_shader.frag
// -------------------------------------------------------------------------
struct alignas(16) PointLightGPU {
    float     constant_attenuation;     // offset 0
    float     linear_attenuation;       // offset 4
    float     quadratic_attenuation;    // offset 8
    float     intensity;                // offset 12
    glm::vec4 color;                    // offset 16
    glm::vec4 position;                 // offset 32
};

// -------------------------------------------------------------------------
// DirectionalLightInfo -- default_static_shader.frag
// -------------------------------------------------------------------------
struct alignas(16) DirLightGPU {
    glm::vec3 _padding;                // offset 0
    float     intensity;                // offset 12
    glm::vec4 color;                    // offset 16
    glm::vec4 direction;                // offset 32
};

// -------------------------------------------------------------------------
// VisibleIndex -- light_culling_shader.comp (SSBO output)
// -------------------------------------------------------------------------
struct VisibleIndex {
    int index;
};

// -------------------------------------------------------------------------
// LightCullingParams -- set 0, binding 0 in light_culling_shader.comp
// -------------------------------------------------------------------------
struct alignas(16) LightCullingParams {
    glm::mat4  view;                    // offset 0
    glm::mat4  projection;              // offset 64
    glm::ivec2 screen_size;             // offset 128
    int        light_count;             // offset 136
    int        _pad;                    // offset 140
};

// -------------------------------------------------------------------------
// CameraUBO -- set 0, binding 0 in depth_pre_pass.vert
// -------------------------------------------------------------------------
struct CameraUBOData {
    glm::mat4 view;
    glm::mat4 projection;
};

// -------------------------------------------------------------------------
// SkyboxUBO -- set 0, binding 0 in skybox.vert
// -------------------------------------------------------------------------
struct SkyboxUBOData {
    glm::mat4 camera_view;
    glm::mat4 camera_projection;
    float     brightness;
};

// -------------------------------------------------------------------------
// MaterialUBO -- set 1, binding 0 in default_static_shader.frag
// -------------------------------------------------------------------------
struct alignas(16) MaterialGPU {
    glm::vec3 albedo_color;             // offset 0
    float     metalness;                // offset 12
    float     roughness;                // offset 16
    float     emission;                 // offset 20
    int       use_normal_map;           // offset 24
    float     _padding;                 // offset 28
};

// -------------------------------------------------------------------------
// Shadow cascade push constants
// -------------------------------------------------------------------------
struct ShadowPushConstant {
    glm::mat4 transform;
};

// -------------------------------------------------------------------------
// Prefilter environment map push constants
// -------------------------------------------------------------------------
struct PrefilterPushConstants {
    float    roughness;
    uint32_t mip_size;
};

// -------------------------------------------------------------------------
// Tonemap push constants
// -------------------------------------------------------------------------
struct TonemapPushConstants {
    float exposure;
};

// -------------------------------------------------------------------------
// Pipeline-wide constants
// -------------------------------------------------------------------------
constexpr uint32_t MAX_POINT_LIGHTS            = 1024;
constexpr uint32_t MAX_DIR_LIGHTS              = 4;
constexpr uint32_t MAX_CASCADE_MATRICES        = 16;
constexpr uint32_t TILE_SIZE                   = 16;
constexpr uint32_t MAX_VISIBLE_LIGHTS_PER_TILE = 1024;

} // namespace helios

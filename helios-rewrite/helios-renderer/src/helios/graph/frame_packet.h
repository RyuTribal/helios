// helios-rewrite/helios-renderer/src/helios/graph/frame_packet.h
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdint>

namespace helios::renderer {

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

struct CameraData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};
    float near_plane = 0.1f;
    float far_plane = 500.0f;
    float fov_y = 45.0f;             // vertical FOV in degrees (for cascade splits, light culling)
    float aspect_ratio = 16.0f / 9.0f; // width / height
};

// ---------------------------------------------------------------------------
// Meshes
// ---------------------------------------------------------------------------

struct MeshDraw {
    glm::mat4 transform{1.0f};
    uint64_t mesh = 0;         // AssetHandle::packed()
    uint64_t material = 0;     // AssetHandle::packed()
};

// ---------------------------------------------------------------------------
// Lights
// ---------------------------------------------------------------------------

struct PointLightData {
    glm::vec3 position{0.0f};
    float _pad0 = 0.0f;       // align to 16 bytes for GPU upload
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
    float _pad1[3] = {};       // pad to 48 bytes
};

struct DirLightData {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float _pad0 = 0.0f;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    bool cast_shadows = true;
    uint8_t _pad1[15] = {};    // pad to 48 bytes
};

struct SpotLightData {
    glm::vec3 position{0.0f};
    float _pad0 = 0.0f;
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float intensity = 1.0f;
    glm::vec3 color{1.0f};
    float radius = 10.0f;
    float inner_cone = 0.9f;   // cos(angle)
    float outer_cone = 0.8f;   // cos(angle)
    float _pad1[2] = {};
};

// ---------------------------------------------------------------------------
// Skybox
// ---------------------------------------------------------------------------

struct SkyboxData {
    uint64_t cubemap_texture = 0;     // AssetHandle::id for the cubemap
    uint64_t irradiance_map = 0;      // AssetHandle::id for IBL irradiance
    uint64_t prefilter_map = 0;       // AssetHandle::id for IBL prefilter
    uint64_t brdf_lut = 0;           // AssetHandle::id for BRDF LUT
    float intensity = 1.0f;
    float rotation = 0.0f;           // Y-axis rotation in radians
};

// ---------------------------------------------------------------------------
// FramePacket -- the complete snapshot of one frame's render data.
//
// Built on the main thread by extract_render_data().
// Moved to the render thread via RenderThread::submit().
// No World pointers, no entity handles, no ECS references.
// ---------------------------------------------------------------------------

struct FramePacket {
    CameraData camera;

    std::vector<MeshDraw> mesh_draws;
    std::vector<PointLightData> point_lights;
    std::vector<DirLightData> dir_lights;
    std::vector<SpotLightData> spot_lights;

    SkyboxData skybox;

    // Frame metadata
    uint64_t frame_number = 0;
    float time_elapsed = 0.0f;
    float delta_time = 0.0f;

    // Viewport dimensions (for render target sizing)
    uint32_t viewport_width = 1;
    uint32_t viewport_height = 1;

    void clear() {
        camera = CameraData{};
        mesh_draws.clear();
        point_lights.clear();
        dir_lights.clear();
        spot_lights.clear();
        skybox = SkyboxData{};
        frame_number = 0;
        time_elapsed = 0.0f;
        delta_time = 0.0f;
    }
};

} // namespace helios::renderer

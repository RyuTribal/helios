// helios-renderer/src/helios/forward_plus/extract_render_data.cpp
#include "helios/forward_plus/extract_render_data.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/rhi/rhi_swapchain.h"

#include <algorithm>

namespace helios {

void extract_render_data(
    Query<const Transform, const MeshRenderer, Without<Disabled>> meshes,
    Query<const Transform, const PointLight>                      point_lights,
    Query<const Transform, const DirectionalLight>                dir_lights,
    Query<const Transform, const Camera, With<ActiveCamera>>      cameras,
    Res<RenderContext>                                             render_ctx,
    ResMut<renderer::FramePacket>                                 packet)
{
    packet->clear();

    // Full swapchain dimensions (used for default aspect if viewport is full-window)
    const float sw_w = (render_ctx->swapchain)
        ? static_cast<float>(render_ctx->swapchain->width()) : 1920.0f;
    const float sw_h = (render_ctx->swapchain)
        ? static_cast<float>(render_ctx->swapchain->height()) : 1080.0f;

    // --- Active cameras (multi-camera) ---
    // Collect all cameras with ActiveCamera tag, then sort by order.
    struct CamEntry {
        Transform transform;
        Camera cam;
    };
    std::vector<CamEntry> cam_entries;
    for (auto [t, cam] : cameras) {
        cam_entries.push_back({t, cam});
    }

    // Sort by render order (lower first)
    std::sort(cam_entries.begin(), cam_entries.end(),
              [](const CamEntry& a, const CamEntry& b) {
                  return a.cam.order < b.cam.order;
              });

    packet->camera_views.reserve(cam_entries.size());
    for (const auto& entry : cam_entries) {
        const auto& t = entry.transform;
        const auto& cam = entry.cam;

        // Aspect ratio comes from the camera's viewport, not the full window
        const float vp_pixel_w = cam.viewport_w * sw_w;
        const float vp_pixel_h = cam.viewport_h * sw_h;
        const float aspect = (vp_pixel_h > 0.0f) ? (vp_pixel_w / vp_pixel_h) : (16.0f / 9.0f);

        renderer::CameraView cv;
        cv.camera = renderer::CameraData{
            .view         = glm::inverse(t.to_mat4()),
            .projection   = (cam.projection == ProjectionType::Perspective)
                ? glm::perspective(
                      glm::radians(cam.fov_degrees),
                      aspect,
                      cam.near_plane,
                      cam.far_plane)
                : glm::ortho(
                      -cam.ortho_size, cam.ortho_size,
                      -cam.ortho_size, cam.ortho_size,
                      cam.near_plane, cam.far_plane),
            .position     = t.position,
            .near_plane   = cam.near_plane,
            .far_plane    = cam.far_plane,
            .fov_y        = cam.fov_degrees,
            .aspect_ratio = aspect,
        };
        cv.viewport_x = cam.viewport_x;
        cv.viewport_y = cam.viewport_y;
        cv.viewport_w = cam.viewport_w;
        cv.viewport_h = cam.viewport_h;
        cv.clear_mode = (cam.clear_mode == Camera::ClearMode::SolidColor)
            ? renderer::CameraClearMode::SolidColor
            : renderer::CameraClearMode::None;

        packet->camera_views.push_back(cv);
    }

    // Backward compat: primary camera is the first in sorted order
    if (!packet->camera_views.empty()) {
        packet->camera = packet->camera_views.front().camera;
    } else {
        HELIOS_LOG_WARN(ForwardPlus, "No active camera found; frame will use default camera");
    }

    // --- Mesh draws ---
    packet->mesh_draws.reserve(meshes.count());
    for (auto [t, mr] : meshes) {
        packet->mesh_draws.push_back(renderer::MeshDraw{
            .transform = t.to_mat4(),
            .mesh      = mr.mesh.untyped().packed(),
            .material  = mr.material.untyped().packed(),
        });
    }

    // --- Point lights ---
    packet->point_lights.reserve(point_lights.count());
    for (auto [t, pl] : point_lights) {
        packet->point_lights.push_back(renderer::PointLightData{
            .position  = t.position,
            .color     = pl.color,
            .intensity = pl.intensity,
            .radius    = pl.radius,
        });
    }

    // --- Directional lights ---
    for (auto [t, dl] : dir_lights) {
        // Direction derived from the transform's forward vector (negative Z).
        glm::vec3 forward = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        packet->dir_lights.push_back(renderer::DirLightData{
            .direction    = forward,
            .color        = dl.color,
            .intensity    = dl.intensity,
            .cast_shadows = true,
        });
    }

    HELIOS_LOG_TRACE(ForwardPlus,
        "Extracted {} meshes, {} point lights, {} dir lights",
        packet->mesh_draws.size(),
        packet->point_lights.size(),
        packet->dir_lights.size());
}

} // namespace helios

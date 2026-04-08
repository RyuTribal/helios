// helios-renderer/src/helios/forward_plus/extract_render_data.cpp
#include "helios/forward_plus/extract_render_data.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/rhi/rhi_swapchain.h"

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

    // Compute aspect ratio from actual swapchain dimensions
    const float aspect = (render_ctx->swapchain && render_ctx->swapchain->height() > 0)
        ? static_cast<float>(render_ctx->swapchain->width()) / static_cast<float>(render_ctx->swapchain->height())
        : 16.0f / 9.0f;

    // --- Active camera ---
    // Only one active camera is expected; take the first match.
    bool camera_found = false;
    for (auto [t, cam] : cameras) {
        packet->camera = renderer::CameraData{
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
        camera_found = true;
        break;
    }

    if (!camera_found) {
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

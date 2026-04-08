// helios-renderer/src/helios/forward_plus/extract_render_data.h
#pragma once

#include "helios/ecs/query.h"
#include "helios/ecs/query_filters.h"
#include "helios/ecs/system_params.h"
#include "helios/components/components.h"
#include "helios/graph/frame_packet.h"
#include "helios/render_plugin.h"

namespace helios {

/// ECS system: queries Transform+MeshRenderer, Transform+PointLight,
/// Transform+DirectionalLight, Transform+Camera+ActiveCamera and populates
/// the FramePacket resource.
///
/// Uses swapchain dimensions from RenderContext for correct aspect ratio.
///
/// Runs on the main thread during Schedule::PreRender.
void extract_render_data(
    Query<const Transform, const MeshRenderer, Without<Disabled>> meshes,
    Query<const Transform, const PointLight>                      point_lights,
    Query<const Transform, const DirectionalLight>                dir_lights,
    Query<const Transform, const Camera, With<ActiveCamera>>      cameras,
    Res<RenderContext>                                             render_ctx,
    ResMut<renderer::FramePacket>                                 packet);

} // namespace helios

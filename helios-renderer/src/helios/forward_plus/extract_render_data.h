#pragma once

#include "helios/ecs/query.h"
#include "helios/ecs/query_filters.h"
#include "helios/ecs/system_params.h"
#include "helios/components/components.h"
#include "helios/forward_plus/gpu_cache.h"
#include "helios/graph/frame_packet.h"
#include "helios/render_plugin.h"
#include "helios/render_schedule.h"
#include "helios/assets/asset_server.h"

#include <memory>

namespace helios {

/// ECS system: queries Transform+MeshRenderer, Transform+PointLight,
/// Transform+DirectionalLight, Transform+Camera+ActiveCamera and populates
/// the FramePacket resource.
///
/// Uses swapchain dimensions from RenderContext for correct aspect ratio.
/// Resolves each camera's render_schedule string to a label via the registry.
/// Evicts unused GPU resources from the cache once per frame.
///
/// Runs on the main thread during Schedule::PreRender.
void extract_render_data(
    Query<const Transform, const MeshRenderer, Optional<const RenderLayers>, Without<Disabled>> meshes,
    Query<const Transform, const PointLight>                                                     point_lights,
    Query<const Transform, const DirectionalLight>                                               dir_lights,
    Query<const Transform, const Camera, Optional<const RenderLayers>, With<ActiveCamera>>       cameras,
    Res<RenderContext>                                                                            render_ctx,
    Res<RenderScheduleRegistry>                                                                  registry,
    ResMut<renderer::FramePacket>                                                                packet,
    ResMut<GPUResourceCache>                                                                     gpu_cache,
    Res<std::shared_ptr<AssetServer>>                                                            asset_server);

} // namespace helios

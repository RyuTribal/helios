#pragma once

#include "helios/graph/frame_packet.h"

#include <cstdint>

namespace helios {

class World;

/// Draw step: renders skybox + PBR meshes for a single camera view.
/// Called by camera_driver via CameraRenderSchedules for each camera
/// using the "forward_plus" render schedule.
void forward_plus_draw_view(World& world,
                            const renderer::CameraView& view,
                            uint32_t camera_index);

} // namespace helios

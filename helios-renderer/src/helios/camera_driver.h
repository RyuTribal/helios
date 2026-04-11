#pragma once

#include "helios/ecs/system_params.h"
#include "helios/camera_render_schedule.h"
#include "helios/render_schedule.h"
#include "helios/render_plugin.h"
#include "helios/render_settings.h"
#include "helios/graph/frame_packet.h"

namespace helios {

void camera_driver(
    World& world,
    ResMut<RenderContext> ctx,
    Res<renderer::FramePacket> packet,
    ResMut<CameraRenderSchedules> schedules,
    Res<RenderScheduleRegistry> registry,
    Res<RenderSettings> settings);

} // namespace helios

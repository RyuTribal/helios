// helios-renderer/src/helios/forward_plus/forward_plus_draw.h
//
// Draw system for Forward+ pipeline. Renders skybox and PBR meshes
// between frame_begin and frame_end.
#pragma once

#include "helios/ecs/system_params.h"
#include "helios/forward_plus/pbr_render_state.h"
#include "helios/forward_plus/skybox_state.h"
#include "helios/graph/frame_packet.h"

namespace helios {

struct RenderContext; // forward declaration

/// ECS system: renders skybox + PBR meshes using the current command buffer.
/// Must run between frame_begin and frame_end (ordering enforced by plugin).
void forward_plus_draw(ResMut<RenderContext> ctx,
                       Res<renderer::FramePacket> packet,
                       ResMut<SkyboxState> skybox,
                       ResMut<PBRRenderState> pbr);

} // namespace helios

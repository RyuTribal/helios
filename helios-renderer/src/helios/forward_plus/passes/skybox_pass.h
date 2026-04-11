#pragma once

#include "helios/graph/render_graph.h"
#include "helios/graph/frame_packet.h"

namespace helios {

/// Adds the skybox pass to the render graph.
/// Reads/writes the HDR color texture (renders behind all geometry).
/// Returns the same HDR color handle (modified in place).
graph::TextureHandle add_skybox_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle hdr_color,
    const renderer::FramePacket& packet);

} // namespace helios

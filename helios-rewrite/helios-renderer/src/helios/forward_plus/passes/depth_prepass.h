// helios-renderer/src/helios/forward_plus/passes/depth_prepass.h
//
// Depth prepass render graph node. Creates a transient Depth32F texture and
// draws all meshes depth-only for early-z and downstream consumers (light
// culling, forward pass depth test).
#pragma once

#include "helios/graph/render_graph.h"
#include "helios/graph/frame_packet.h"
#include "helios/forward_plus/forward_plus_config.h"

namespace helios {

struct DepthPrepassOutput {
    graph::TextureHandle depth;
};

/// Adds the depth prepass to the render graph.
/// Returns the handle to the depth texture produced by this pass.
DepthPrepassOutput add_depth_prepass(
    graph::RenderGraph& graph,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config);

} // namespace helios

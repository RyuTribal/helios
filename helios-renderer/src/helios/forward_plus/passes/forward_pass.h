#pragma once

#include "helios/graph/render_graph.h"
#include "helios/graph/frame_packet.h"
#include "helios/forward_plus/forward_plus_config.h"
#include "helios/forward_plus/passes/shadow_pass.h"
#include "helios/forward_plus/passes/light_culling_pass.h"

namespace helios {

struct ForwardPassOutput {
    graph::TextureHandle color;
};

/// Adds the forward PBR pass to the render graph.
/// Reads: depth texture, shadow map, light culling buffers.
/// Writes: HDR color texture (RGBA16F).
/// Returns: the HDR color output handle.
ForwardPassOutput add_forward_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle depth_texture,
    const ShadowPassOutput& shadows,
    const LightCullingOutput& light_cull,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config);

} // namespace helios

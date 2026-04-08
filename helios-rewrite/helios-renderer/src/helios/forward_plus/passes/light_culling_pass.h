// helios-renderer/src/helios/forward_plus/passes/light_culling_pass.h
//
// Tile-based Forward+ light culling compute pass. Reads the depth buffer and
// point/directional light data, writes per-tile visible light index lists.
#pragma once

#include "helios/graph/render_graph.h"
#include "helios/graph/frame_packet.h"
#include "helios/forward_plus/forward_plus_config.h"

namespace helios {

struct LightCullingOutput {
    graph::BufferHandle light_ssbo;
    graph::BufferHandle dir_light_ssbo;
    graph::BufferHandle visible_indices_ssbo;
};

/// Adds the tile-based light culling compute pass to the render graph.
/// Reads the depth texture from the depth prepass.
/// Writes per-tile visible light index lists.
LightCullingOutput add_light_culling_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle depth_texture,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config);

} // namespace helios

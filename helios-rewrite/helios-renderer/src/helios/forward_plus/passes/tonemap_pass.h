// helios-renderer/src/helios/forward_plus/passes/tonemap_pass.h
//
// Tonemap compute pass. Reads HDR (RGBA16F) and writes LDR (RGBA8) output,
// applying exposure-based tone mapping.
#pragma once

#include "helios/graph/render_graph.h"
#include "helios/graph/frame_packet.h"
#include "helios/forward_plus/forward_plus_config.h"

namespace helios {

/// Adds the tonemap compute pass to the render graph.
/// Reads the HDR color texture, writes an LDR (RGBA8) output.
/// Uses exposure value from ForwardPlusConfig.
graph::TextureHandle add_tonemap_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle hdr_input,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config);

} // namespace helios

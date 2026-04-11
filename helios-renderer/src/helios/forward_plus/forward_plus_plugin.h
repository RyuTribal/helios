#pragma once

#include "helios/forward_plus/forward_plus_config.h"
#include "helios/ecs/system_params.h"
#include "helios/graph/frame_packet.h"
#include "helios/graph/render_graph.h"

namespace helios {

class App; // forward declaration

/// ForwardPlus rendering plugin.
///
/// Registers the Forward+ pipeline configuration, extraction system, and
/// the graph-build system with the App scheduler.
///
/// Usage:
///   app.add_plugin(ForwardPlusPlugin{});           // default config
///   app.add_plugin(ForwardPlusPlugin{.config = {   // custom config
///       .shadow_resolution = 2048,
///       .exposure = 1.5f,
///   }});
struct ForwardPlusPlugin {
    ForwardPlusConfig config{};

    void build(App& app);
};

/// ECS system: wires all 6 Forward+ render passes into the render graph.
/// Runs during PreRender, after extract_render_data.
void build_forward_plus_graph(
    Res<renderer::FramePacket> packet,
    Res<ForwardPlusConfig> config,
    ResMut<graph::RenderGraph> graph);

} // namespace helios

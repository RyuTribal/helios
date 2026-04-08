// helios-renderer/src/helios/forward_plus/forward_plus_plugin.h
#pragma once

#include "helios/forward_plus/forward_plus_config.h"

namespace helios {

class App; // forward declaration

/// ForwardPlus rendering plugin.
///
/// Registers the Forward+ pipeline configuration, extraction system, and
/// (eventually) the graph-build system with the App scheduler.
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

} // namespace helios

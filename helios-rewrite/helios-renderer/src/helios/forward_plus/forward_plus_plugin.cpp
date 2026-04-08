// helios-renderer/src/helios/forward_plus/forward_plus_plugin.cpp
#include "helios/forward_plus/forward_plus_plugin.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/extract_render_data.h"
#include "helios/ecs/app.h"
#include "helios/graph/frame_packet.h"

namespace helios {

void ForwardPlusPlugin::build(App& app) {
    HELIOS_LOG_INFO(ForwardPlus, "Initializing ForwardPlus plugin");

    // Insert the pipeline configuration as a world resource.
    app.insert_resource(config);

    // Insert an empty FramePacket so extract_render_data can write into it.
    app.insert_resource(renderer::FramePacket{});

    // Register the extraction system (main thread, runs during PreRender).
    auto extract_id = app.add_system(Schedule::PreRender, extract_render_data,
                                     "extract_render_data").id();

    // build_forward_plus_graph will be registered here once implemented,
    // with .after(extract_id) to ensure extraction completes first.
    (void)extract_id;

    HELIOS_LOG_INFO(ForwardPlus, "ForwardPlus plugin registered");
}

} // namespace helios

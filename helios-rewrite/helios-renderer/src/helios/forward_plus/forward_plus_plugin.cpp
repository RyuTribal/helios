// helios-renderer/src/helios/forward_plus/forward_plus_plugin.cpp
#include "helios/forward_plus/forward_plus_plugin.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/extract_render_data.h"
#include "helios/forward_plus/forward_plus_draw.h"
#include "helios/forward_plus/pbr_render_state.h"
#include "helios/forward_plus/skybox_state.h"
#include "helios/forward_plus/passes/depth_prepass.h"
#include "helios/forward_plus/passes/shadow_pass.h"
#include "helios/forward_plus/passes/light_culling_pass.h"
#include "helios/forward_plus/passes/forward_pass.h"
#include "helios/forward_plus/passes/skybox_pass.h"
#include "helios/forward_plus/passes/tonemap_pass.h"
#include "helios/forward_plus/pipeline_init.h"
#include "helios/render_plugin.h"
#include "helios/ecs/app.h"
#include "helios/graph/frame_packet.h"
#include "helios/graph/render_graph.h"

namespace helios {

// ---------------------------------------------------------------------------
// build_forward_plus_graph -- wires all 6 passes into the render graph.
//
// Pass ordering:
//   DepthPrepass --> LightCulling --> ForwardPass --> SkyboxPass --> TonemapPass
//   ShadowPass  --^                  ^
//                                    |
//   (shadow map read by forward) ----+
// ---------------------------------------------------------------------------

void build_forward_plus_graph(
    Res<renderer::FramePacket> packet,
    Res<ForwardPlusConfig> config,
    ResMut<graph::RenderGraph> graph)
{
    // Clear the graph from the previous frame.
    graph->clear();

    // 1. Depth prepass -- writes a depth texture at viewport resolution.
    auto depth = add_depth_prepass(*graph, *packet, *config);

    // 2. Shadow pass -- cascade shadow maps from the first shadow-casting dir light.
    auto shadows = add_shadow_pass(*graph, *packet, *config);

    // 3. Light culling -- tile-based compute that reads the depth buffer.
    auto light_cull = add_light_culling_pass(
        *graph, depth.depth, *packet, *config);

    // 4. Forward PBR pass -- reads depth, shadow map, light buffers.
    auto hdr = add_forward_pass(
        *graph, depth.depth, shadows, light_cull, *packet, *config);

    // 5. Skybox pass -- renders behind all geometry into the HDR target.
    auto skybox = add_skybox_pass(*graph, hdr.color, *packet);

    // 6. Tonemap pass -- HDR -> LDR compute dispatch.
    auto ldr = add_tonemap_pass(*graph, skybox, *packet, *config);

    // Set final output: the render graph will cull any passes
    // that don't contribute to this output.
    graph->set_output(ldr);
}

// ---------------------------------------------------------------------------
// ForwardPlusPlugin::build
// ---------------------------------------------------------------------------

void ForwardPlusPlugin::build(App& app) {
    HELIOS_LOG_INFO(ForwardPlus, "Initializing ForwardPlus plugin");

    // Insert the pipeline configuration as a world resource.
    app.insert_resource(config);

    // Insert an empty FramePacket so extract_render_data can write into it.
    app.insert_resource(renderer::FramePacket{});

    // Insert an empty RenderGraph resource.
    app.insert_resource(graph::RenderGraph{});

    // Insert default empty PBRRenderState and SkyboxState as fallbacks;
    // the sandbox (or any consuming app) will overwrite with populated versions.
    if (!app.world().has_resource<PBRRenderState>()) {
        app.insert_resource(PBRRenderState{});
    }
    if (!app.world().has_resource<SkyboxState>()) {
        app.insert_resource(SkyboxState{});
    }

    // Look up the frame_begin and frame_end SystemIds registered by RenderPlugin
    // so we can insert systems between them.
    auto begin_id = app.id_of(frame_begin);
    auto end_id = app.id_of(frame_end);

    // Register the extraction system (main thread, runs during PreRender).
    // Must run after frame_begin (needs RenderContext) and before frame_end.
    auto extract_builder = app.add_system(Schedule::PreRender, extract_render_data,
                                          "extract_render_data");
    if (begin_id.value != 0)
        extract_builder.after(begin_id);
    if (end_id.value != 0)
        extract_builder.before(end_id);
    auto extract_id = extract_builder.id();

    // Register the graph builder after extraction completes, before frame_end.
    auto graph_builder = app.add_system(Schedule::PreRender, build_forward_plus_graph,
                                        "build_forward_plus_graph");
    graph_builder.after(extract_id);
    if (end_id.value != 0)
        graph_builder.before(end_id);

    // Register the draw system between frame_begin and frame_end.
    // Runs after extraction + graph build so FramePacket is populated.
    auto draw_builder = app.add_system(Schedule::PreRender, forward_plus_draw,
                                       "forward_plus_draw");
    draw_builder.after(extract_id);
    if (end_id.value != 0)
        draw_builder.before(end_id);

    HELIOS_LOG_INFO(ForwardPlus, "ForwardPlus plugin registered");
}

} // namespace helios

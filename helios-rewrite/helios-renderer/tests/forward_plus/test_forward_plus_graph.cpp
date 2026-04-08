// helios-renderer/tests/forward_plus/test_forward_plus_graph.cpp
//
// Tests for the Forward+ graph builder system and pass wiring.
// No GPU required -- uses compile_only() which only performs the
// backward liveness walk, topological sort, and barrier computation.

#include <gtest/gtest.h>

#include "helios/graph/render_graph.h"
#include "helios/graph/frame_packet.h"
#include "helios/forward_plus/forward_plus_config.h"
#include "helios/forward_plus/passes/depth_prepass.h"
#include "helios/forward_plus/passes/shadow_pass.h"
#include "helios/forward_plus/passes/light_culling_pass.h"
#include "helios/forward_plus/passes/forward_pass.h"
#include "helios/forward_plus/passes/skybox_pass.h"
#include "helios/forward_plus/passes/tonemap_pass.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

using namespace helios;
using namespace helios::graph;
using namespace helios::renderer;

// ---------------------------------------------------------------------------
// Helper: find the position of a pass name in the execution order
// ---------------------------------------------------------------------------

static int order_of(const RenderGraph& graph, const std::string& name) {
    const auto& order = graph.execution_order();
    const auto& passes = graph.passes();
    for (int i = 0; i < static_cast<int>(order.size()); i++) {
        if (passes[order[i]].name == name) return i;
    }
    return -1; // not found (culled or missing)
}

static bool is_culled(const RenderGraph& graph, const std::string& name) {
    for (const auto& p : graph.passes()) {
        if (p.name == name) return p.culled;
    }
    return true; // not found counts as culled
}

// ---------------------------------------------------------------------------
// Helper: build a standard FramePacket for testing
// ---------------------------------------------------------------------------

static FramePacket make_test_packet() {
    FramePacket packet;
    packet.viewport_width  = 1280;
    packet.viewport_height = 720;
    packet.camera = CameraData{
        .view       = glm::lookAt(glm::vec3(0, 0, 5),
                                  glm::vec3(0, 0, 0),
                                  glm::vec3(0, 1, 0)),
        .projection = glm::perspective(
                          glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 500.0f),
        .position     = {0.0f, 0.0f, 5.0f},
        .near_plane   = 0.1f,
        .far_plane    = 500.0f,
        .fov_y        = 45.0f,
        .aspect_ratio = 16.0f / 9.0f,
    };
    // Add a shadow-casting directional light
    DirLightData dl;
    dl.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    dl.intensity = 1.0f;
    dl.cast_shadows = true;
    packet.dir_lights.push_back(dl);
    // Add a point light
    PointLightData pl;
    pl.position = glm::vec3(5.0f, 5.0f, 5.0f);
    pl.intensity = 1.0f;
    packet.point_lights.push_back(pl);
    return packet;
}

// ===========================================================================
// Test: full graph build produces all 6 passes in correct order
// ===========================================================================

TEST(ForwardPlusGraph, FullGraphBuild_ProducesAllPasses) {
    RenderGraph graph;
    FramePacket packet = make_test_packet();
    ForwardPlusConfig config;

    // Wire up exactly as build_forward_plus_graph does:
    auto depth      = add_depth_prepass(graph, packet, config);
    auto shadows    = add_shadow_pass(graph, packet, config);
    auto light_cull = add_light_culling_pass(graph, depth.depth, packet, config);
    auto hdr        = add_forward_pass(graph, depth.depth, shadows, light_cull,
                                       packet, config);
    auto skybox     = add_skybox_pass(graph, hdr.color, packet);
    auto ldr        = add_tonemap_pass(graph, skybox, packet, config);

    graph.set_output(ldr);
    graph.compile_only();

    // All 6 passes should be registered
    EXPECT_EQ(graph.pass_count(), 6u);

    // Verify pass names in registration order
    auto names = std::vector<std::string>{};
    for (const auto& p : graph.passes()) {
        names.push_back(p.name);
    }
    ASSERT_EQ(names.size(), 6u);
    EXPECT_EQ(names[0], "DepthPrepass");
    EXPECT_EQ(names[1], "ShadowPass");
    EXPECT_EQ(names[2], "LightCulling");
    EXPECT_EQ(names[3], "ForwardPass");
    EXPECT_EQ(names[4], "SkyboxPass");
    EXPECT_EQ(names[5], "TonemapPass");
}

// ===========================================================================
// Test: no pass is culled when all are connected to the output
// ===========================================================================

TEST(ForwardPlusGraph, FullGraphBuild_NoPassesCulled) {
    RenderGraph graph;
    FramePacket packet = make_test_packet();
    ForwardPlusConfig config;

    auto depth      = add_depth_prepass(graph, packet, config);
    auto shadows    = add_shadow_pass(graph, packet, config);
    auto light_cull = add_light_culling_pass(graph, depth.depth, packet, config);
    auto hdr        = add_forward_pass(graph, depth.depth, shadows, light_cull,
                                       packet, config);
    auto skybox     = add_skybox_pass(graph, hdr.color, packet);
    auto ldr        = add_tonemap_pass(graph, skybox, packet, config);

    graph.set_output(ldr);
    graph.compile_only();

    // No pass should be culled
    EXPECT_FALSE(is_culled(graph, "DepthPrepass"));
    EXPECT_FALSE(is_culled(graph, "ShadowPass"));
    EXPECT_FALSE(is_culled(graph, "LightCulling"));
    EXPECT_FALSE(is_culled(graph, "ForwardPass"));
    EXPECT_FALSE(is_culled(graph, "SkyboxPass"));
    EXPECT_FALSE(is_culled(graph, "TonemapPass"));
}

// ===========================================================================
// Test: execution ordering respects dependencies
// ===========================================================================

TEST(ForwardPlusGraph, FullGraphBuild_ExecutionOrderRespectsDependencies) {
    RenderGraph graph;
    FramePacket packet = make_test_packet();
    ForwardPlusConfig config;

    auto depth      = add_depth_prepass(graph, packet, config);
    auto shadows    = add_shadow_pass(graph, packet, config);
    auto light_cull = add_light_culling_pass(graph, depth.depth, packet, config);
    auto hdr        = add_forward_pass(graph, depth.depth, shadows, light_cull,
                                       packet, config);
    auto skybox     = add_skybox_pass(graph, hdr.color, packet);
    auto ldr        = add_tonemap_pass(graph, skybox, packet, config);

    graph.set_output(ldr);
    graph.compile_only();

    int o_depth   = order_of(graph, "DepthPrepass");
    int o_shadow  = order_of(graph, "ShadowPass");
    int o_cull    = order_of(graph, "LightCulling");
    int o_forward = order_of(graph, "ForwardPass");
    int o_skybox  = order_of(graph, "SkyboxPass");
    int o_tonemap = order_of(graph, "TonemapPass");

    // All passes should be present
    EXPECT_NE(o_depth,   -1);
    EXPECT_NE(o_shadow,  -1);
    EXPECT_NE(o_cull,    -1);
    EXPECT_NE(o_forward, -1);
    EXPECT_NE(o_skybox,  -1);
    EXPECT_NE(o_tonemap, -1);

    // Depth prepass must execute before light culling (depth -> cull reads depth)
    EXPECT_LT(o_depth, o_cull)
        << "DepthPrepass must execute before LightCulling";

    // Light culling must execute before forward pass (cull -> forward reads light indices)
    EXPECT_LT(o_cull, o_forward)
        << "LightCulling must execute before ForwardPass";

    // Shadow pass must execute before forward pass (shadow map read by forward)
    EXPECT_LT(o_shadow, o_forward)
        << "ShadowPass must execute before ForwardPass";

    // Forward pass must execute before skybox (skybox writes into HDR color)
    EXPECT_LT(o_forward, o_skybox)
        << "ForwardPass must execute before SkyboxPass";

    // Skybox must execute before tonemap (tonemap reads HDR)
    EXPECT_LT(o_skybox, o_tonemap)
        << "SkyboxPass must execute before TonemapPass";
}

// ===========================================================================
// Test: graph produces correct resource count
// ===========================================================================

TEST(ForwardPlusGraph, FullGraphBuild_ResourceCount) {
    RenderGraph graph;
    FramePacket packet = make_test_packet();
    ForwardPlusConfig config;

    auto depth      = add_depth_prepass(graph, packet, config);
    auto shadows    = add_shadow_pass(graph, packet, config);
    auto light_cull = add_light_culling_pass(graph, depth.depth, packet, config);
    auto hdr        = add_forward_pass(graph, depth.depth, shadows, light_cull,
                                       packet, config);
    auto skybox     = add_skybox_pass(graph, hdr.color, packet);
    auto ldr        = add_tonemap_pass(graph, skybox, packet, config);

    graph.set_output(ldr);

    // Resources created by the passes:
    //   DepthPrepass: 1 texture (depth)
    //   ShadowPass: 1 texture (shadow map)
    //   LightCulling: 4 buffers (light SSBO, dir light SSBO, visible indices, params UBO)
    //   ForwardPass: 1 texture (HDR color)
    //   SkyboxPass: 0 (reuses HDR color)
    //   TonemapPass: 1 texture (LDR output)
    // Total: 3 textures + 4 buffers = 7 minimum resources,
    // but the tonemap also reads hdr so that's referenced, not new.
    EXPECT_GE(graph.resource_count(), 7u)
        << "Graph should have at least 7 resources (3 textures + 4 buffers)";
}

// ===========================================================================
// Test: output handle is valid
// ===========================================================================

TEST(ForwardPlusGraph, FullGraphBuild_OutputHandleValid) {
    RenderGraph graph;
    FramePacket packet = make_test_packet();
    ForwardPlusConfig config;

    auto depth      = add_depth_prepass(graph, packet, config);
    auto shadows    = add_shadow_pass(graph, packet, config);
    auto light_cull = add_light_culling_pass(graph, depth.depth, packet, config);
    auto hdr        = add_forward_pass(graph, depth.depth, shadows, light_cull,
                                       packet, config);
    auto skybox     = add_skybox_pass(graph, hdr.color, packet);
    auto ldr        = add_tonemap_pass(graph, skybox, packet, config);

    EXPECT_TRUE(ldr.is_valid())
        << "Tonemap output handle should be valid";
}

// ===========================================================================
// Test: graph without shadow-casting light still produces valid structure
// ===========================================================================

TEST(ForwardPlusGraph, GraphBuild_NoShadows) {
    RenderGraph graph;
    FramePacket packet = make_test_packet();
    // Remove shadow-casting lights
    packet.dir_lights.clear();

    ForwardPlusConfig config;

    auto depth      = add_depth_prepass(graph, packet, config);
    auto shadows    = add_shadow_pass(graph, packet, config);
    // Shadow pass should have returned an invalid handle
    auto light_cull = add_light_culling_pass(graph, depth.depth, packet, config);
    auto hdr        = add_forward_pass(graph, depth.depth, shadows, light_cull,
                                       packet, config);
    auto skybox     = add_skybox_pass(graph, hdr.color, packet);
    auto ldr        = add_tonemap_pass(graph, skybox, packet, config);

    graph.set_output(ldr);
    graph.compile_only();

    // The shadow pass should still be registered but may have no output.
    // The forward pass should still work because it checks shadow_map.is_valid().
    // Tonemap should still be at the end.
    EXPECT_NE(order_of(graph, "TonemapPass"), -1)
        << "TonemapPass should always be in execution order";
}

// ===========================================================================
// Test: clear() resets the graph for reuse
// ===========================================================================

TEST(ForwardPlusGraph, GraphClear_ResetsForNextFrame) {
    RenderGraph graph;
    FramePacket packet = make_test_packet();
    ForwardPlusConfig config;

    // Build a graph
    auto depth      = add_depth_prepass(graph, packet, config);
    auto shadows    = add_shadow_pass(graph, packet, config);
    auto light_cull = add_light_culling_pass(graph, depth.depth, packet, config);
    auto hdr        = add_forward_pass(graph, depth.depth, shadows, light_cull,
                                       packet, config);
    auto skybox     = add_skybox_pass(graph, hdr.color, packet);
    auto ldr        = add_tonemap_pass(graph, skybox, packet, config);
    graph.set_output(ldr);

    EXPECT_EQ(graph.pass_count(), 6u);

    // Clear should reset
    graph.clear();
    EXPECT_EQ(graph.pass_count(), 0u);
    EXPECT_EQ(graph.resource_count(), 0u);

    // Should be able to build again
    auto depth2      = add_depth_prepass(graph, packet, config);
    auto shadows2    = add_shadow_pass(graph, packet, config);
    auto light_cull2 = add_light_culling_pass(graph, depth2.depth, packet, config);
    auto hdr2        = add_forward_pass(graph, depth2.depth, shadows2,
                                        light_cull2, packet, config);
    auto skybox2     = add_skybox_pass(graph, hdr2.color, packet);
    auto ldr2        = add_tonemap_pass(graph, skybox2, packet, config);
    graph.set_output(ldr2);

    EXPECT_EQ(graph.pass_count(), 6u);
}

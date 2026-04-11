// helios-rewrite/helios-renderer/tests/graph/test_graph_culling.cpp
//
// Detailed culling tests for the RenderGraph system.
// All tests use compile_only() -- no GPU required.

#include <gtest/gtest.h>
#include "helios/graph/render_graph.h"
#include "helios/rhi/rhi_types.h"

using namespace helios::graph;
using namespace helios::rhi;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static TextureDesc make_tex(uint32_t w = 64, uint32_t h = 64) {
    TextureDesc d;
    d.width  = w;
    d.height = h;
    d.format = TextureFormat::RGBA8;
    d.usage  = TextureUsage::ColorAttachment | TextureUsage::Sampled;
    return d;
}

static bool is_culled(const RenderGraph& graph, const std::string& name) {
    for (const auto& p : graph.passes()) {
        if (p.name == name) return p.culled;
    }
    return true; // not found = effectively culled
}

static bool in_execution_order(const RenderGraph& graph, const std::string& name) {
    const auto& order  = graph.execution_order();
    const auto& passes = graph.passes();
    for (uint32_t idx : order) {
        if (passes[idx].name == name) return true;
    }
    return false;
}

// ===========================================================================
// An isolated pass that writes to a resource nobody reads is culled
// ===========================================================================

TEST(GraphCulling, UnusedPassIsCulled) {
    RenderGraph graph;

    // PassKeep: creates the output resource
    struct KData { TextureHandle out; };
    graph.add_pass<KData>(
        "PassKeep",
        [](KData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "kept");
            b.write(d.out);
        },
        [](const KData&, RenderContext&) {}
    );

    // PassDead: creates a resource nobody cares about
    struct DData { TextureHandle out; };
    graph.add_pass<DData>(
        "PassDead",
        [](DData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "dead");
            b.write(d.out);
        },
        [](const DData&, RenderContext&) {}
    );

    // Output = resource 0 (kept)
    graph.set_output(TextureHandle{0});
    graph.compile_only();

    EXPECT_TRUE(is_culled(graph, "PassDead"));
    EXPECT_FALSE(is_culled(graph, "PassKeep"));
    EXPECT_EQ(graph.execution_order().size(), 1u);
}

// ===========================================================================
// Transitive dependency: A -> B -> output
// A is transitively needed -- must NOT be culled
// ===========================================================================

TEST(GraphCulling, TransitiveDependencyKeepsPassAlive) {
    RenderGraph graph;

    // A creates texA (0)
    struct AData { TextureHandle out; };
    graph.add_pass<AData>(
        "PassA",
        [](AData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texA");
            b.write(d.out);
        },
        [](const AData&, RenderContext&) {}
    );

    // B reads texA (0), creates texB (1) = the output
    struct BData { TextureHandle out; };
    graph.add_pass<BData>(
        "PassB",
        [](BData& d, RenderGraphBuilder& b) {
            b.read(TextureHandle{0}); // depends on A
            d.out = b.create(make_tex(), "texB");
            b.write(d.out);
        },
        [](const BData&, RenderContext&) {}
    );

    graph.set_output(TextureHandle{1}); // texB
    graph.compile_only();

    EXPECT_FALSE(is_culled(graph, "PassA")) << "PassA is transitively needed";
    EXPECT_FALSE(is_culled(graph, "PassB"));
    EXPECT_EQ(graph.execution_order().size(), 2u);
}

// ===========================================================================
// Multiple outputs: two separate output resources; all paths to both survive.
// In this setup we set_output to one resource, but the other path also feeds
// into it through a merge pass, so everything stays alive.
// ===========================================================================

TEST(GraphCulling, MultipleOutputPathsSurvive) {
    RenderGraph graph;

    // PathLeft: creates texLeft (0)
    struct LeftData { TextureHandle out; };
    graph.add_pass<LeftData>(
        "PathLeft",
        [](LeftData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texLeft");
            b.write(d.out);
        },
        [](const LeftData&, RenderContext&) {}
    );

    // PathRight: creates texRight (1)
    struct RightData { TextureHandle out; };
    graph.add_pass<RightData>(
        "PathRight",
        [](RightData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texRight");
            b.write(d.out);
        },
        [](const RightData&, RenderContext&) {}
    );

    // Merge: reads texLeft (0) and texRight (1), creates texMerge (2)
    struct MergeData { TextureHandle out; };
    graph.add_pass<MergeData>(
        "Merge",
        [](MergeData& d, RenderGraphBuilder& b) {
            b.read(TextureHandle{0});
            b.read(TextureHandle{1});
            d.out = b.create(make_tex(), "texMerge");
            b.write(d.out);
        },
        [](const MergeData&, RenderContext&) {}
    );

    graph.set_output(TextureHandle{2}); // texMerge
    graph.compile_only();

    EXPECT_EQ(graph.execution_order().size(), 3u);
    EXPECT_TRUE(in_execution_order(graph, "PathLeft"));
    EXPECT_TRUE(in_execution_order(graph, "PathRight"));
    EXPECT_TRUE(in_execution_order(graph, "Merge"));
}

// ===========================================================================
// A pass that is not on any path to the output, even transitively, is culled
// ===========================================================================

TEST(GraphCulling, IndirectlyUnusedPassIsCulled) {
    RenderGraph graph;

    // PassRoot: creates texRoot (0)
    struct RootData { TextureHandle out; };
    graph.add_pass<RootData>(
        "PassRoot",
        [](RootData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texRoot");
            b.write(d.out);
        },
        [](const RootData&, RenderContext&) {}
    );

    // PassOrphanProducer: creates texOrphanA (1) -- nobody reads it
    struct OrphanPData { TextureHandle out; };
    graph.add_pass<OrphanPData>(
        "PassOrphanProducer",
        [](OrphanPData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texOrphanA");
            b.write(d.out);
        },
        [](const OrphanPData&, RenderContext&) {}
    );

    // PassOrphanConsumer: reads texOrphanA (1), creates texOrphanB (2) -- nobody reads it
    struct OrphanCData { TextureHandle out; };
    graph.add_pass<OrphanCData>(
        "PassOrphanConsumer",
        [](OrphanCData& d, RenderGraphBuilder& b) {
            b.read(TextureHandle{1});
            d.out = b.create(make_tex(), "texOrphanB");
            b.write(d.out);
        },
        [](const OrphanCData&, RenderContext&) {}
    );

    graph.set_output(TextureHandle{0}); // only texRoot matters
    graph.compile_only();

    EXPECT_FALSE(is_culled(graph, "PassRoot"));
    EXPECT_TRUE(is_culled(graph, "PassOrphanProducer"));
    EXPECT_TRUE(is_culled(graph, "PassOrphanConsumer"));
    EXPECT_EQ(graph.execution_order().size(), 1u);
}

// helios-rewrite/helios-renderer/tests/graph/test_graph_compilation.cpp
//
// Tests for RenderGraph compilation order, using compile_only() which requires
// no GPU device.

#include <gtest/gtest.h>
#include "helios/graph/render_graph.h"
#include "helios/rhi/rhi_types.h"

using namespace helios::graph;
using namespace helios::rhi;

// ---------------------------------------------------------------------------
// Helper: build a minimal TextureDesc
// ---------------------------------------------------------------------------

static TextureDesc make_tex(uint32_t w = 64, uint32_t h = 64) {
    TextureDesc d;
    d.width  = w;
    d.height = h;
    d.format = TextureFormat::RGBA8;
    d.usage  = TextureUsage::ColorAttachment | TextureUsage::Sampled;
    return d;
}

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

// ===========================================================================
// Single pass producing output
// ===========================================================================

TEST(GraphCompilation, SinglePass) {
    RenderGraph graph;

    struct PassData { TextureHandle output; };
    graph.add_pass<PassData>(
        "OnlyPass",
        [](PassData& data, RenderGraphBuilder& builder) {
            data.output = builder.create(make_tex(), "result");
            builder.write(data.output);
        },
        [](const PassData&, RenderContext&) {}
    );

    // Get the output handle from pass data (it's the first resource, index 0).
    graph.set_output(TextureHandle{0});
    graph.compile_only();

    EXPECT_EQ(graph.execution_order().size(), 1u);
    EXPECT_FALSE(is_culled(graph, "OnlyPass"));
}

// ===========================================================================
// Empty graph -- no passes
// ===========================================================================

TEST(GraphCompilation, EmptyGraph) {
    RenderGraph graph;
    // No set_output call either.
    graph.compile_only();

    EXPECT_EQ(graph.execution_order().size(), 0u);
    EXPECT_EQ(graph.pass_count(), 0u);
}

// ===========================================================================
// 3-pass chain: A writes tex0, B reads tex0 & writes tex1, C reads tex1
// Expected order: A before B before C
// ===========================================================================

TEST(GraphCompilation, ThreePassChainOrder) {
    RenderGraph graph;

    // Pass A
    struct AData { TextureHandle out; };
    graph.add_pass<AData>(
        "PassA",
        [](AData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texA");
            b.write(d.out);
        },
        [](const AData&, RenderContext&) {}
    );

    // Pass B
    struct BData { TextureHandle in; TextureHandle out; };
    graph.add_pass<BData>(
        "PassB",
        [](BData& d, RenderGraphBuilder& b) {
            d.in  = TextureHandle{0}; // texA
            d.out = b.create(make_tex(), "texB");
            b.read(d.in);
            b.write(d.out);
        },
        [](const BData&, RenderContext&) {}
    );

    // Pass C
    struct CData { TextureHandle in; TextureHandle out; };
    graph.add_pass<CData>(
        "PassC",
        [](CData& d, RenderGraphBuilder& b) {
            d.in  = TextureHandle{1}; // texB
            d.out = b.create(make_tex(), "texC");
            b.read(d.in);
            b.write(d.out);
        },
        [](const CData&, RenderContext&) {}
    );

    // texC is at resource index 2
    graph.set_output(TextureHandle{2});
    graph.compile_only();

    ASSERT_EQ(graph.execution_order().size(), 3u);
    int oa = order_of(graph, "PassA");
    int ob = order_of(graph, "PassB");
    int oc = order_of(graph, "PassC");

    EXPECT_NE(oa, -1);
    EXPECT_NE(ob, -1);
    EXPECT_NE(oc, -1);
    EXPECT_LT(oa, ob) << "PassA must execute before PassB";
    EXPECT_LT(ob, oc) << "PassB must execute before PassC";
}

// ===========================================================================
// Diamond dependency: A->B, A->C, B->D, C->D
//   D is the output.
//   A must be before both B and C; B and C must be before D.
// ===========================================================================

TEST(GraphCompilation, DiamondDependency) {
    RenderGraph graph;

    // Pass A: creates texA (index 0)
    struct AData { TextureHandle out; };
    graph.add_pass<AData>(
        "A",
        [](AData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texA");
            b.write(d.out);
        },
        [](const AData&, RenderContext&) {}
    );

    // Pass B: reads texA (0), creates texB (1)
    struct BData { TextureHandle out; };
    graph.add_pass<BData>(
        "B",
        [](BData& d, RenderGraphBuilder& b) {
            b.read(TextureHandle{0});
            d.out = b.create(make_tex(), "texB");
            b.write(d.out);
        },
        [](const BData&, RenderContext&) {}
    );

    // Pass C: reads texA (0), creates texC (2)
    struct CData { TextureHandle out; };
    graph.add_pass<CData>(
        "C",
        [](CData& d, RenderGraphBuilder& b) {
            b.read(TextureHandle{0});
            d.out = b.create(make_tex(), "texC");
            b.write(d.out);
        },
        [](const CData&, RenderContext&) {}
    );

    // Pass D: reads texB (1) and texC (2), creates texD (3)
    struct DData { TextureHandle out; };
    graph.add_pass<DData>(
        "D",
        [](DData& d, RenderGraphBuilder& b) {
            b.read(TextureHandle{1});
            b.read(TextureHandle{2});
            d.out = b.create(make_tex(), "texD");
            b.write(d.out);
        },
        [](const DData&, RenderContext&) {}
    );

    // Output is texD at index 3
    graph.set_output(TextureHandle{3});
    graph.compile_only();

    ASSERT_EQ(graph.execution_order().size(), 4u);
    int oa = order_of(graph, "A");
    int ob = order_of(graph, "B");
    int oc = order_of(graph, "C");
    int od = order_of(graph, "D");

    EXPECT_LT(oa, ob) << "A must execute before B";
    EXPECT_LT(oa, oc) << "A must execute before C";
    EXPECT_LT(ob, od) << "B must execute before D";
    EXPECT_LT(oc, od) << "C must execute before D";
}

// ===========================================================================
// Pass culling: PassX writes tex that nobody reads; should be culled
// ===========================================================================

TEST(GraphCompilation, CulledUnusedPass) {
    RenderGraph graph;

    // PassUsed: creates texUsed (0)
    struct UsedData { TextureHandle out; };
    graph.add_pass<UsedData>(
        "PassUsed",
        [](UsedData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texUsed");
            b.write(d.out);
        },
        [](const UsedData&, RenderContext&) {}
    );

    // PassUnused: creates texUnused (1) -- nobody reads it
    struct UnusedData { TextureHandle out; };
    graph.add_pass<UnusedData>(
        "PassUnused",
        [](UnusedData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texUnused");
            b.write(d.out);
        },
        [](const UnusedData&, RenderContext&) {}
    );

    // PassAlso: creates texAlso (2), depends on texUsed (0)
    struct AlsoData { TextureHandle out; };
    graph.add_pass<AlsoData>(
        "PassAlso",
        [](AlsoData& d, RenderGraphBuilder& b) {
            b.read(TextureHandle{0});
            d.out = b.create(make_tex(), "texAlso");
            b.write(d.out);
        },
        [](const AlsoData&, RenderContext&) {}
    );

    // Output is texAlso (2); PassUnused should be culled
    graph.set_output(TextureHandle{2});
    graph.compile_only();

    EXPECT_EQ(graph.execution_order().size(), 2u);
    EXPECT_FALSE(is_culled(graph, "PassUsed"));
    EXPECT_FALSE(is_culled(graph, "PassAlso"));
    EXPECT_TRUE(is_culled(graph, "PassUnused"));
}

// ===========================================================================
// Side-effect pass: marked with set_side_effect(), must survive even without
// any resource output referenced by the graph output.
// ===========================================================================

TEST(GraphCompilation, SideEffectPassPreserved) {
    RenderGraph graph;

    // PassOutput: creates the graph output (index 0)
    struct OutData { TextureHandle out; };
    graph.add_pass<OutData>(
        "PassOutput",
        [](OutData& d, RenderGraphBuilder& b) {
            d.out = b.create(make_tex(), "texOutput");
            b.write(d.out);
        },
        [](const OutData&, RenderContext&) {}
    );

    // PassSideEffect: no resource output the graph cares about, but marked
    // as a side-effect pass (e.g. copy-to-disk, UI overlay).
    struct SideData {};
    graph.add_pass<SideData>(
        "PassSideEffect",
        [](SideData&, RenderGraphBuilder& b) {
            b.set_side_effect();
        },
        [](const SideData&, RenderContext&) {}
    );

    graph.set_output(TextureHandle{0});
    graph.compile_only();

    // Both passes should survive
    EXPECT_EQ(graph.execution_order().size(), 2u);
    EXPECT_FALSE(is_culled(graph, "PassOutput"));
    EXPECT_FALSE(is_culled(graph, "PassSideEffect"));
}

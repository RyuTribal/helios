// helios-rewrite/helios-renderer/src/helios/graph/render_graph_builder.h
#pragma once

#include "helios/graph/render_graph_resources.h"
#include <vector>
#include <string>

namespace helios::graph {

// Forward declaration -- RenderGraph owns the resource table; builder writes into it.
class RenderGraph;

class RenderGraphBuilder {
public:
    // Constructed by RenderGraph::add_pass() with a reference to the graph
    // and the index of the pass being set up.
    explicit RenderGraphBuilder(RenderGraph& graph, uint32_t pass_index);

    // --- Texture operations ---

    // Declare that this pass reads from a texture produced by an earlier pass.
    // Returns the same handle (for chaining convenience).
    TextureHandle read(TextureHandle input, ResourceUsage usage = ResourceUsage::ShaderRead);

    // Declare that this pass writes to a texture (render target or storage write).
    // Returns the same handle.
    TextureHandle write(TextureHandle output, ResourceUsage usage = ResourceUsage::ColorAttachment);

    // Create a new transient texture. Returns a handle to the newly created resource.
    TextureHandle create(const rhi::TextureDesc& desc, const std::string& name = "");

    // --- Buffer operations ---

    BufferHandle read(BufferHandle input, ResourceUsage usage = ResourceUsage::ShaderRead);
    BufferHandle write(BufferHandle output, ResourceUsage usage = ResourceUsage::ShaderWrite);
    BufferHandle create(const rhi::BufferDesc& desc, const std::string& name = "");

    // --- Side-effect pass (no resource outputs, e.g. copy, present) ---

    void set_side_effect();

private:
    RenderGraph& m_graph;
    uint32_t m_pass_index;
};

} // namespace helios::graph

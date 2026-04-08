// helios-rewrite/helios-renderer/src/helios/graph/render_graph_builder.cpp
#include "helios/graph/render_graph_builder.h"
#include "helios/graph/render_graph.h"   // full definition needed for resource table access

namespace helios::graph {

RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, uint32_t pass_index)
    : m_graph(graph)
    , m_pass_index(pass_index)
{}

// ---------------------------------------------------------------------------
// Texture operations
// ---------------------------------------------------------------------------

TextureHandle RenderGraphBuilder::read(TextureHandle input, ResourceUsage usage) {
    m_graph.record_read(m_pass_index, input.index, usage);
    return input;
}

TextureHandle RenderGraphBuilder::write(TextureHandle output, ResourceUsage usage) {
    m_graph.record_write(m_pass_index, output.index, usage);
    return output;
}

TextureHandle RenderGraphBuilder::create(const rhi::TextureDesc& desc, const std::string& name) {
    uint32_t idx = m_graph.create_resource_node(
        name.empty() ? ("transient_tex_" + std::to_string(m_graph.resource_count())) : name,
        ResourceType::Texture,
        ResourceLifetime::Transient,
        TextureResource{ .desc = desc }
    );
    // Creating a resource implies a write from this pass.
    m_graph.record_write(m_pass_index, idx, ResourceUsage::ColorAttachment);
    return TextureHandle{ idx };
}

// ---------------------------------------------------------------------------
// Buffer operations
// ---------------------------------------------------------------------------

BufferHandle RenderGraphBuilder::read(BufferHandle input, ResourceUsage usage) {
    m_graph.record_read(m_pass_index, input.index, usage);
    return input;
}

BufferHandle RenderGraphBuilder::write(BufferHandle output, ResourceUsage usage) {
    m_graph.record_write(m_pass_index, output.index, usage);
    return output;
}

BufferHandle RenderGraphBuilder::create(const rhi::BufferDesc& desc, const std::string& name) {
    uint32_t idx = m_graph.create_resource_node(
        name.empty() ? ("transient_buf_" + std::to_string(m_graph.resource_count())) : name,
        ResourceType::Buffer,
        ResourceLifetime::Transient,
        BufferResource{ .desc = desc }
    );
    m_graph.record_write(m_pass_index, idx, ResourceUsage::ShaderWrite);
    return BufferHandle{ idx };
}

// ---------------------------------------------------------------------------
// Side-effect
// ---------------------------------------------------------------------------

void RenderGraphBuilder::set_side_effect() {
    m_graph.mark_side_effect(m_pass_index);
}

} // namespace helios::graph

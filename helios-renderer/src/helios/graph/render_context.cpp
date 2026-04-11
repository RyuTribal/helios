#include "helios/graph/render_context.h"
#include "helios/core/assert.h"

namespace helios::graph {

RenderContext::RenderContext(
    rhi::CommandBuffer& cmd,
    std::vector<rhi::Texture*>& resolved_textures,
    std::vector<rhi::Buffer*>& resolved_buffers,
    const std::vector<ResourceNode>& resource_nodes
)
    : m_cmd(cmd)
    , m_resolved_textures(resolved_textures)
    , m_resolved_buffers(resolved_buffers)
    , m_resource_nodes(resource_nodes)
{}

rhi::Texture& RenderContext::resolve(TextureHandle handle) const {
    HELIOS_ASSERT(handle.is_valid(), "Invalid TextureHandle");
    HELIOS_ASSERT(handle.index < m_resource_nodes.size(), "TextureHandle out of range");

    const auto& node = m_resource_nodes[handle.index];
    HELIOS_ASSERT(node.type == ResourceType::Texture, "Handle does not reference a texture");

    rhi::Texture* tex = m_resolved_textures[handle.index];
    HELIOS_ASSERT(tex != nullptr, "Texture not yet allocated -- resolve called outside execute phase?");
    return *tex;
}

rhi::Buffer& RenderContext::resolve(BufferHandle handle) const {
    HELIOS_ASSERT(handle.is_valid(), "Invalid BufferHandle");
    HELIOS_ASSERT(handle.index < m_resource_nodes.size(), "BufferHandle out of range");

    const auto& node = m_resource_nodes[handle.index];
    HELIOS_ASSERT(node.type == ResourceType::Buffer, "Handle does not reference a buffer");

    rhi::Buffer* buf = m_resolved_buffers[handle.index];
    HELIOS_ASSERT(buf != nullptr, "Buffer not yet allocated -- resolve called outside execute phase?");
    return *buf;
}

rhi::CommandBuffer& RenderContext::cmd() {
    return m_cmd;
}

} // namespace helios::graph

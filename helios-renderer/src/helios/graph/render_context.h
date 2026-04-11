#pragma once

#include "helios/graph/render_graph_resources.h"
#include "helios/rhi/rhi.h"
#include <vector>
#include <memory>

namespace helios::graph {

class RenderContext {
public:
    RenderContext(
        rhi::CommandBuffer& cmd,
        std::vector<rhi::Texture*>& resolved_textures,
        std::vector<rhi::Buffer*>& resolved_buffers,
        const std::vector<ResourceNode>& resource_nodes
    );

    // Resolve a graph handle to the actual GPU texture.
    rhi::Texture& resolve(TextureHandle handle) const;

    // Resolve a graph handle to the actual GPU buffer.
    rhi::Buffer& resolve(BufferHandle handle) const;

    // Access the command buffer for GPU command recording.
    rhi::CommandBuffer& cmd();

private:
    rhi::CommandBuffer& m_cmd;
    std::vector<rhi::Texture*>& m_resolved_textures;
    std::vector<rhi::Buffer*>& m_resolved_buffers;
    const std::vector<ResourceNode>& m_resource_nodes;
};

} // namespace helios::graph

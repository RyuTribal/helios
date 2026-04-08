// helios-rewrite/helios-renderer/src/helios/graph/render_graph.h
#pragma once

#include "helios/graph/render_graph_resources.h"
#include "helios/graph/render_graph_builder.h"
#include "helios/graph/render_context.h"
#include "helios/graph/resource_pool.h"
#include "helios/rhi/rhi.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace helios::graph {

// ---------------------------------------------------------------------------
// PassNode -- internal representation of a single render pass.
// ---------------------------------------------------------------------------

struct PassNode {
    std::string name;
    uint32_t index = 0;

    // Resources this pass reads from.
    std::vector<ResourceAccess> reads;

    // Resources this pass writes to.
    std::vector<ResourceAccess> writes;

    // Type-erased pass data (allocated by add_pass<Data>).
    std::shared_ptr<void> data;

    // Execute callback. Captured by add_pass<Data>; invokes the user's
    // execute lambda with the correctly-typed Data& and a RenderContext&.
    std::function<void(RenderContext&)> execute;

    // Compilation results
    bool culled = false;         // true if backward walk determined this pass is unused
    bool side_effect = false;    // true if pass has side effects (never culled)
    uint32_t sorted_order = 0;   // position in the final execution order
};

// ---------------------------------------------------------------------------
// Barrier -- inserted by the compiler between passes.
// ---------------------------------------------------------------------------

struct BarrierInfo {
    uint32_t resource_index;
    ResourceUsage usage_before;
    ResourceUsage usage_after;
};

struct PassBarriers {
    uint32_t pass_index;
    std::vector<BarrierInfo> barriers;
};

// ---------------------------------------------------------------------------
// RenderGraph
// ---------------------------------------------------------------------------

class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph();

    RenderGraph(RenderGraph&&) noexcept;
    RenderGraph& operator=(RenderGraph&&) noexcept;
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    // -----------------------------------------------------------------------
    // Setup phase -- called by the graph-building system (main thread).
    // -----------------------------------------------------------------------

    // Import a persistent (externally-owned) texture into the graph.
    TextureHandle import_texture(const std::string& name, rhi::Texture& external);

    // Import a persistent (externally-owned) buffer into the graph.
    BufferHandle import_buffer(const std::string& name, rhi::Buffer& external);

    // Add a render pass with typed data.
    //
    //   Data  -- plain struct that the setup lambda populates with handles
    //            and the execute lambda reads back.
    //   setup -- called immediately; receives Data& and RenderGraphBuilder&.
    //   execute -- called later during the execute phase with const Data&
    //              and RenderContext&.
    //
    template<typename Data>
    Data& add_pass(
        const char* name,
        std::function<void(Data&, RenderGraphBuilder&)> setup,
        std::function<void(const Data&, RenderContext&)> execute
    );

    // Designate the final output texture. The graph compiler uses this as
    // the root for its backward liveness walk.
    void set_output(TextureHandle final_color);

    // -----------------------------------------------------------------------
    // Compilation + Execution (called by render thread).
    // -----------------------------------------------------------------------

    // Compile the graph (cull, sort, insert barriers) and execute all
    // surviving passes. Allocates transient resources from the pool.
    void compile_and_execute(rhi::Device& device, ResourcePool& pool);

    // Compile without execution -- for testing.
    void compile_only();

    // Reset the graph for the next frame (clears passes and resources,
    // but does NOT clear the pool).
    void clear();

    // -----------------------------------------------------------------------
    // Accessors (mostly for testing / debugging).
    // -----------------------------------------------------------------------

    uint32_t pass_count() const { return static_cast<uint32_t>(m_passes.size()); }
    uint32_t resource_count() const { return static_cast<uint32_t>(m_resources.size()); }
    const std::vector<PassNode>& passes() const { return m_passes; }
    const std::vector<ResourceNode>& resources() const { return m_resources; }

    // After compile, returns passes in execution order (culled excluded).
    const std::vector<uint32_t>& execution_order() const { return m_execution_order; }

    // After compile, returns computed barriers.
    const std::vector<PassBarriers>& barriers() const { return m_barriers; }

    // -----------------------------------------------------------------------
    // Internal -- used by RenderGraphBuilder (friend-like access via public,
    // but not part of the user-facing API).
    // -----------------------------------------------------------------------

    uint32_t create_resource_node(
        const std::string& name,
        ResourceType type,
        ResourceLifetime lifetime,
        std::variant<TextureResource, BufferResource> resource
    );

    void record_read(uint32_t pass_index, uint32_t resource_index, ResourceUsage usage);
    void record_write(uint32_t pass_index, uint32_t resource_index, ResourceUsage usage);
    void mark_side_effect(uint32_t pass_index);

private:
    // -----------------------------------------------------------------------
    // Compile sub-steps
    // -----------------------------------------------------------------------

    // Walk backwards from the output, marking reachable passes.
    void cull_passes();

    // Topological sort of surviving passes.
    void topological_sort();

    // Compute resource lifetime spans and insert barriers.
    void compute_barriers();

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------

    std::vector<PassNode> m_passes;
    std::vector<ResourceNode> m_resources;

    TextureHandle m_output;  // final output handle set by set_output()

    // Populated by compile
    std::vector<uint32_t> m_execution_order;   // pass indices in execution order
    std::vector<PassBarriers> m_barriers;       // barriers to issue before each pass

    // Track last usage of each resource (resource_index -> ResourceUsage)
    // for barrier insertion. Rebuilt each frame during compile.
    std::vector<ResourceUsage> m_last_resource_usage;
};

// ---------------------------------------------------------------------------
// Template implementation -- must be in header.
// ---------------------------------------------------------------------------

template<typename Data>
Data& RenderGraph::add_pass(
    const char* name,
    std::function<void(Data&, RenderGraphBuilder&)> setup,
    std::function<void(const Data&, RenderContext&)> execute
) {
    uint32_t pass_index = static_cast<uint32_t>(m_passes.size());

    // Allocate pass data.
    auto data_ptr = std::make_shared<Data>();

    // Create the pass node.
    PassNode node;
    node.name = name;
    node.index = pass_index;
    node.data = data_ptr;

    // Capture the execute lambda with the typed data pointer.
    node.execute = [data_ptr, exec = std::move(execute)](RenderContext& ctx) {
        exec(*data_ptr, ctx);
    };

    m_passes.push_back(std::move(node));

    // Run the setup lambda so the pass declares its resource dependencies.
    RenderGraphBuilder builder(*this, pass_index);
    setup(*data_ptr, builder);

    return *data_ptr;
}

} // namespace helios::graph

#include "helios/graph/render_graph.h"
#include "helios/graph/graph_log_channel.h"
#include "helios/core/assert.h"

#include <algorithm>
#include <queue>
#include <unordered_set>
#include <stack>

namespace helios::graph {

RenderGraph::RenderGraph() = default;
RenderGraph::~RenderGraph() = default;
RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
RenderGraph& RenderGraph::operator=(RenderGraph&&) noexcept = default;

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

TextureHandle RenderGraph::import_texture(const std::string& name, rhi::Texture& external) {
    uint32_t idx = create_resource_node(
        name,
        ResourceType::Texture,
        ResourceLifetime::Imported,
        TextureResource{ .desc = {}, .imported = &external }
    );
    HELIOS_LOG(Graph, Trace, "RenderGraph: imported texture '{}' at index {}", name, idx);
    return TextureHandle{ idx };
}

BufferHandle RenderGraph::import_buffer(const std::string& name, rhi::Buffer& external) {
    uint32_t idx = create_resource_node(
        name,
        ResourceType::Buffer,
        ResourceLifetime::Imported,
        BufferResource{ .desc = {}, .imported = &external }
    );
    HELIOS_LOG(Graph, Trace, "RenderGraph: imported buffer '{}' at index {}", name, idx);
    return BufferHandle{ idx };
}

void RenderGraph::set_output(TextureHandle final_color) {
    m_output = final_color;
}

// ---------------------------------------------------------------------------
// Internal resource / pass recording
// ---------------------------------------------------------------------------

uint32_t RenderGraph::create_resource_node(
    const std::string& name,
    ResourceType type,
    ResourceLifetime lifetime,
    std::variant<TextureResource, BufferResource> resource
) {
    uint32_t idx = static_cast<uint32_t>(m_resources.size());
    ResourceNode node;
    node.name = name;
    node.type = type;
    node.lifetime = lifetime;
    node.resource = std::move(resource);
    m_resources.push_back(std::move(node));
    return idx;
}

void RenderGraph::record_read(uint32_t pass_index, uint32_t resource_index, ResourceUsage usage) {
    HELIOS_ASSERT(pass_index < m_passes.size(), "pass_index out of range");
    HELIOS_ASSERT(resource_index < m_resources.size(), "resource_index out of range");
    m_passes[pass_index].reads.push_back(ResourceAccess{ resource_index, usage });
    m_resources[resource_index].ref_count++;
}

void RenderGraph::record_write(uint32_t pass_index, uint32_t resource_index, ResourceUsage usage) {
    HELIOS_ASSERT(pass_index < m_passes.size(), "pass_index out of range");
    HELIOS_ASSERT(resource_index < m_resources.size(), "resource_index out of range");
    m_passes[pass_index].writes.push_back(ResourceAccess{ resource_index, usage });
    m_resources[resource_index].ref_count++;
}

void RenderGraph::mark_side_effect(uint32_t pass_index) {
    HELIOS_ASSERT(pass_index < m_passes.size(), "pass_index out of range");
    m_passes[pass_index].side_effect = true;
}

// ---------------------------------------------------------------------------
// Compile only (for testing)
// ---------------------------------------------------------------------------

void RenderGraph::compile_only() {
    cull_passes();
    topological_sort();
    compute_barriers();

    HELIOS_LOG(Graph, Debug, "RenderGraph::compile_only: {} passes survived out of {}",
        m_execution_order.size(), m_passes.size());
}

// ---------------------------------------------------------------------------
// Compile + Execute
// ---------------------------------------------------------------------------

void RenderGraph::compile_and_execute(rhi::Device& device, ResourcePool& pool,
                                      rhi::Swapchain* swapchain) {
    // Phase 1: Setup is already complete (add_pass ran setup lambdas eagerly).

    // Phase 2: Compile.
    cull_passes();
    topological_sort();
    compute_barriers();

    HELIOS_LOG(Graph, Debug, "RenderGraph: {} passes survived out of {}, executing...",
        m_execution_order.size(), m_passes.size());

    // Phase 3: Execute surviving passes.
    // Prepare resolved resource arrays.
    std::vector<rhi::Texture*> resolved_textures(m_resources.size(), nullptr);
    std::vector<rhi::Buffer*> resolved_buffers(m_resources.size(), nullptr);

    // Resolve imported resources.
    for (uint32_t i = 0; i < m_resources.size(); i++) {
        auto& res = m_resources[i];
        if (res.lifetime == ResourceLifetime::Imported) {
            if (res.type == ResourceType::Texture) {
                resolved_textures[i] = std::get<TextureResource>(res.resource).imported;
            } else {
                resolved_buffers[i] = std::get<BufferResource>(res.resource).imported;
            }
        }
    }

    // Compute resource lifetimes (first_pass / last_pass) over the execution order.
    for (uint32_t order = 0; order < m_execution_order.size(); order++) {
        uint32_t pi = m_execution_order[order];
        const auto& pass = m_passes[pi];

        auto touch = [&](uint32_t ri) {
            auto& res = m_resources[ri];
            if (res.first_pass == UINT32_MAX) res.first_pass = order;
            res.last_pass = order;
        };

        for (const auto& r : pass.reads)  touch(r.resource_index);
        for (const auto& w : pass.writes) touch(w.resource_index);
    }

    // Create command buffer for the frame.
    auto cmd = device.create_command_buffer();
    HELIOS_ASSERT(cmd != nullptr, "Failed to create command buffer");
    cmd->begin();

    // Execute passes in order.
    for (uint32_t order = 0; order < m_execution_order.size(); order++) {
        uint32_t pi = m_execution_order[order];
        const auto& pass = m_passes[pi];

        // Allocate transient resources whose first_pass is this order index.
        for (const auto& w : pass.writes) {
            uint32_t ri = w.resource_index;
            auto& res = m_resources[ri];
            if (res.lifetime == ResourceLifetime::Transient && res.first_pass == order) {
                if (res.type == ResourceType::Texture) {
                    const auto& tex_res = std::get<TextureResource>(res.resource);
                    resolved_textures[ri] = pool.acquire_texture(tex_res.desc);
                } else {
                    const auto& buf_res = std::get<BufferResource>(res.resource);
                    resolved_buffers[ri] = pool.acquire_buffer(buf_res.desc);
                }
            }
        }
        // Also check reads -- a transient resource created by an earlier pass
        // might first appear in a read here if the creating pass was ordered
        // earlier and we already allocated it.
        for (const auto& r : pass.reads) {
            uint32_t ri = r.resource_index;
            auto& res = m_resources[ri];
            if (res.lifetime == ResourceLifetime::Transient && res.first_pass == order) {
                if (res.type == ResourceType::Texture && resolved_textures[ri] == nullptr) {
                    const auto& tex_res = std::get<TextureResource>(res.resource);
                    resolved_textures[ri] = pool.acquire_texture(tex_res.desc);
                } else if (res.type == ResourceType::Buffer && resolved_buffers[ri] == nullptr) {
                    const auto& buf_res = std::get<BufferResource>(res.resource);
                    resolved_buffers[ri] = pool.acquire_buffer(buf_res.desc);
                }
            }
        }

        // Issue barriers before this pass.
        // The barrier descriptions were computed during compile phase.
        // We translate them into pipeline_barrier calls on the command buffer.
        for (const auto& pb : m_barriers) {
            if (pb.pass_index == pi) {
                // Map a ResourceUsage value to the corresponding ShaderStage.
                // ColorAttachment / DepthAttachment are written in the fragment
                // stage.  Storage reads/writes happen in compute.  Plain shader
                // reads default to fragment (the most common consumer).
                // Transfer has no dedicated ShaderStage value, so we approximate
                // with Compute (both are outside the rasterization pipeline).
                auto usage_to_stage = [](ResourceUsage u) -> rhi::ShaderStage {
                    switch (u) {
                        case ResourceUsage::ColorAttachment:
                        case ResourceUsage::DepthAttachment:
                            return rhi::ShaderStage::Fragment;
                        case ResourceUsage::ShaderRead:
                            return rhi::ShaderStage::Fragment;
                        case ResourceUsage::ShaderWrite:
                            return rhi::ShaderStage::Compute;
                        case ResourceUsage::TransferSrc:
                        case ResourceUsage::TransferDst:
                            return rhi::ShaderStage::Compute;
                        case ResourceUsage::Present:
                            return rhi::ShaderStage::Fragment;
                        default:
                            return rhi::ShaderStage::Fragment;
                    }
                };

                for (const auto& b : pb.barriers) {
                    rhi::BarrierDesc barrier_desc{};
                    barrier_desc.src_stage = usage_to_stage(b.usage_before);
                    barrier_desc.dst_stage = usage_to_stage(b.usage_after);
                    cmd->pipeline_barrier(barrier_desc);
                }
                break;
            }
        }

        // Execute the pass.
        RenderContext ctx(*cmd, resolved_textures, resolved_buffers, m_resources);
        pass.execute(ctx);

        // Release transient resources whose last_pass is this order index.
        for (uint32_t ri = 0; ri < m_resources.size(); ri++) {
            auto& res = m_resources[ri];
            if (res.lifetime == ResourceLifetime::Transient && res.last_pass == order) {
                if (res.type == ResourceType::Texture && resolved_textures[ri]) {
                    pool.release_texture(resolved_textures[ri]);
                    resolved_textures[ri] = nullptr;
                } else if (res.type == ResourceType::Buffer && resolved_buffers[ri]) {
                    pool.release_buffer(resolved_buffers[ri]);
                    resolved_buffers[ri] = nullptr;
                }
            }
        }
    }

    cmd->end();

    // Submit the command buffer.
    // When a swapchain is provided, use submit_for_present so that the
    // image-available and render-finished semaphores are wired up correctly
    // for synchronisation with the present call.  Without this the present
    // call would wait on a semaphore that was never signaled.
    if (swapchain != nullptr) {
        device.submit_for_present(*cmd, *swapchain);
    } else {
        device.submit(*cmd, {});
    }
}

// ---------------------------------------------------------------------------
// cull_passes -- backward walk from output
// ---------------------------------------------------------------------------

void RenderGraph::cull_passes() {
    // Start with all passes culled.
    for (auto& pass : m_passes) {
        pass.culled = true;
    }

    // Find the pass(es) that write to the output resource.
    // Also keep all side-effect passes.
    std::unordered_set<uint32_t> alive_resources;
    std::stack<uint32_t> work_stack;

    // Seed: output resource and side-effect passes.
    if (m_output.is_valid()) {
        alive_resources.insert(m_output.index);
    }

    // Lambda to mark a pass as alive and propagate its read dependencies.
    auto mark_pass = [&](uint32_t pi) {
        if (!m_passes[pi].culled) return; // already alive
        m_passes[pi].culled = false;
        // All resources this pass reads become alive too.
        for (const auto& r : m_passes[pi].reads) {
            if (alive_resources.insert(r.resource_index).second) {
                // New alive resource -- find passes that write it.
                work_stack.push(r.resource_index);
            }
        }
    };

    // Side-effect passes are always alive.
    for (uint32_t pi = 0; pi < m_passes.size(); pi++) {
        if (m_passes[pi].side_effect) {
            mark_pass(pi);
        }
    }

    // Seed from output: find passes that write to the output resource.
    if (m_output.is_valid()) {
        for (uint32_t pi = 0; pi < m_passes.size(); pi++) {
            for (const auto& w : m_passes[pi].writes) {
                if (w.resource_index == m_output.index) {
                    mark_pass(pi);
                }
            }
        }
    }

    // Propagate: for each alive resource, find all passes that produce it,
    // mark them alive, and add their inputs to the alive set.
    while (!work_stack.empty()) {
        uint32_t ri = work_stack.top();
        work_stack.pop();

        for (uint32_t pi = 0; pi < m_passes.size(); pi++) {
            for (const auto& w : m_passes[pi].writes) {
                if (w.resource_index == ri) {
                    mark_pass(pi);
                }
            }
        }
    }

    // Log culling results.
    uint32_t culled_count = 0;
    for (const auto& pass : m_passes) {
        if (pass.culled) culled_count++;
    }
    if (culled_count > 0) {
        HELIOS_LOG(Graph, Debug, "RenderGraph: culled {} passes out of {}", culled_count, m_passes.size());
    }
}

// ---------------------------------------------------------------------------
// topological_sort -- order surviving passes respecting dependencies
// ---------------------------------------------------------------------------

void RenderGraph::topological_sort() {
    m_execution_order.clear();

    uint32_t num_passes = static_cast<uint32_t>(m_passes.size());

    // Build adjacency: if pass A writes resource R and pass B reads resource R,
    // then A must execute before B (edge A -> B).
    // Only consider non-culled passes.

    // resource_index -> vector of pass indices that write it.
    std::vector<std::vector<uint32_t>> writers(m_resources.size());
    for (uint32_t pi = 0; pi < num_passes; pi++) {
        if (m_passes[pi].culled) continue;
        for (const auto& w : m_passes[pi].writes) {
            writers[w.resource_index].push_back(pi);
        }
    }

    // Build in-degree and adjacency list.
    std::vector<uint32_t> in_degree(num_passes, 0);
    std::vector<std::vector<uint32_t>> adj(num_passes);

    for (uint32_t pi = 0; pi < num_passes; pi++) {
        if (m_passes[pi].culled) continue;
        for (const auto& r : m_passes[pi].reads) {
            // Every writer of this resource must run before this pass.
            for (uint32_t writer_pi : writers[r.resource_index]) {
                if (writer_pi != pi) {
                    adj[writer_pi].push_back(pi);
                    in_degree[pi]++;
                }
            }
        }
    }

    // Kahn's algorithm.
    std::queue<uint32_t> ready;
    for (uint32_t pi = 0; pi < num_passes; pi++) {
        if (!m_passes[pi].culled && in_degree[pi] == 0) {
            ready.push(pi);
        }
    }

    while (!ready.empty()) {
        uint32_t pi = ready.front();
        ready.pop();

        m_passes[pi].sorted_order = static_cast<uint32_t>(m_execution_order.size());
        m_execution_order.push_back(pi);

        for (uint32_t next : adj[pi]) {
            in_degree[next]--;
            if (in_degree[next] == 0) {
                ready.push(next);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// compute_barriers -- derive transitions between passes
// ---------------------------------------------------------------------------

void RenderGraph::compute_barriers() {
    m_barriers.clear();
    m_last_resource_usage.assign(m_resources.size(), ResourceUsage::None);

    for (uint32_t pi : m_execution_order) {
        const auto& pass = m_passes[pi];
        PassBarriers pb;
        pb.pass_index = pi;

        auto check_transition = [&](uint32_t ri, ResourceUsage new_usage) {
            ResourceUsage old_usage = m_last_resource_usage[ri];
            if (old_usage != new_usage && old_usage != ResourceUsage::None) {
                pb.barriers.push_back(BarrierInfo{
                    .resource_index = ri,
                    .usage_before = old_usage,
                    .usage_after = new_usage,
                });
            }
            m_last_resource_usage[ri] = new_usage;
        };

        for (const auto& r : pass.reads) {
            check_transition(r.resource_index, r.usage);
        }
        for (const auto& w : pass.writes) {
            check_transition(w.resource_index, w.usage);
        }

        if (!pb.barriers.empty()) {
            m_barriers.push_back(std::move(pb));
        }
    }
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

void RenderGraph::clear() {
    m_passes.clear();
    m_resources.clear();
    m_execution_order.clear();
    m_barriers.clear();
    m_last_resource_usage.clear();
    m_output = TextureHandle{};
}

} // namespace helios::graph

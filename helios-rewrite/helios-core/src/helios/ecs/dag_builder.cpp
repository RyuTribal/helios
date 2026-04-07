// helios-core/src/helios/ecs/dag_builder.cpp
#include "helios/ecs/dag_builder.h"
#include "helios/ecs/access_descriptor.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

namespace helios {

namespace {

/// Check whether two system descriptors have conflicting data access.
bool systems_conflict(const SystemDescriptor& a, const SystemDescriptor& b) {
    return has_conflict(a.accesses, b.accesses);
}

} // anonymous namespace

ExecutionPlan build_execution_plan(const std::vector<SystemDescriptor>& systems) {
    const size_t n = systems.size();
    if (n == 0) return {};

    // ---- 1. Build adjacency list + in-degree map ----
    // edges[i] contains all j such that i must run before j.
    std::vector<std::vector<size_t>> edges(n);
    std::vector<uint32_t> in_degree(n, 0);

    // Map SystemId -> index for explicit ordering resolution
    std::unordered_map<uint64_t, size_t> id_to_index;
    for (size_t i = 0; i < n; ++i) {
        id_to_index[systems[i].id.value] = i;
    }

    // Explicit ordering: after / before constraints
    for (size_t i = 0; i < n; ++i) {
        // "systems[i] must run after system X"  =>  edge X -> i
        for (const auto& dep : systems[i].after) {
            auto it = id_to_index.find(dep.value);
            if (it != id_to_index.end()) {
                size_t from = it->second;
                edges[from].push_back(i);
                in_degree[i]++;
            }
        }
        // "systems[i] must run before system X"  =>  edge i -> X
        for (const auto& dep : systems[i].before) {
            auto it = id_to_index.find(dep.value);
            if (it != id_to_index.end()) {
                size_t to = it->second;
                edges[i].push_back(to);
                in_degree[to]++;
            }
        }
    }

    // Access conflicts: if systems[i] and systems[j] conflict, and neither
    // has an explicit ordering edge already, add an edge i->j (arbitrary but
    // deterministic: lower index runs first). This preserves registration order
    // as a tiebreaker for conflicting systems.
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (!systems_conflict(systems[i], systems[j])) continue;

            // Check if there is already an explicit path between i and j.
            // For simplicity we only check direct edges, not transitive.
            bool has_edge_ij = false;
            bool has_edge_ji = false;
            for (size_t dst : edges[i]) {
                if (dst == j) { has_edge_ij = true; break; }
            }
            for (size_t dst : edges[j]) {
                if (dst == i) { has_edge_ji = true; break; }
            }

            if (!has_edge_ij && !has_edge_ji) {
                // Deterministic tiebreak: lower registration index goes first
                edges[i].push_back(j);
                in_degree[j]++;
            }
        }
    }

    // ---- 2. Kahn's algorithm with depth tracking ----
    // depth[i] = length of longest path to i (determines stage assignment)
    std::vector<uint32_t> depth(n, 0);
    std::queue<size_t> ready;

    for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            ready.push(i);
        }
    }

    std::vector<size_t> topo_order;
    topo_order.reserve(n);

    while (!ready.empty()) {
        size_t current = ready.front();
        ready.pop();
        topo_order.push_back(current);

        for (size_t next : edges[current]) {
            depth[next] = std::max(depth[next], depth[current] + 1);
            in_degree[next]--;
            if (in_degree[next] == 0) {
                ready.push(next);
            }
        }
    }

    // Cycle detection
    if (topo_order.size() != n) {
        // Cycle detected -- return empty plan. The caller should treat this
        // as a fatal configuration error.
        return {};
    }

    // ---- 3. Group by depth into stages ----
    uint32_t max_depth = *std::max_element(depth.begin(), depth.end());
    ExecutionPlan plan;
    plan.stages.resize(max_depth + 1);

    for (size_t i = 0; i < n; ++i) {
        plan.stages[depth[i]].system_indices.push_back(i);
    }

    return plan;
}

} // namespace helios

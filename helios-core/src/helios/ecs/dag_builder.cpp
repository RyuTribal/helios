#include "helios/ecs/dag_builder.h"
#include "helios/ecs/access_descriptor.h"
#include "helios/core/assert.h"
#include "helios/core/engine_log_channels.h"

#include <algorithm>
#include <queue>
#include <string>
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

    // Ambiguity detection (Bevy-style): warn about conflicting systems that
    // are not transitively ordered. Uses BFS reachability to avoid false
    // positives from chained explicit edges (A→B→C means A and C are ordered).

    // Build reachability matrix via BFS from each node.
    std::vector<std::vector<bool>> reachable(n, std::vector<bool>(n, false));
    for (size_t src = 0; src < n; ++src) {
        std::queue<size_t> bfs;
        bfs.push(src);
        while (!bfs.empty()) {
            size_t cur = bfs.front(); bfs.pop();
            for (size_t next : edges[cur]) {
                if (!reachable[src][next]) {
                    reachable[src][next] = true;
                    bfs.push(next);
                }
            }
        }
    }

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (!systems_conflict(systems[i], systems[j])) continue;
            if (reachable[i][j] || reachable[j][i]) continue; // transitively ordered

            HELIOS_LOG(Scheduler, Warn,
                "Ambiguous system ordering: '{}' and '{}' have conflicting "
                "access but no explicit ordering constraint",
                systems[i].name, systems[j].name);
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
        // Identify which systems are in the cycle (those not in topo_order).
        std::string cycle_systems;
        for (size_t i = 0; i < n; i++) {
            if (std::find(topo_order.begin(), topo_order.end(), i) == topo_order.end()) {
                if (!cycle_systems.empty()) cycle_systems += ", ";
                cycle_systems += systems[i].name;
            }
        }
        HELIOS_ASSERT(false,
            ("System ordering cycle detected involving: " + cycle_systems).c_str());
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

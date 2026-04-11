#pragma once

#include "helios/ecs/system_descriptor.h"

#include <cstdint>
#include <vector>

namespace helios {

/// A stage is a group of systems that can all execute in parallel.
struct ExecutionStage {
    std::vector<size_t> system_indices; // indices into the original SystemDescriptor vector
};

/// An execution plan is a topologically sorted sequence of stages.
struct ExecutionPlan {
    std::vector<ExecutionStage> stages;
};

/// Build an execution plan from a set of system descriptors.
///
/// Algorithm:
/// 1. Build adjacency list from access conflicts + explicit ordering.
/// 2. Topological sort (Kahn's algorithm).
/// 3. Group systems with equal topological depth into parallel stages.
///
/// Returns an empty plan if a cycle is detected (should not happen with
/// well-formed systems, but we handle it gracefully).
ExecutionPlan build_execution_plan(const std::vector<SystemDescriptor>& systems);

} // namespace helios

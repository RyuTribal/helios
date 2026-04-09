// helios-core/src/helios/ecs/transform_propagation.h
#pragma once

#include "helios/ecs/world.h"
#include "helios/components/components.h"

namespace helios {

/// Resource that tracks the last tick at which transform propagation ran.
/// Used as the change-detection threshold so we only recompute GlobalTransform
/// for entities whose Transform (or parent GlobalTransform) actually changed.
struct TransformPropagationState {
    uint32_t last_run_tick = 0;
};

/// Propagate the Transform hierarchy top-down, updating GlobalTransform.
///
/// Algorithm:
///   1. For every root entity (has Transform + GlobalTransform, no Parent):
///      if Transform changed since last propagation, recompute GlobalTransform.
///   2. Walk the Children hierarchy recursively. For each child:
///      if the parent's GlobalTransform was just updated OR the child's
///      Transform changed, recompute the child's GlobalTransform and recurse.
///      Otherwise skip the entire subtree (neither parent nor child changed).
///
/// Called directly from App::tick() after PostUpdate -- not a regular
/// parameterised system, because the recursive hierarchy walk needs
/// direct World access that cannot be expressed through Queries alone.
void propagate_transforms(World& world);

} // namespace helios

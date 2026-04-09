// helios-core/src/helios/ecs/transform_propagation.cpp
#include "helios/ecs/transform_propagation.h"

#include "helios/ecs/archetype.h"
#include "helios/ecs/archetype_storage.h"
#include "helios/ecs/component_id.h"
#include "helios/components/components.h"

namespace helios {

// -------------------------------------------------------------------------
// Recursive helper: propagate through children
// -------------------------------------------------------------------------

static void propagate_children(World& world,
                               const Children& children,
                               const glm::mat4& parent_global,
                               bool parent_changed,
                               uint32_t last_run_tick,
                               uint32_t current_tick)
{
    for (Entity child : children.entities) {
        if (!world.is_alive(child)) continue;

        auto loc = world.archetypes().locate(child);
        if (!loc.has_value()) continue;

        Archetype& arch = *loc->archetype;
        size_t row = loc->row;

        // Child must have both Transform and GlobalTransform.
        if (!arch.has_component(component_id<Transform>())) continue;
        if (!arch.has_component(component_id<GlobalTransform>())) continue;

        auto& t_col = arch.get_column<Transform>();
        auto& g_col = arch.get_column<GlobalTransform>();

        bool child_transform_changed = t_col.changed_tick(row) > last_run_tick;

        if (parent_changed || child_transform_changed) {
            auto& t = t_col.get<Transform>(row);
            auto& g = g_col.get<GlobalTransform>(row);
            g.matrix = parent_global * t.to_mat4();
            g_col.stamp(row, current_tick);

            // Recurse into grandchildren.
            auto* grandchildren = world.try_get<Children>(child);
            if (grandchildren) {
                propagate_children(world, *grandchildren, g.matrix,
                                   /*parent_changed=*/true,
                                   last_run_tick, current_tick);
            }
        } else {
            // Neither parent nor child changed -- skip this entire subtree.
            // (The Bevy 0.16 optimisation: unchanged subtrees are never visited.)
        }
    }
}

// -------------------------------------------------------------------------
// Top-level propagation entry point
// -------------------------------------------------------------------------

void propagate_transforms(World& world) {
    // Ensure the propagation state resource exists.
    if (!world.has_resource<TransformPropagationState>()) {
        world.insert_resource(TransformPropagationState{});
    }

    auto& state = world.resource<TransformPropagationState>();
    uint32_t last_run_tick = state.last_run_tick;

    // Advance the world tick so our stamps are distinguishable.
    world.advance_tick();
    uint32_t current_tick = world.current_tick();

    // Phase 1: Process root entities (have Transform + GlobalTransform, no Parent).
    world.archetypes().for_each_archetype([&](Archetype& arch) {
        if (!arch.has_component(component_id<Transform>())) return;
        if (!arch.has_component(component_id<GlobalTransform>())) return;
        if (arch.has_component(component_id<Parent>())) return;  // skip children

        auto& t_col = arch.get_column<Transform>();
        auto& g_col = arch.get_column<GlobalTransform>();

        for (size_t row = 0; row < arch.size(); ++row) {
            bool t_changed = t_col.changed_tick(row) > last_run_tick;

            if (t_changed) {
                auto& t = t_col.get<Transform>(row);
                auto& g = g_col.get<GlobalTransform>(row);
                g.matrix = t.to_mat4();
                g_col.stamp(row, current_tick);
            }

            // Phase 2: If this root entity has Children, propagate down.
            Entity entity = arch.entities[row];
            auto* ch = world.try_get<Children>(entity);
            if (ch) {
                // parent_changed is true if we just updated this root's GlobalTransform,
                // OR if the GlobalTransform was already stamped recently (e.g. by physics
                // writeback earlier this frame).
                bool g_changed = g_col.changed_tick(row) > last_run_tick;
                propagate_children(world, *ch,
                                   g_col.get<GlobalTransform>(row).matrix,
                                   g_changed,
                                   last_run_tick, current_tick);
            }
        }
    });

    // Update the propagation state so next frame uses this tick as threshold.
    state.last_run_tick = current_tick;
}

} // namespace helios

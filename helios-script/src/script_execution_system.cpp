#include "script_execution_system.h"
#include "hot_reload_system.h"

#include <helios/ecs/world.h>
#include <helios/ecs/time.h>
#include <helios/ecs/thread_pool.h>
#include <helios/script/script_runtime.h>
#include <helios/script/script_instance.h>
#include "interface/contact_event.h"
#include "script_log.h"

#include <unordered_map>
#include <vector>

namespace helios {

// -- Helper: pack Entity into uint64_t ----------------------------------------
static uint64_t entity_to_raw(Entity e) {
    return (static_cast<uint64_t>(e.generation) << 32) | static_cast<uint64_t>(e.index);
}

// -- script_create_system -----------------------------------------------------

void script_create_system(World& world) {
    auto* exec = world.try_resource<ScriptExecutionState>();
    if (!exec || !exec->running) return;
    auto* runtime = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime || !*runtime) return;

    // Query entities that have ScriptInstance but NOT ScriptInitialized
    // (i.e., newly added script components that need instance creation)
    auto q = world.query<ScriptInstance, Without<ScriptInitialized>>();

    for (auto [entity, script] : q.with_entity()) {
        if (script.script_class_name.empty()) continue;
        if (!(*runtime)->class_exists(script.script_class_name)) {
            HELIOS_LOG(Script, Warn,
                "Script class '{}' not found in loaded assembly",
                script.script_class_name);
            continue;
        }

        // Assign type ID
        script.script_type_id =
            (*runtime)->get_script_type_id(script.script_class_name);

        // Create managed instance
        uint64_t handle =
            (*runtime)->invoke_create(script.script_class_name, entity_to_raw(entity));

        if (handle == 0) {
            HELIOS_LOG(Script, Error,
                "Failed to create instance of '{}' — will not retry",
                script.script_class_name);
        }

        script.managed_handle = handle;

        // Mark as initialized even on failure (handle==0) so we don't retry every frame.
        // The execution system already skips entities with managed_handle==0.
        world.add(entity, ScriptInitialized{});
    }

    // Assets loaded in OnCreate() are async. They'll be available within
    // a frame or two via AssetServer caching. No blocking here — calling
    // wait_idle() would deadlock when this system runs on the thread pool.
}

// -- script_execution_system --------------------------------------------------

void script_execution_system(World& world) {
    auto* exec = world.try_resource<ScriptExecutionState>();
    if (!exec || !exec->running) return;
    auto* runtime = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime || !*runtime) return;

    auto& time = world.resource<Time>();
    float delta = time.delta();

    // Query all initialized script entities
    auto q = world.query<const ScriptInstance, With<ScriptInitialized>>();

    // Group by script_type_id
    struct BatchEntry {
        uint64_t entity_id;
        uint64_t managed_handle;
    };

    std::unordered_map<uint32_t, std::vector<BatchEntry>> batches;

    for (auto [entity, script] : q.with_entity()) {
        if (script.managed_handle == 0) continue;
        batches[script.script_type_id].push_back(
            {entity_to_raw(entity), script.managed_handle});
    }

    // Execute each batch
    for (auto& [type_id, entries] : batches) {
        std::vector<uint64_t> entity_ids;
        std::vector<uint64_t> handles;
        entity_ids.reserve(entries.size());
        handles.reserve(entries.size());

        for (auto& e : entries) {
            entity_ids.push_back(e.entity_id);
            handles.push_back(e.managed_handle);
        }

        (*runtime)->invoke_update(
            type_id,
            entity_ids.data(),
            handles.data(),
            entity_ids.size(),
            delta);
    }
}

// -- script_destroy_system ----------------------------------------------------

void script_destroy_system(World& world) {
    auto* runtime = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime || !*runtime) return;

    auto* tracker = world.try_resource<ScriptAliveTracker>();
    if (!tracker) return;

    std::unordered_map<uint64_t, uint64_t> current_alive;
    auto q = world.query<const ScriptInstance, With<ScriptInitialized>>();
    for (auto [entity, script] : q.with_entity()) {
        if (script.managed_handle == 0) continue;
        current_alive[entity_to_raw(entity)] = script.managed_handle;
    }

    // Entities in prev but not in current → despawned, call OnDestroy
    for (auto& [raw_id, handle] : tracker->prev_alive) {
        if (current_alive.find(raw_id) == current_alive.end()) {
            (*runtime)->invoke_destroy(raw_id, handle);
        }
    }

    tracker->prev_alive = std::move(current_alive);
}

// -- script_collision_dispatch_system -----------------------------------------

void script_collision_dispatch_system(World& world) {
    auto* exec = world.try_resource<ScriptExecutionState>();
    if (!exec || !exec->running) return;
    auto* runtime = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime || !*runtime) return;

    // Check if contact events are registered (physics plugin may not be loaded)
    if (!world.has_event<physics::ContactEvent>()) return;

    auto reader = world.event_reader<physics::ContactEvent>();
    if (reader.is_empty()) return;

    // Build a set of entity IDs that have script instances for fast lookup
    auto q = world.query<const ScriptInstance, With<ScriptInitialized>>();
    std::unordered_map<uint64_t, bool> scripted_entities;
    for (auto [entity, script] : q.with_entity()) {
        if (script.managed_handle != 0)
            scripted_entities[entity_to_raw(entity)] = true;
    }

    if (scripted_entities.empty()) return;

    // For each contact event, dispatch to scripted entities on both sides
    for (const auto& contact : reader) {
        if (scripted_entities.count(contact.entity_a)) {
            (*runtime)->invoke_on_collision(
                contact.entity_a, contact.entity_b,
                contact.world_point.x, contact.world_point.y, contact.world_point.z,
                contact.normal.x, contact.normal.y, contact.normal.z,
                contact.impulse);
        }
        if (scripted_entities.count(contact.entity_b)) {
            // For entity_b, the normal is reversed (points from B toward A)
            (*runtime)->invoke_on_collision(
                contact.entity_b, contact.entity_a,
                contact.world_point.x, contact.world_point.y, contact.world_point.z,
                -contact.normal.x, -contact.normal.y, -contact.normal.z,
                contact.impulse);
        }
    }
}

} // namespace helios

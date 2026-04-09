// helios-physics/src/interface/physics_plugin.h
#pragma once

#include <memory>
#include <type_traits>
#include <unordered_map>

#include "interface/physics_world.h"
#include "interface/contact_event.h"
#include "stub/stub_physics_world.h"

#include <helios/ecs/schedule.h>
#include <helios/ecs/system_params.h>
#include <helios/ecs/event_storage.h>
#include <helios/ecs/query.h>
#include <helios/ecs/entity.h>
#include <helios/components/components.h>

namespace helios::physics {

// ============================================================
// PhysicsBodyMap resource
//
// Game code registers entity↔body associations here so that the
// automatic sync systems can push/pull transforms.  Call
// register_body() after create_body(), unregister_body() before
// destroy_body().
// ============================================================

struct PhysicsBodyMapEntry {
    BodyHandle  handle    = 0;
    BodyType    body_type = BodyType::Static;
};

struct PhysicsBodyMap {
    // Key: combined 64-bit entity identity (generation<<32 | index)
    std::unordered_map<uint64_t, PhysicsBodyMapEntry> entries;

    static uint64_t key(helios::Entity e) noexcept {
        return (static_cast<uint64_t>(e.generation) << 32) | e.index;
    }

    void register_body(helios::Entity entity, BodyHandle handle, BodyType type) {
        entries[key(entity)] = PhysicsBodyMapEntry{handle, type};
    }

    void unregister_body(helios::Entity entity) {
        entries.erase(key(entity));
    }
};

// ============================================================
// Physics systems (free functions, registered by the plugin)
// ============================================================

// Schedule: FixedUpdate
// Step the simulation and emit contact events for the current tick.
inline void physics_step(helios::ResMut<std::unique_ptr<PhysicsWorld>> world,
                         helios::Res<PhysicsConfig> config,
                         helios::EventWriter<ContactEvent> contacts_out) {
    if (!*world) return;

    (*world)->step(config->fixed_timestep);

    auto contacts = (*world)->drain_contacts();
    for (auto& c : contacts) {
        contacts_out.send(c);
    }
}

// Schedule: PreUpdate (after poll_window_events)
// Push ECS Transform → physics body for Kinematic bodies.
// This lets game code drive kinematic bodies by setting Transform.position/rotation.
inline void sync_ecs_to_physics(
    helios::Res<std::unique_ptr<PhysicsWorld>> world,
    helios::Res<PhysicsBodyMap>                body_map,
    helios::Query<const helios::Transform>     transforms)
{
    if (!*world) return;

    for (const auto& [k, entry] : body_map->entries) {
        if (entry.body_type != BodyType::Kinematic) continue;

        // Reconstruct entity from map key
        helios::Entity entity;
        entity.index      = static_cast<uint32_t>(k & 0xFFFF'FFFFu);
        entity.generation = static_cast<uint32_t>(k >> 32);

        auto result = transforms.get(entity);
        if (!result.has_value()) continue;

        const auto& [t] = *result;
        (*world)->set_transform(entry.handle, t.position, t.rotation);
    }
}

// Schedule: PostUpdate
// Read physics body position/rotation → ECS Transform for Dynamic bodies.
inline void sync_physics_to_ecs(
    helios::Res<std::unique_ptr<PhysicsWorld>> world,
    helios::Res<PhysicsBodyMap>                body_map,
    helios::Query<helios::Transform>           transforms)
{
    if (!*world) return;

    for (const auto& [k, entry] : body_map->entries) {
        if (entry.body_type != BodyType::Dynamic) continue;

        helios::Entity entity;
        entity.index      = static_cast<uint32_t>(k & 0xFFFF'FFFFu);
        entity.generation = static_cast<uint32_t>(k >> 32);

        auto result = transforms.get(entity);
        if (!result.has_value()) continue;

        auto& [t] = *result;
        t.position = (*world)->get_position(entry.handle);
        t.rotation = (*world)->get_rotation(entry.handle);
    }
}

// ============================================================
// PhysicsPlugin
// ============================================================

// Backend concept: must derive from PhysicsWorld.
//
// Example backends:
//   StubPhysicsWorld  -- no-op (no external dependency)
//   JoltPhysicsWorld  -- production (Jolt Physics, if available)

template<typename Backend>
struct PhysicsPlugin {
    static_assert(std::is_base_of_v<PhysicsWorld, Backend>,
                  "Physics backend must derive from PhysicsWorld");

    PhysicsConfig config;  // User can customize before adding plugin

    void build(auto& app) {
        // Insert physics configuration resource
        app.insert_resource(PhysicsConfig{config});

        // Create and insert the physics world backend
        auto world = std::make_unique<Backend>(config);
        app.template insert_resource<std::unique_ptr<PhysicsWorld>>(std::move(world));

        // Insert the entity↔body mapping resource (empty; game code populates it)
        app.insert_resource(PhysicsBodyMap{});

        // Register collision event type
        app.template add_event<ContactEvent>();

        // Step the physics world each fixed-update tick
        app.add_system(helios::Schedule::FixedUpdate, physics_step, "physics_step");

        // Kinematic: ECS Transform → physics body (runs before game Update)
        app.add_system(helios::Schedule::PreUpdate, sync_ecs_to_physics, "sync_ecs_to_physics");

        // Dynamic: physics body → ECS Transform (runs after game Update / physics step)
        app.add_system(helios::Schedule::PostUpdate, sync_physics_to_ecs, "sync_physics_to_ecs");
    }
};

// Convenience alias for stub use
using StubPhysicsPlugin = PhysicsPlugin<StubPhysicsWorld>;

} // namespace helios::physics

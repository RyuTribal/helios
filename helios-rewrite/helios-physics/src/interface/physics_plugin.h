// helios-physics/src/interface/physics_plugin.h
#pragma once

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "interface/physics_world.h"
#include "interface/contact_event.h"

#include <helios/ecs/schedule.h>
#include <helios/ecs/system_params.h>
#include <helios/ecs/event_storage.h>
#include <helios/ecs/query.h>
#include <helios/ecs/query_filters.h>
#include <helios/ecs/entity.h>
#include <helios/ecs/commands.h>
#include <helios/components/components.h>

namespace helios::physics {

// ============================================================
// Physics ECS components (defined here because they depend on
// physics types like ColliderShape and BodyHandle)
// ============================================================

/// Collider component: wraps a ColliderShape variant.
/// Attach to an entity alongside RigidBody to have the physics
/// plugin automatically create a physics body.
struct Collider {
    ColliderShape shape = BoxShape{};
};

/// Marker component added by the physics plugin after it creates
/// a body for an entity. Stores the physics BodyHandle so sync
/// systems can look it up. Also prevents duplicate creation.
struct PhysicsBody {
    BodyHandle handle = 0;
};

// ============================================================
// PhysicsBodyMap resource
//
// Tracks entity-to-body associations so that the automatic sync
// systems can push/pull transforms. Populated automatically by
// the auto-create system; game code no longer needs to call
// register_body() / unregister_body() manually.
// ============================================================

struct PhysicsBodyMapEntry {
    BodyHandle  handle    = 0;
    BodyType    body_type = BodyType::Static;
    uint32_t    last_physics_write_tick = 0;  // tick when physics last wrote Transform
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
// Helper: map helios::BodyType (core enum) to physics::BodyType
// ============================================================

inline BodyType to_physics_body_type(helios::BodyType t) {
    switch (t) {
        case helios::BodyType::Static:    return BodyType::Static;
        case helios::BodyType::Kinematic: return BodyType::Kinematic;
        case helios::BodyType::Dynamic:   return BodyType::Dynamic;
    }
    return BodyType::Static;
}

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

// Schedule: PreUpdate
// Automatically create physics bodies for entities that have
// RigidBody + Collider but not yet a PhysicsBody marker.
inline void physics_auto_create(
    helios::ResMut<std::unique_ptr<PhysicsWorld>> world,
    helios::ResMut<PhysicsBodyMap>                body_map,
    helios::Query<const helios::Transform,
                  const helios::RigidBody,
                  const Collider,
                  helios::Without<PhysicsBody>>   new_bodies,
    helios::Commands&                             cmds)
{
    if (!*world) return;

    for (auto [entity, t, rb, col] : new_bodies.with_entity()) {
        BodyDesc desc;
        desc.type        = to_physics_body_type(rb.body_type);
        desc.position    = t.position;
        desc.rotation    = t.rotation;
        desc.mass        = rb.mass;
        desc.friction    = rb.friction;
        desc.restitution = rb.restitution;
        desc.shape       = col.shape;

        uint64_t entity_key = PhysicsBodyMap::key(entity);
        auto handle = (*world)->create_body(desc, entity_key);
        body_map->register_body(entity, handle, desc.type);
        cmds.insert(entity, PhysicsBody{handle});
    }
}

// Schedule: PreUpdate (after auto_create)
// Push ECS GlobalTransform -> physics body when the user (not physics) changed it.
// Handles both Kinematic and Dynamic bodies:
//   - Kinematic: always push when GlobalTransform changed (user drives these)
//   - Dynamic: only push when the change was NOT from physics writeback
//     (detected by comparing Transform changed_tick vs last_physics_write_tick)
//
// For Dynamic bodies, this also resets linear velocity to zero when the user
// teleports the body (e.g., R-key reset), preventing the body from continuing
// with its old momentum.
inline void sync_ecs_to_physics(
    helios::ResMut<std::unique_ptr<PhysicsWorld>> world,
    helios::Res<PhysicsBodyMap>                   body_map,
    helios::Query<const helios::GlobalTransform,
                  helios::Changed<helios::GlobalTransform>> changed_globals)
{
    if (!*world) return;

    for (const auto& [k, entry] : body_map->entries) {
        if (entry.body_type == BodyType::Static) continue;

        helios::Entity entity;
        entity.index      = static_cast<uint32_t>(k & 0xFFFF'FFFFu);
        entity.generation = static_cast<uint32_t>(k >> 32);

        auto result = changed_globals.get(entity);
        if (!result.has_value()) continue;

        // For Dynamic bodies: skip if the change was from physics writeback
        if (entry.body_type == BodyType::Dynamic) {
            // The GlobalTransform changed, but was it because propagation picked up
            // the physics writeback's Transform change? If the Transform's tick matches
            // last_physics_write_tick, physics caused this — skip to avoid feedback.
            // We use a simple heuristic: if the write tick is recent (within last 2 ticks),
            // it's from physics. Otherwise, the user explicitly moved the body.
            if (entry.last_physics_write_tick > 0 &&
                changed_globals.current_tick() - entry.last_physics_write_tick <= 2) {
                continue;  // physics caused this change, skip
            }
        }

        const auto& [gt] = *result;
        glm::vec3 pos = glm::vec3(gt.matrix[3]);
        glm::quat rot = glm::quat_cast(glm::mat3(gt.matrix));
        (*world)->set_transform(entry.handle, pos, rot);

        // If user teleported a dynamic body, zero velocity to prevent old momentum
        if (entry.body_type == BodyType::Dynamic) {
            (*world)->set_velocity(entry.handle, glm::vec3{0.0f});
        }
    }
}

// Schedule: PostUpdate
// Read physics body position/rotation -> ECS Transform for Dynamic bodies.
// Records the write tick so sync_ecs_to_physics can distinguish user changes
// from physics writeback (prevents feedback loop).
inline void sync_physics_to_ecs(
    helios::Res<std::unique_ptr<PhysicsWorld>> world,
    helios::ResMut<PhysicsBodyMap>             body_map,
    helios::Query<helios::Transform>           transforms)
{
    if (!*world) return;

    for (auto& [k, entry] : body_map->entries) {
        if (entry.body_type != BodyType::Dynamic) continue;

        helios::Entity entity;
        entity.index      = static_cast<uint32_t>(k & 0xFFFF'FFFFu);
        entity.generation = static_cast<uint32_t>(k >> 32);

        auto result = transforms.get(entity);
        if (!result.has_value()) continue;

        auto& [t] = *result;
        t.position = (*world)->get_position(entry.handle);
        t.rotation = (*world)->get_rotation(entry.handle);
        entry.last_physics_write_tick = transforms.current_tick();
    }
}

// Schedule: PostUpdate
// Clean up physics bodies for entities that have been despawned.
// Iterates the body map and removes entries whose entities are no
// longer in the ECS (stale entries).
inline void physics_auto_destroy(
    helios::ResMut<std::unique_ptr<PhysicsWorld>> world,
    helios::ResMut<PhysicsBodyMap>                body_map,
    helios::Query<const helios::Transform>        all_transforms)
{
    if (!*world) return;

    // Collect stale keys first to avoid mutating the map during iteration.
    std::vector<uint64_t> stale_keys;
    for (const auto& [k, entry] : body_map->entries) {
        helios::Entity entity;
        entity.index      = static_cast<uint32_t>(k & 0xFFFF'FFFFu);
        entity.generation = static_cast<uint32_t>(k >> 32);

        // If the entity no longer exists (Transform query lookup fails), it was despawned.
        if (!all_transforms.get(entity).has_value()) {
            stale_keys.push_back(k);
        }
    }

    for (uint64_t k : stale_keys) {
        auto it = body_map->entries.find(k);
        if (it != body_map->entries.end()) {
            (*world)->destroy_body(it->second.handle);
            body_map->entries.erase(it);
        }
    }
}

// ============================================================
// PhysicsPlugin
// ============================================================

// Backend concept: must derive from PhysicsWorld.
//
// Use JoltPhysicsWorld (the only backend) or DefaultPhysicsPlugin for convenience.

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

        // Insert the entity-to-body mapping resource (populated automatically)
        app.insert_resource(PhysicsBodyMap{});

        // Register collision event type
        app.template add_event<ContactEvent>();

        // Auto-create bodies from RigidBody + Collider components (runs first in PreUpdate)
        app.add_system(helios::Schedule::PreUpdate, physics_auto_create, "physics_auto_create");

        // Step the physics world each fixed-update tick
        app.add_system(helios::Schedule::FixedUpdate, physics_step, "physics_step");

        // Kinematic: ECS Transform -> physics body (runs in PreUpdate after auto-create)
        app.add_system(helios::Schedule::PreUpdate, sync_ecs_to_physics, "sync_ecs_to_physics");

        // Dynamic: physics body -> ECS Transform (runs after game Update / physics step)
        app.add_system(helios::Schedule::PostUpdate, sync_physics_to_ecs, "sync_physics_to_ecs");

        // Clean up bodies for despawned entities
        app.add_system(helios::Schedule::PostUpdate, physics_auto_destroy, "physics_auto_destroy");
    }
};

} // namespace helios::physics

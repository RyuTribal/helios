// helios-physics/src/interface/physics_plugin.h
#pragma once

#include <memory>
#include <type_traits>

#include "interface/physics_world.h"
#include "interface/contact_event.h"
#include "stub/stub_physics_world.h"

#include <helios/ecs/schedule.h>
#include <helios/ecs/system_params.h>
#include <helios/ecs/event_storage.h>

namespace helios::physics {

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

        // Register collision event type
        app.template add_event<ContactEvent>();

        // Step the physics world each fixed-update tick
        app.add_system(helios::Schedule::FixedUpdate, physics_step, "physics_step");

        // Full ECS-driven transform sync (uncomment when wiring to components):
        // app.add_system(helios::Schedule::PostUpdate,  sync_physics_transforms, "sync_physics_transforms");
        // app.add_system(helios::Schedule::PostUpdate,  push_kinematic_transforms, "push_kinematic_transforms");
    }
};

// Convenience alias for stub use
using StubPhysicsPlugin = PhysicsPlugin<StubPhysicsWorld>;

} // namespace helios::physics

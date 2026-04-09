// helios-physics/src/interface/physics_factory.h
//
// Factory function that creates a JoltPhysicsWorld.
// This header only exposes the base interface; Jolt headers are included
// only in the .cpp.
#pragma once

#include <memory>
#include "interface/physics_world.h"

namespace helios::physics {

/// Create a PhysicsWorld backed by Jolt Physics.
std::unique_ptr<PhysicsWorld> create_physics_world(const PhysicsConfig& config = {});

} // namespace helios::physics

#include "interface/physics_plugin.h"

namespace helios::physics {

/// Plugin that uses the best available backend automatically.
/// No backend knowledge needed — just add_plugin(DefaultPhysicsPlugin{}).
struct DefaultPhysicsPlugin {
    PhysicsConfig config;

    void build(auto& app) {
        app.insert_resource(PhysicsConfig{config});
        app.template insert_resource<std::unique_ptr<PhysicsWorld>>(
            create_physics_world(config));
        app.insert_resource(PhysicsBodyMap{});
        app.template add_event<ContactEvent>();
        app.add_system(helios::Schedule::FixedUpdate, physics_step, "physics_step");
        app.add_system(helios::Schedule::PreUpdate, physics_auto_create, "physics_auto_create");
        app.add_system(helios::Schedule::PreUpdate, sync_ecs_to_physics, "sync_ecs_to_physics");
        app.add_system(helios::Schedule::PostUpdate, sync_physics_to_ecs, "sync_physics_to_ecs");
        app.add_system(helios::Schedule::PostUpdate, physics_auto_destroy, "physics_auto_destroy");
    }
};

} // namespace helios::physics

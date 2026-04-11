# PhysicsPlugin API Reference

The `PhysicsPlugin` handles the registration of physics-related resources and systems within the Helios `App`. It is a template class that accepts a `PhysicsWorld` backend implementation.

## Overview

```cpp
#include <helios/physics/interface/physics_plugin.h>
#include <helios/physics/jolt/jolt_physics_world.h>

app.add_plugin(helios::physics::PhysicsPlugin<helios::physics::JoltPhysicsWorld>{});
```

## Configuration

The plugin is configured using the `PhysicsConfig` structure. You can customize the physics world settings before adding the plugin:

```cpp
helios::physics::PhysicsConfig config;
config.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
config.fixed_timestep = 1.0f / 120.0f; // Higher precision

app.add_plugin(helios::physics::PhysicsPlugin<JoltPhysicsWorld>{config});
```

### PhysicsConfig Members

| Member | Description | Default |
| :--- | :--- | :--- |
| `fixed_timestep` | The time step for each simulation step. | `1.0 / 60.0` |
| `gravity` | Global gravity vector in world-space. | `(0.0, -9.81, 0.0)` |
| `collision_steps` | Number of sub-steps for collision resolution. | `1` |

## Registered Systems

When the plugin is added, it automatically registers several systems into the `App` schedule:

| System | Schedule | Description |
| :--- | :--- | :--- |
| `physics_auto_create` | `PreUpdate` | Automatically creates physics bodies for entities with `RigidBody` and `Collider` components. |
| `physics_step` | `FixedUpdate` | Advances the simulation and emits contact events. |
| `sync_ecs_to_physics` | `PreUpdate` | Pushes ECS `Transform` changes into the physics world. Handles both Kinematic and user-driven Dynamic teleportation. |
| `sync_physics_to_ecs` | `PostUpdate` | Pulls the simulated `Transform` from the physics world back into the ECS. |
| `physics_auto_destroy` | `PostUpdate` | Cleans up physics bodies when entities are despawned. |

## Event Handling

The `PhysicsPlugin` also adds the `ContactEvent` type. You can listen for collision events in any system:

```cpp
void on_collision(helios::EventReader<helios::physics::ContactEvent> events) {
    for (const auto& ev : events) {
        HELIOS_LOG(Physics, Info, "Collision between entity {} and {}", ev.entity_a, ev.entity_b);
    }
}
```

## Related Pages

- [Physics API Overview](index.md)
- [PhysicsWorld API](physics_world.md)

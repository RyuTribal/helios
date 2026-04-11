# PhysicsWorld API Reference

The `PhysicsWorld` class provides an abstract interface for the physics simulation. It manages the lifecycle of physics bodies, performs simulation steps, and handles spatial queries like raycasting.

## Overview

A `PhysicsWorld` is typically created and managed by the `PhysicsPlugin`. It is available as a world resource in the ECS:

```cpp
#include <helios/physics/interface/physics_world.h>

void my_system(helios::ResMut<std::unique_ptr<helios::physics::PhysicsWorld>> world) {
    if (*world) {
        // Direct simulation access...
    }
}
```

## Public Methods

### Body Lifecycle

| Method | Description |
| :--- | :--- |
| `create_body(const BodyDesc&, uint64_t)` | Creates a new physics body. Returns a `BodyHandle`. |
| `destroy_body(BodyHandle)` | Destroys an existing body. The handle becomes invalid. |

### Transform Management

| Method | Description |
| :--- | :--- |
| `set_transform(BodyHandle, pos, rot)` | Manually sets a body's world-space position and rotation. |
| `get_position(BodyHandle)` | Returns the current world-space position of a body. |
| `get_rotation(BodyHandle)` | Returns the current world-space rotation of a body. |

### Simulation Control

| Method | Description |
| :--- | :--- |
| `step(float dt)` | Advances the simulation by `dt` seconds. |
| `drain_contacts()` | Returns all contact events since the last step and clears the internal buffer. |
| `set_gravity(vec3)` | Updates the global gravity vector. |

### Forces & Impulses

| Method | Description |
| :--- | :--- |
| `set_velocity(BodyHandle, vec3)` | Sets the linear velocity of a body. |
| `get_velocity(BodyHandle)` | Returns the current linear velocity. |
| `set_angular_velocity(BodyHandle, vec3)` | Sets the angular velocity of a body. |
| `get_angular_velocity(BodyHandle)` | Returns the current angular velocity. |
| `apply_force(BodyHandle, vec3)` | Applies a force to the center of mass. |
| `apply_impulse(BodyHandle, vec3)` | Applies an instantaneous impulse to the center of mass. |
| `apply_torque(BodyHandle, vec3)` | Applies a torque to the body. |

### Raycasting

| Method | Description |
| :--- | :--- |
| `raycast(origin, direction, max_dist)` | Returns the closest `RayHit`, or `nullopt`. |
| `raycast_all(origin, direction, max_dist)` | Returns all `RayHit`s sorted by distance. |

## Data Structures

### RayHit

Returned by raycasting methods.

- `entity`: The ID of the ECS entity associated with the hit body.
- `point`: World-space intersection point.
- `normal`: Surface normal at the hit point.
- `distance`: Distance from the ray origin.

## Code Snippet: Raycasting

```cpp
#include <helios/physics/interface/physics_world.h>

void perform_aim_raycast(const helios::physics::PhysicsWorld& world) {
    glm::vec3 origin{0.0f, 1.0f, 0.0f};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    float max_dist = 100.0f;

    if (auto hit = world.raycast(origin, direction, max_dist)) {
        HELIOS_LOG(Physics, Info, "Hit entity {} at distance {}", hit->entity, hit->distance);
    }
}
```

## Related Pages

- [Physics API Overview](index.md)
- [Physics Components](components.md)

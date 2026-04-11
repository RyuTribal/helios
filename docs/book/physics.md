# Physics

## Overview

Helios uses **Jolt Physics** for its 3D physics simulation. The `helios-physics` library provides an abstract `PhysicsWorld` interface, allowing the engine to remain decoupled from the specific backend while taking advantage of Jolt's performance and stability.

The `PhysicsPlugin` handles all ECS integration automatically. By simply attaching `RigidBody` and `Collider` components to an entity, the engine will manage body creation, synchronization, and destruction.

## Setup

To enable physics, add the `PhysicsPlugin` with the `JoltPhysicsWorld` implementation to your application:

```cpp
#include <helios/physics/physics_plugin.h>
#include <helios/physics/jolt_physics_world.h>

app.add_plugin(helios::physics::PhysicsPlugin<helios::physics::JoltPhysicsWorld>{
    .config = {
        .fixed_timestep = 1.0f / 60.0f,
        .gravity = {0.0f, -9.81f, 0.0f},
        .collision_steps = 1,
    }
});
```

## Fixed Update and Timestepping

Physics simulations in Helios run in the `FixedUpdate` schedule. This ensures that the simulation is deterministic and independent of the rendering frame rate.

The engine uses a `FixedTimeAccumulator` resource to track elapsed time. If the frame rate is high, `physics_step` may not run at all in a frame. If the frame rate is low, it may run multiple times to catch up to real-time.

```cpp
// Internal physics step system
void physics_step(ResMut<std::unique_ptr<PhysicsWorld>> world,
                  Res<PhysicsConfig> config,
                  EventWriter<ContactEvent> contacts_out) {
    if (!*world) return;

    float dt = config->fixed_timestep;
    world->step(dt);
    
    // Drain and emit contact events
    auto contacts = world->drain_contacts();
    for (auto& event : contacts) {
        contacts_out.send(event);
    }
}
```

## RigidBody Component

The `RigidBody` component defines how an entity interacts with the physical world. It supports three primary body types:

- **Static**: Non-movable objects like terrain or walls. They have zero mass and do not respond to forces.
- **Dynamic**: Objects that respond to gravity, forces, and impulses. They provide full physical simulation.
- **Kinematic**: Objects moved manually via code (e.g., moving platforms). They push dynamic bodies but are not affected by forces themselves.

### Usage Example

```cpp
struct RigidBody {
    BodyType body_type = BodyType::Dynamic;
    BodyHandle handle;
    float    mass        = 1.0f;
    float    friction    = 0.5f;
    float    restitution = 0.3f;
    uint32_t flags       = RigidBodyFlags::UseGravity;
};
```

## Collider Components

A `RigidBody` must be paired with a `Collider` component to have a physical shape. Helios provides several primitive collider components:

### Supported Colliders
- **BoxCollider**: Defined by `half_extents` and `offset`.
- **SphereCollider**: Defined by `radius` and `offset`.

### Setting up a Dynamic Body

```cpp
world.spawn(
    Transform{.position = {0, 10, 0}},
    RigidBody{
        .body_type = BodyType::Dynamic,
        .mass = 5.0f,
        .friction = 0.3f,
        .flags = RigidBodyFlags::UseGravity
    },
    BoxCollider{
        .half_extents = {0.5f, 0.5f, 0.5f}
    }
);
```

## Interacting with the Physics World

For advanced interactions like applying forces or raycasting, use the `PhysicsWorld` resource.

### Applying Forces and Impulses

```cpp
void apply_explosion(World& world, Entity entity, glm::vec3 explosion_origin) {
    auto& physics = *world.resource<std::unique_ptr<physics::PhysicsWorld>>();
    auto* rb = world.try_get<RigidBody>(entity);
    
    if (rb && rb->handle) {
        glm::vec3 pos = physics.get_position(rb->handle);
        glm::vec3 dir = glm::normalize(pos - explosion_origin);
        
        // Instant change in velocity
        physics.apply_impulse(rb->handle, dir * 10.0f);
        
        // Continuous force (should be called in FixedUpdate)
        physics.apply_force(rb->handle, dir * 5.0f);
    }
}
```

### Raycasting

Raycasting allows you to query the physics world for intersections along a line.

```cpp
auto& physics = *world.resource<std::unique_ptr<physics::PhysicsWorld>>();

glm::vec3 origin = {0, 5, 0};
glm::vec3 direction = {0, -1, 0};
float max_distance = 10.0f;

auto hit = physics.raycast(origin, direction, max_distance);
if (hit) {
    HELIOS_LOG(Physics, Info, "Hit entity {} at distance {}", hit->entity, hit->distance);
}
```

## Handling Contact Events

Collision events are emitted during the physics step and can be consumed by any system using an `EventReader<ContactEvent>`.

```cpp
void on_collision(EventReader<physics::ContactEvent> contacts) {
    for (const auto& event : contacts) {
        HELIOS_LOG(Physics, Info, "Collision between {} and {} at point {}", 
                   event.entity_a, event.entity_b, event.world_point);
    }
}
```

The `ContactEvent` struct contains:
- `entity_a`, `entity_b`: The two entities involved.
- `world_point`: The contact point in world space.
- `normal`: The contact normal.
- `impulse`: The magnitude of the impulse applied.

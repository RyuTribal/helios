# Physics

## Overview

Physics is provided by the `helios-physics` library, which wraps Jolt Physics
behind an abstract `PhysicsWorld` interface. The `PhysicsPlugin` handles all
ECS integration automatically -- attach `RigidBody` + `Collider` components
and bodies are created, synced, and destroyed without manual calls.

## Setup

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

The plugin inserts these resources and systems:

| Resource | Type |
|---|---|
| `PhysicsConfig` | Timestep, gravity, collision steps |
| `PhysicsWorld` | `std::unique_ptr<PhysicsWorld>` -- the simulation backend |
| `PhysicsBodyMap` | Entity-to-body handle mapping |

| System | Schedule | Purpose |
|---|---|---|
| `physics_auto_create` | PreUpdate | Creates bodies for new RigidBody + Collider entities |
| `physics_collider_cleanup` | PreUpdate | Destroys bodies when Collider or RigidBody is removed |
| `sync_ecs_to_physics` | PreUpdate | Pushes ECS transforms to physics (Kinematic + user-moved Dynamic) |
| `physics_step` | FixedUpdate | Steps the simulation, drains contact events |
| `sync_physics_to_ecs` | PostUpdate | Writes physics transforms back to ECS (Dynamic bodies) |
| `physics_auto_destroy` | PostUpdate | Cleans up bodies for despawned entities |

## Body Types

```cpp
enum class BodyType : uint8_t {
    Static,     // Non-movable. Placed once. Zero mass.
    Dynamic,    // Responds to forces, gravity, impulses.
    Kinematic,  // Moved by code. Pushes dynamic bodies. Ignores forces.
};
```

Assign via the `RigidBody` component:

```cpp
world.spawn(
    Transform{.position = {0, 5, 0}},
    RigidBody{
        .body_type = BodyType::Dynamic,
        .mass = 2.0f,
        .friction = 0.6f,
        .restitution = 0.3f,
        .flags = RigidBodyFlags::UseGravity,
    },
    physics::Collider{.shape = physics::BoxShape{.half_extents = {0.5f, 0.5f, 0.5f}}}
);
```

## Collider Shapes

The `Collider` component holds a `ColliderShape` variant:

```cpp
struct Collider {
    ColliderShape shape;  // BoxShape | SphereShape | CapsuleShape
};
```

### Available shapes

```cpp
struct BoxShape {
    glm::vec3 half_extents{0.5f};
};

struct SphereShape {
    float radius = 0.5f;
};

struct CapsuleShape {
    float half_height = 0.5f;
    float radius = 0.25f;
};
```

### Using colliders

```cpp
// Box
physics::Collider{.shape = physics::BoxShape{.half_extents = {1, 0.5f, 2}}}

// Sphere
physics::Collider{.shape = physics::SphereShape{.radius = 1.0f}}

// Capsule
physics::Collider{.shape = physics::CapsuleShape{.half_height = 0.75f, .radius = 0.3f}}
```

## Automatic Body Creation

You do not create physics bodies manually. The `physics_auto_create` system
(PreUpdate) queries for entities that have `Transform + RigidBody + Collider`
but no `PhysicsBody` marker. For each match it:

1. Creates a body in the `PhysicsWorld` from the component data.
2. Registers the entity-to-body mapping in `PhysicsBodyMap`.
3. Adds a `PhysicsBody` marker component via deferred `Commands`.

The `PhysicsBody` marker prevents re-creation on subsequent frames.

### Automatic cleanup

- **Component removal**: If `Collider` or `RigidBody` is removed from an
  entity that has `PhysicsBody`, `physics_collider_cleanup` destroys the
  body and removes the marker.
- **Entity despawn**: `physics_auto_destroy` checks all mapped entities each
  frame and destroys bodies for entities that no longer exist.

## Transform Synchronization

### ECS to Physics (`sync_ecs_to_physics`)

Runs in `PreUpdate`. For Kinematic bodies, always pushes the
`GlobalTransform` to the physics body. For Dynamic bodies, only pushes
when the user explicitly changed the transform (not when physics wrote it
back). Detected via change-tick comparison.

When a Dynamic body is teleported by user code, velocity is zeroed to
prevent the old momentum from carrying over.

### Physics to ECS (`sync_physics_to_ecs`)

Runs in `PostUpdate`. Reads the physics body's position and rotation and
writes them into the entity's `Transform` component. Only applies to
Dynamic bodies.

**Limitation**: Physics bodies must be root entities (no parent). Parented
entities will have their local transform overwritten with world-space values.

## Contact Events

Contact events are emitted as ECS events during `physics_step`:

```cpp
struct ContactEvent {
    uint64_t   entity_a;       // Entity ID of first body
    uint64_t   entity_b;       // Entity ID of second body
    glm::vec3  world_point;    // Contact point in world space
    glm::vec3  normal;         // Contact normal (from A toward B)
    float      impulse;        // Normal impulse magnitude
};
```

Read them in any system:

```cpp
void on_collision(EventReader<physics::ContactEvent> contacts) {
    for (const auto& c : contacts) {
        // c.entity_a, c.entity_b, c.world_point, c.impulse
    }
}
```

The C# scripting layer routes these to `ScriptBehaviour.OnCollisionEnter()`.

## PhysicsWorld Interface

The abstract interface exposed to systems:

```cpp
class PhysicsWorld {
public:
    // Body lifecycle
    virtual BodyHandle create_body(const BodyDesc& desc, uint64_t entity_id = 0) = 0;
    virtual void destroy_body(BodyHandle handle) = 0;

    // Transform
    virtual void set_transform(BodyHandle, const glm::vec3& pos, const glm::quat& rot) = 0;
    virtual glm::vec3 get_position(BodyHandle) const = 0;
    virtual glm::quat get_rotation(BodyHandle) const = 0;

    // Velocity / forces
    virtual void set_velocity(BodyHandle, const glm::vec3& linear) = 0;
    virtual glm::vec3 get_velocity(BodyHandle) const = 0;
    virtual void apply_force(BodyHandle, const glm::vec3& force) = 0;
    virtual void apply_impulse(BodyHandle, const glm::vec3& impulse) = 0;
    virtual void apply_torque(BodyHandle, const glm::vec3& torque) = 0;

    // Simulation
    virtual void step(float dt) = 0;
    virtual std::vector<ContactEvent> drain_contacts() = 0;

    // Raycasting
    virtual std::optional<RayHit> raycast(const glm::vec3& origin,
                                           const glm::vec3& direction,
                                           float max_distance) const = 0;
    virtual std::vector<RayHit> raycast_all(...) const = 0;
};
```

### Raycasting

```cpp
auto& physics = *world.resource<std::unique_ptr<physics::PhysicsWorld>>();

// Closest hit
auto hit = physics.raycast(origin, direction, 100.0f);
if (hit) {
    // hit->entity, hit->point, hit->normal, hit->distance
}

// All hits sorted by distance
auto hits = physics.raycast_all(origin, direction, 100.0f);
```

## Editor Integration

In Edit mode, the editor sets the `PhysicsWorld` pointer to null, which
pauses all physics systems (every system guards with
`if (!*world) return;`). When entering Play mode, a new `PhysicsWorld` is
created and bodies are rebuilt from the current ECS state.

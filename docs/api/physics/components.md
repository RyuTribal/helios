# Physics Components API Reference

The Helios Physics module utilizes a set of ECS components to define physical properties and shapes of entities.

## User-Facing Components

These components are defined in the core `helios::components` namespace and are used by developers to define an entity's physical presence.

### RigidBody

The `RigidBody` component defines how an entity participates in the physics simulation.

| Member | Type | Description |
| :--- | :--- | :--- |
| `body_type` | `BodyType` | `Static`, `Dynamic`, or `Kinematic`. |
| `mass` | `float` | Mass of the body in kilograms. |
| `friction` | `float` | Surface friction (0.0 to 1.0). |
| `restitution` | `float` | Bounciness (0.0 to 1.0). |
| `flags` | `uint32_t` | `RigidBodyFlags::UseGravity`, etc. |

### BoxCollider

Defines a box-shaped collision volume.

- `half_extents`: A `vec3` representing half the dimensions of the box.
- `offset`: Local-space offset from the entity's origin.
- `flags`: `ColliderFlags::IsTrigger`, etc.

### SphereCollider

Defines a sphere-shaped collision volume.

- `radius`: Radius of the sphere.
- `offset`: Local-space offset from the entity's origin.
- `flags`: `ColliderFlags::IsTrigger`, etc.

## Backend Components

These components are internal to the `helios::physics` namespace and are managed by the `PhysicsPlugin`.

### Collider

A wrapper around a `ColliderShape` variant (Box, Sphere, or Capsule). The `PhysicsPlugin` automatically generates this from user-facing collider components.

### PhysicsBody

A marker component containing the backend `BodyHandle`.

## ContactEvent

Generated when two physics bodies collide. It is accessible via an `EventReader<ContactEvent>`.

- `entity_a`: The first entity ID involved in the collision.
- `entity_b`: The second entity ID involved in the collision.
- `position`: The world-space coordinates of the contact point.
- `normal`: The contact normal vector.
- `impulse`: The magnitude of the impulse applied.

## Example: Creating a Physics Entity

```cpp
auto entity = cmds.spawn();
cmds.insert(entity, helios::Transform{ .position = {0, 10, 0} });
cmds.insert(entity, helios::RigidBody{ .body_type = BodyType::Dynamic, .mass = 5.0f });
cmds.insert(entity, helios::BoxCollider{ .half_extents = {0.5f, 0.5f, 0.5f} });
```

## Related Pages

- [Physics API Overview](index.md)
- [PhysicsWorld API](physics_world.md)

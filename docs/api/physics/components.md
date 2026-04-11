# Physics Components API Reference

The Helios Physics module utilizes a set of ECS components to define physical properties and shapes of entities.

## RigidBody

The `RigidBody` component defines how an entity participates in the physics simulation.

| Member | Type | Description |
| :--- | :--- | :--- |
| `body_type` | `BodyType` | `Static`, `Dynamic`, or `Kinematic`. |
| `mass` | `float` | Mass of the body in kilograms. |
| `friction` | `float` | Surface friction (0.0 to 1.0). |
| `restitution` | `float` | Bounciness (0.0 to 1.0). |
| `flags` | `uint32_t` | `RigidBodyFlags::UseGravity`, etc. |

### Body Types

- **`Static`**: A non-moving object (e.g., floor, wall). Has infinite mass and does not respond to forces.
- **`Dynamic`**: A fully simulated object. Responds to gravity, forces, and impulses.
- **`Kinematic`**: An object moved explicitly by code (e.g., a moving platform). It will push dynamic bodies but is unaffected by forces itself.

## Colliders

An entity must have at least one collider component for it to be registered in the physics simulation.

### BoxCollider

Defines a box-shaped collision volume.

- `half_extents`: A `vec3` representing half the dimensions of the box (width/2, height/2, depth/2).
- `offset`: World-space offset from the entity's origin.
- `flags`: `ColliderFlags::IsTrigger`, etc.

### SphereCollider

Defines a sphere-shaped collision volume.

- `radius`: Radius of the sphere.
- `offset`: World-space offset from the entity's origin.
- `flags`: `ColliderFlags::IsTrigger`, etc.

## ContactEvent

Generated when two physics bodies collide. It is accessible via an `EventReader<ContactEvent>`.

- `entity_a`: The first entity involved in the collision.
- `entity_b`: The second entity involved in the collision.
- `world_point`: The world-space coordinates of the contact point.
- `normal`: The contact normal vector (from `entity_a` toward `entity_b`).
- `impulse`: The magnitude of the normal impulse applied between the bodies.

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

# Physics API Reference

The Helios Physics module provides a high-level, ECS-integrated interface for 3D physics simulation. It currently uses **Jolt Physics** as the primary backend, exposed through an abstract interface to allow for future backend flexibility.

## Core Concepts

- **Physics World**: The central simulation container. Manages rigid bodies, constraints, and queries.
- **Rigid Body**: An entity that participates in the physics simulation. Can be Static, Dynamic, or Kinematic.
- **Colliders**: Define the physical shape of a Rigid Body (Box, Sphere, Capsule).
- **Contact Events**: Generated when two physics bodies collide, containing impulse and point data.

## API Pages

- [**PhysicsWorld**](physics_world.md): The main interface for manual simulation control and raycasting.
- [**PhysicsPlugin**](physics_plugin.md): Registration and configuration of the physics module.
- [**Components**](components.md): ECS components for defining physical properties and shapes.

## Getting Started

To enable physics in your application, add the `PhysicsPlugin` to your `App` builder:

```cpp
#include <helios/physics/interface/physics_plugin.h>
#include <helios/physics/jolt/jolt_physics_world.h>

using namespace helios::physics;

app.add_plugin(PhysicsPlugin<JoltPhysicsWorld>{});
```

Once the plugin is active, simply adding a `RigidBody` and a `BoxCollider` (or `SphereCollider`) to an entity will automatically register it with the physics world.

## Related Pages

- [Physics Architecture](../../book/physics.md)
- [ECS Overview](../../book/ecs.md)

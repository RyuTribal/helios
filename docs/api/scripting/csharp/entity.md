# Entity

The `Entity` class is a managed wrapper for C++ entities. It provides access to components, physics, and world state. `ScriptBehaviour` inherits from `Entity`.

## Properties

| Property | Type | Description |
| :--- | :--- | :--- |
| `ID` | `ulong` | The entity's unique identifier. |
| `TagName` | `string` | The entity's tag name (if any). |
| `IsAlive` | `bool` | Returns true if the entity still exists in the world. |
| `LinearVelocity` | `Vector3` | Get or set the entity's linear velocity. |
| `AngularVelocity` | `Vector3` | Get or set the entity's angular velocity. |
| `DeltaTime` | `float` (static) | Seconds since the last frame. |

## Component Access

| Method | Description |
| :--- | :--- |
| `T? GetComponent<T>()` | Get a component by type (native or managed). |
| `bool HasComponent<T>()` | Check if the entity has a specific component. |
| `T AddComponent<T>(T)` | Add a managed C# component to the entity. |
| `bool RemoveComponent<T>()` | Remove a managed C# component from the entity. |

### Note on Native Components
Native components (like `TransformComponent`) cannot be added or removed from C#. They must be added via the editor or C++ code.

## Physics Methods

| Method | Description |
| :--- | :--- |
| `void ApplyForce(Vector3)` | Apply a continuous force to the entity. |
| `void ApplyImpulse(Vector3)` | Apply an instantaneous impulse to the entity. |
| `void ApplyTorque(Vector3)` | Apply a continuous torque to the entity. |
| `void PhysicsTeleport(...)` | Teleport the physics body and reset velocity. |

## Utility Methods

| Method | Description |
| :--- | :--- |
| `void LookAt(target, up)` | Rotate the entity to face a target position. |
| `void Destroy()` | Despawn the entity from the world. |

## Example

```csharp
public override void OnUpdate(float delta) {
    var transform = GetComponent<TransformComponent>();
    if (transform != null) {
        transform.Translation += new Vector3(0, 0, delta * 5f);
    }
}
```

## Related Pages

- [TransformComponent](transform_component.md)
- [ScriptBehaviour](script_behaviour.md)
- [Scene](scene.md)
- [ECS Overview (Book)](../../../book/ecs.md)

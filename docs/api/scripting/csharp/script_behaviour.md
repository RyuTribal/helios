# ScriptBehaviour

`ScriptBehaviour` is the base class for all user scripts. It provides lifecycle callbacks and inherits all methods from `Entity`.

## Lifecycle Callbacks

| Callback | Description |
| :--- | :--- |
| `void OnCreate()` | Called once when the script instance is created. |
| `void OnUpdate(float delta)` | Called every frame during `Schedule::Update`. |
| `void OnDestroy()` | Called when the entity is destroyed or script removed. |
| `void OnCollisionEnter(...)` | Called when this entity collides with another. |
| `void OnCollisionExit(ulong)` | Called when this entity stops colliding. |

## Instance Methods

`ScriptBehaviour` provides several protected helper methods for cleaner access to engine features:

### Input Helpers

| Method | Description |
| :--- | :--- |
| `bool IsKeyPressed(int)` | Check if a key is held. |
| `bool IsKeyJustPressed(int)` | Check if a key was pressed this frame. |
| `bool IsMouseButtonPressed(int)` | Check if a mouse button is held. |
| `Vector2 GetMousePosition()` | Current mouse screen coordinates. |
| `Vector2 GetMouseDelta()` | Mouse movement since last frame. |
| `float GetScrollDelta()` | Scroll wheel movement. |
| `void SetCursorMode(int)` | 0 = normal, 1 = captured/hidden. |

## Example

```csharp
using Helios;
using System.Numerics;

public class PlayerController : ScriptBehaviour {
    public override void OnCreate() {
        Log.Info("Player controller initialized!");
    }

    public override void OnUpdate(float delta) {
        if (IsKeyPressed(70)) { // 'F'
            ApplyForce(new Vector3(0, 10, 0));
        }
    }

    public override void OnCollisionEnter(ulong otherId, Vector3 point, Vector3 normal, float impulse) {
        Log.Info($"Collided with entity {otherId}!");
    }
}
```

## Related Pages

- [Entity](entity.md)
- [Input](input.md)
- [TransformComponent](transform_component.md)

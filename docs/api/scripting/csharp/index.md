# C# ScriptCore API

The `ScriptCore` library provides the primary API for user scripts in Helios. All user scripts should inherit from `ScriptBehaviour`.

## Core Classes

- [**ScriptBehaviour**](script_behaviour.md): Base class for all user scripts, providing lifecycle callbacks and entity access.
- [**Entity**](entity.md): The primary wrapper for interacting with world objects, components, and physics.
- [**TransformComponent**](transform_component.md): API for managing position, rotation, and scale.

## Static Helpers

- [**Input**](input.md): Access keyboard and mouse state.
- [**Audio**](audio.md): Play sounds and manage audio assets.
- [**Scene**](scene.md): Runtime scene loading and instantiation.

## Namespaces

The entire API is contained within the `Helios` namespace.

```csharp
using Helios;
using System.Numerics;

public class MyScript : ScriptBehaviour {
    public override void OnUpdate(float delta) {
        if (IsKeyPressed(70)) { // 'F'
            ApplyForce(new Vector3(0, 10, 0));
        }
    }
}
```

## Related Pages

- [C++ Scripting API](../cpp/index.md)
- [Scripting Guide](../../../book/scripting.md)
- [Examples](../../../book/index.md)

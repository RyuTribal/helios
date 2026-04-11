# Scripting Integration Guide

Helios features a high-performance C# scripting system, allowing you to write
game logic in a managed environment with full access to the engine's ECS,
physics, and rendering systems.

## C# via CoreCLR

Helios hosts the **CoreCLR runtime** (the open-source heart of .NET 8.0+)
directly within the engine process. This provides a modern C# 8.0+ environment
with JIT compilation, strong typing, and the vast .NET ecosystem.

- **Assembly Loading:** Your game scripts are compiled into a separate DLL
  (e.g., `SandboxScripts.dll`).
- **Engine Core:** The engine provides a `ScriptCore.dll` containing the base
  classes and the native bridge API.
- **Execution:** Scripts run during the main ECS update loop, managed by the
  `ScriptingPlugin`.

## ScriptBehaviour

All user scripts must inherit from the **[`ScriptBehaviour`](../api/scripting/csharp/script_behaviour.md)** base class. This
provides the gateway to entity properties and lifecycle events.

### Lifecycle Callbacks

| Callback | Description |
|---|---|
| `OnCreate()` | Called once when the script instance is first created. Use for initialization. |
| `OnUpdate(float delta)` | Called every frame during `Schedule::Update` (variable timestep). `delta` is the time in seconds since the last frame. |
| `OnDestroy()` | Called when the entity is destroyed or the script component is removed. |
| `OnCollisionEnter(...)` | Triggered when the physics engine detects a new contact. |
| `OnCollisionExit(...)` | Triggered when a physics contact ends. |

> [!IMPORTANT]
> **Execution Order:** `OnUpdate` runs during the engine's main update phase (`Schedule::Update`), which uses a variable timestep. There is currently no `OnFixedUpdate` for C# scripts; any logic requiring a fixed-step (e.g., custom physics integration) should be implemented in C++ or manually accumulated within `OnUpdate`.

### Example: Rotator Script

A simple script that rotates an entity around its Y-axis:

```csharp
using Helios;
using System.Numerics;

namespace Game;

public class Rotator : ScriptBehaviour
{
    public float Speed = 1.0f;

    public override void OnUpdate(float delta)
    {
        // Get the transform component
        var transform = GetComponent<TransformComponent>();
        if (transform != null)
        {
            // Update rotation (stored as Euler angles in radians)
            var rotation = transform.Rotation;
            rotation.Y += Speed * delta; // Speed is in radians/sec
            transform.Rotation = rotation;
        }

        // Example: Log a message if Space is pressed
        if (Input.IsKeyJustPressed(KeyCode.Space))
        {
            Log.Info($"Rotator {Entity.ID} is spinning at speed {Speed}");
        }
    }
}
```

## The Native Bridge (Glue)

C# interacts with the C++ engine via a high-performance **Native Bridge**
using internal calls (P/Invoke with function pointers). This bridge is
bidirectional: the engine calls into C# lifecycle methods, and C# calls into
engine systems.

### The Entity Class

The **[`Entity`](../api/scripting/csharp/entity.md)** class (which `ScriptBehaviour` inherits from) provides the
primary interface to the engine:

- **`GetComponent<T>()`**: Retrieves a component from the entity. Returns `null` if not found.
- **`AddComponent<T>(T instance)`**: Adds a managed C# component to the entity.
- **`HasComponent<T>()`**: Checks if the entity has a specific component.
- **`ApplyForce(Vector3 force)`**: Applies a physical force to the entity's `RigidBody`.
- **`LookAt(Vector3 target, Vector3 up)`**: Rotates the transform to face a target.
- **`Destroy()`**: Despawns the entity from the world.

### Available Engine Components

You can access native C++ components from C# as if they were managed classes.
These are marked with `[NativeComponent]` and cannot be added/removed from C#.

The following component is currently available with a full C# bridge:

| Component | Description |
|---|---|
| **[`TransformComponent`](../api/scripting/csharp/transform_component.md)** | `Translation`, `Rotation` (Euler angles in radians), and `Scale`. |

> [!NOTE]
> Additional engine components (e.g., `RigidBody`, `MeshRenderer`, `PointLight`) are planned for future updates. Currently, these are only accessible via the raw `NativeAPI` layer if implemented, or must be managed from the C++ side.

## Input, Audio, and Scenes

Helios provides several static helper classes for interacting with global engine
systems.

### Input System

Query the keyboard and mouse state directly:

```csharp
if (Input.IsKeyPressed(KeyCode.W)) { /* Move forward */ }
if (Input.IsKeyJustPressed(KeyCode.Space)) { /* Jump */ }

Vector2 mousePos = Input.MousePosition;
if (Input.IsMouseButtonPressed(0)) { /* Primary fire */ }
```

### Audio System

Play spatialized or global audio using the `Audio` class:

```csharp
// Play a specific audio file at a world position
Audio.Play("Assets/Sounds/Explosion.wav", transform.Translation, 1.0f, false);

// Play a pre-loaded audio asset (faster, load in OnCreate via Assets.Load)
AssetHandle jumpSound = Assets.Load("Assets/Sounds/Jump.wav");
Audio.Play(jumpSound, transform.Translation);
```

### Scene Management

Load and instantiate scenes dynamically:

```csharp
// Replace the current scene entirely
Scene.Load("Scenes/MainLevel.hvescn");

// Instantiate a scene as a sub-tree (e.g., an enemy prefab)
Scene.Instantiate("Prefabs/Enemy.hvescn");

// Preload check for smooth transitions
if (Scene.IsReady("Scenes/BigLevel.hvescn")) {
    Scene.Load("Scenes/BigLevel.hvescn");
}
```

## Hot Reload Workflow

Helios supports a rapid iteration workflow via **Hot Reload**. The engine
monitors your script DLL for changes while the editor or game is running.

1.  **DLL Watcher:** The engine watches for changes to your `.dll` file.
2.  **Snapshot:** When a change is detected, the engine snapshots all active
    script associations.
3.  **Reload:** The old CoreCLR assembly is unloaded, and the new one is loaded.
4.  **Restore:** All `ScriptBehaviour` instances are re-instantiated, and their
    associations with entities are restored via their unique IDs/Handles.

> [!NOTE]
> Managed state (fields/variables) is currently reset during hot reload.
> Persistent state should be stored in ECS components.

## Attaching Scripts

To attach a C# script to an entity, add the `ScriptInstance` component.

### Via C++
```cpp
auto& script = world.add<ScriptInstance>(entity);
script.script_class_name = "Game.Rotator";
```

### Via Editor
In the Helios Editor, you can add a "Script Instance" component to any entity
and type the full name of the C# class (including namespace) in the Class Name
field. The engine will automatically instantiate the script and begin calling
its lifecycle methods once the simulation starts.

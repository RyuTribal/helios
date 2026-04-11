# Scripting

## Overview

Helios supports C# scripting via .NET CoreCLR. User scripts are compiled to
a .NET assembly (`.dll`), loaded at runtime by the engine, and execute during
the ECS update loop. The bridge between C++ and C# is a struct of function
pointers (`NativeEngineAPI`) passed to managed code at initialization.

## Setup

```cpp
#include <helios/script/scripting_plugin.h>

app.add_plugin(ScriptingPlugin{.config = {
    .runtime_config_path = "ScriptCore.runtimeconfig.json",
    .script_core_dll_path = "ScriptCore.dll",
    .app_assembly_path = "GameScripts.dll",
    .watch_directory = "Scripts/",
}});
```

The plugin inserts:
- `ScriptRuntime` resource (`std::unique_ptr<ScriptRuntime>`) -- manages the
  CoreCLR host and assembly loading.
- `ScriptExecutionState` resource -- controls whether scripts execute.

It registers these systems:

| System | Schedule | Purpose |
|---|---|---|
| `check_script_reload` | PreUpdate | Watches for .dll changes, triggers hot reload |
| `script_create_system` | Update | Creates managed instances for new ScriptInstance entities |
| `script_execution_system` | Update | Calls `OnUpdate` on all active script instances |
| `script_destroy_system` | PostUpdate | Calls `OnDestroy` for removed/despawned scripts |

## ScriptBehaviour

All user scripts inherit from `ScriptBehaviour`:

```csharp
using Helios;
using System.Numerics;

public class PlayerController : ScriptBehaviour
{
    public override void OnCreate()
    {
        // Called once when the script instance is created
    }

    public override void OnUpdate(float delta)
    {
        // Called every frame
        var transform = GetComponent<TransformComponent>();
        if (IsKeyPressed(Key.W))
            transform.Translation += new Vector3(0, 0, -5 * delta);
    }

    public override void OnDestroy()
    {
        // Called when the entity is despawned or the script is removed
    }

    public override void OnCollisionEnter(ulong otherEntityId,
        Vector3 contactPoint, Vector3 normal, float impulse)
    {
        // Called when this entity collides with another
        Log.Info($"Hit entity {otherEntityId} with impulse {impulse}");
    }
}
```

### Lifecycle callbacks

| Callback | When |
|---|---|
| `OnCreate()` | Once, after the managed instance is allocated |
| `OnUpdate(float delta)` | Every frame during `Schedule::Update` |
| `OnDestroy()` | When the entity is despawned or the script component is removed |
| `OnCollisionEnter(...)` | When a physics contact is detected |
| `OnCollisionExit(...)` | When a physics contact ends |

### Inherited from Entity

`ScriptBehaviour` extends `Entity`, so all entity methods are available
directly:

```csharp
// Physics
ApplyForce(new Vector3(0, 10, 0));
ApplyImpulse(new Vector3(0, 5, 0));
ApplyTorque(new Vector3(0, 1, 0));
LinearVelocity = new Vector3(0, 0, 0);
PhysicsTeleport(newPos, newRot);

// Transform
LookAt(target, Vector3.UnitY);
var tag = TagName;

// Lifecycle
Destroy();
bool alive = IsAlive();
```

### Input methods (from ScriptBehaviour)

```csharp
if (IsKeyPressed(Key.Space)) { ... }
if (IsKeyJustPressed(Key.E)) { ... }
if (IsMouseButtonPressed(0)) { ... }
Vector2 pos = GetMousePosition();
Vector2 delta = GetMouseDelta();
float scroll = GetScrollDelta();
SetCursorMode(1);  // 0 = normal, 1 = captured
```

## Available C# APIs

### Input

```csharp
Input.IsKeyPressed(keycode)
Input.IsKeyJustPressed(keycode)
Input.IsMouseButtonPressed(button)
Input.MousePosition    // Vector2
Input.MouseDelta       // Vector2
Input.ScrollDelta      // float
Input.SetCursorMode(mode)
Input.GetCursorMode()
```

### Audio

```csharp
Audio.Play(assetHandle, position, volume, loop);
Audio.Play(assetHandle, volume, loop);          // at origin
Audio.Play("path/to/sound.hveaudio", position, volume, loop);
```

### Scene

```csharp
Scene.Load("Scenes/Level_2.hvescn");           // replace current scene
Scene.Instantiate("Scenes/enemy_wave.hvescn");  // add as sub-scene
bool ready = Scene.IsReady("Scenes/Level_2.hvescn");  // preload check
```

### Assets

```csharp
AssetHandle h = Assets.Load("Sounds/explosion.hveaudio");
bool loaded = h.IsLoaded;
```

### Entity / Components

```csharp
var transform = GetComponent<TransformComponent>();
transform.Translation = new Vector3(1, 2, 3);
transform.Rotation = new Vector3(0, 45, 0);    // Euler degrees
transform.Scale = new Vector3(1, 1, 1);

bool has = HasComponent<TransformComponent>();

// C#-defined components
var myComp = AddComponent(new HealthComponent { Current = 100 });
RemoveComponent<HealthComponent>();
```

Native engine components (marked with `[NativeComponent]`) cannot be added
from C# -- they are managed by the C++ ECS. Currently `TransformComponent`
is the only native component exposed to scripts.

### DeltaTime

```csharp
float dt = Entity.DeltaTime;  // static property, convenience shortcut
```

## Native API Bridge

The bridge is a struct of C function pointers passed from C++ to C# at
initialization. The C++ side fills a `NativeEngineAPI` struct; the C# side
stores it and wraps each pointer in a safe static method.

### C++ side (NativeEngineAPI)

Every callback receives `void* world_ctx` as its first argument -- this is
the `World*` pointer. No global state exists.

```cpp
struct NativeEngineAPI {
    void* world_context;

    // Logging
    void (*Log)(void*, int level, const char* message);

    // Entity
    uint64_t (*Spawn)(void*);
    void (*Despawn)(void*, uint64_t entity_id);
    bool (*IsAlive)(void*, uint64_t entity_id);

    // Component
    bool (*HasComponent)(void*, uint64_t entity_id, const char* name);

    // Transform
    void (*TransformGetTranslation)(void*, uint64_t, float* out_xyz);
    void (*TransformSetTranslation)(void*, uint64_t, float* in_xyz);
    // ... rotation, scale, look_at

    // Input
    bool (*IsKeyPressed)(void*, int keycode);
    bool (*IsKeyJustPressed)(void*, int keycode);
    // ... mouse, scroll, cursor mode

    // Physics
    void (*PhysicsApplyForce)(void*, uint64_t, float* xyz);
    void (*PhysicsApplyImpulse)(void*, uint64_t, float* xyz);
    void (*PhysicsApplyTorque)(void*, uint64_t, float* xyz);
    // ... velocity, teleport

    // Assets
    uint64_t (*AssetLoad)(void*, const char* path);
    bool (*AssetIsLoaded)(void*, uint64_t handle);

    // Audio
    void (*AudioPlayHandle)(void*, uint64_t handle, float x, float y, float z,
                            float volume, bool loop);
    void (*AudioPlayFile)(void*, const char* path, float x, float y, float z,
                          float volume, bool loop);

    // Scene
    void (*SceneLoad)(void*, const char* path);
    void (*SceneInstantiate)(void*, const char* path);
    bool (*SceneIsReady)(void*, const char* path);

    // Time
    float (*GetDeltaTime)(void*);
};
```

### C# side (NativeEngineAPI struct)

The C# struct mirrors the C++ layout exactly using
`[StructLayout(LayoutKind.Sequential)]` and `delegate* unmanaged` function
pointers. The `NativeAPI` static class wraps these in safe methods that
handle marshaling (string to UTF-8, Vector3 to float pointer, etc.).

### Adding new native functions

1. Add the function pointer to the C++ `NativeEngineAPI` struct.
2. Implement the glue function in `script_glue.cpp`.
3. Add the matching `delegate* unmanaged` to the C# `NativeEngineAPI` struct.
4. Add a wrapper method to `NativeAPI.cs`.
5. Expose it through the appropriate API class (`Input`, `Audio`, etc.).

Field order must match exactly between C++ and C# -- the struct is passed
as a raw memory block.

## ScriptInstance Component

Attach to an entity to bind a C# script:

```cpp
struct ScriptInstance {
    std::string script_class_name;  // "Game.PlayerController"
    uint32_t script_type_id = 0;    // assigned by runtime
    uint64_t managed_handle = 0;    // GCHandle, 0 = not yet created
};
```

The `script_create_system` finds entities with `ScriptInstance` but without
`ScriptInitialized`, creates the managed object, and adds the
`ScriptInitialized` marker.

## ScriptExecutionState

Controls whether script systems run:

```cpp
struct ScriptExecutionState {
    bool running = false;
};
```

In the editor, this is `false` during Edit mode and `true` during Play mode.
The runtime itself stays alive -- only execution is gated.

## Hot Reload

When `watch_directory` is set, a `FileWatcher` monitors for `.dll` changes.
The reload flow:

1. `check_script_reload` detects the change.
2. `ScriptRuntime::invoke_destroy()` is called on all active instances.
3. The old assembly is unloaded.
4. The new assembly is loaded.
5. `ScriptRuntime::invoke_create()` recreates all instances.

State is not automatically preserved across reloads. Entity-side
`ScriptInstance` components persist, but managed object state is lost.

## Batched Updates

The runtime batches `OnUpdate` calls by script type. Entities sharing the
same `script_type_id` are passed as a parallel array to
`ScriptRuntime::invoke_update()`, allowing the managed side to amortize
per-type overhead.

## ScriptRuntime Interface

```cpp
class ScriptRuntime {
public:
    virtual bool load_assembly(const std::filesystem::path&) = 0;
    virtual void unload_assembly() = 0;
    virtual bool reload_assembly(const std::filesystem::path&) = 0;

    virtual bool class_exists(const std::string& name) const = 0;
    virtual std::vector<std::string> get_script_class_names() const = 0;

    virtual uint64_t invoke_create(const std::string& class_name, uint64_t entity_id) = 0;
    virtual void invoke_update(uint32_t type_id, const uint64_t* entities,
                               const uint64_t* handles, size_t count, float delta) = 0;
    virtual void invoke_destroy(uint64_t entity_id, uint64_t managed_handle) = 0;
    virtual void invoke_on_collision(uint64_t entity_id, uint64_t other_id,
                                      float px, float py, float pz,
                                      float nx, float ny, float nz,
                                      float impulse) = 0;

    virtual void request_reload() = 0;
    virtual bool reload_requested() const = 0;
};
```

The production implementation (`CoreCLRRuntime`) hosts the .NET runtime via
`hostfxr`. A `MockScriptRuntime` exists for tests.

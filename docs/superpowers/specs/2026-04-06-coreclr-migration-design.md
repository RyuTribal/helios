# Helios Scripting: Mono to .NET 10 CoreCLR Migration

## Goal

Replace the Mono runtime with .NET 10 CoreCLR using the `hostfxr` native hosting API and collectible `AssemblyLoadContext` for hot-reload. Self-contained runtime vendored in the repo. Breaking C# API changes allowed.

## Decisions

- **Runtime:** .NET 10 (upcoming LTS, Nov 2025)
- **Hosting API:** `hostfxr` + `AssemblyLoadContext` (Approach A)
- **Script builds:** `dotnet build` via CommandLine (not in-process Roslyn)
- **Distribution:** Self-contained vendored runtime (~60-80MB in `Engine/vendor/dotnet/`)
- **C# API:** Breaking changes allowed — input moves to static class, InternalCalls replaced by NativeAPI

## Architecture

### Overview

C++ engine hosts .NET 10 CoreCLR via `hostfxr`. A managed bridge class (`ScriptHostBridge`) in ScriptCore serves as the single entry point between C++ and C#. User script assemblies load into a collectible `AssemblyLoadContext` that can be unloaded and recreated for hot-reload.

```
C++ Engine
  │
  ├─ dlopen/LoadLibrary ─→ libhostfxr.so / hostfxr.dll
  │     │
  │     ├─ hostfxr_initialize_for_runtime_config()
  │     ├─ hostfxr_get_runtime_delegate() → load_assembly_and_get_function_pointer
  │     └─ hostfxr_close()
  │
  ├─ load_assembly_and_get_function_pointer ─→ ScriptHostBridge.Initialize()
  │
  └─ ManagedBridge function pointers ─→ ScriptHostBridge static methods
        │
        ├─ Default ALC: ScriptCore.dll (loaded once, never unloaded)
        └─ Collectible ALC: UserScripts.dll (loaded/unloaded on hot-reload)
```

### Runtime Initialization

1. C++ locates vendored `hostfxr` at `vendor/dotnet/host/fxr/<version>/`
2. Loads it via `dlopen`/`LoadLibrary`, resolves `hostfxr_initialize_for_runtime_config`, `hostfxr_get_runtime_delegate`, `hostfxr_close`
3. Calls `hostfxr_initialize_for_runtime_config` with `ScriptCore.runtimeconfig.json`
4. Calls `hostfxr_get_runtime_delegate` with `hdt_load_assembly_and_get_function_pointer`
5. Uses delegate loader to call `ScriptCore.ScriptHostBridge.Initialize` — this receives a `NativeEngineAPI*` struct of C++ function pointers and returns a `ManagedBridge` struct of C# function pointers back to C++

### Shutdown

1. C++ calls `bridge.DestroyAllInstances()`
2. C++ calls `bridge.UnloadAppAssembly()`
3. C++ calls `hostfxr_close()`

The CoreCLR runtime lives for the entire process lifetime. Only the user script ALC is created/destroyed.

### C++ → C# Interop (Engine calling scripts)

C++ stores a `ManagedBridge` struct of function pointers obtained from the bridge initialization:

```cpp
struct ManagedBridge {
    void (*LoadAppAssembly)(const char* path);
    void (*UnloadAppAssembly)();
    int  (*GetEntityClassCount)();
    void (*GetEntityClassName)(int index, char* buffer, int bufferSize);
    bool (*EntityClassExists)(const char* fullName);
    int  (*CreateInstance)(const char* className, uint64_t entityId);
    void (*DestroyInstance)(uint64_t entityId);
    void (*InvokeOnCreate)(uint64_t entityId);
    void (*InvokeOnUpdate)(uint64_t entityId, float deltaTime);
    void (*DestroyAllInstances)();
    bool (*EntityHasComponent)(uint64_t entityId, const char* componentName);
};
```

Each corresponds to a `[UnmanagedCallersOnly]` static method in `ScriptHostBridge.cs`. C++ calls them as regular function pointers.

### C# → C++ Interop (Scripts calling engine)

C++ fills a `NativeEngineAPI` struct with function pointers for all 46 engine functions (input, transform, camera, physics, sound) and passes it to the managed bridge at init. The bridge stores it in a static `NativeAPI` class that wraps each pointer as a typed call.

```cpp
struct NativeEngineAPI {
    bool (*IsKeyPressed)(int keycode);
    bool (*IsMouseButtonPressed)(int button);
    void (*GetMousePosition)(float* x, float* y);
    bool (*EntityHasComponent)(uint64_t entityId, const char* componentName);
    void (*EntityDestroy)(uint64_t entityId);
    void (*GetTranslation)(uint64_t entityId, float* xyz);
    void (*SetTranslation)(uint64_t entityId, float* xyz);
    // ... all 46 functions
};
```

C# side:
```csharp
internal static unsafe class NativeAPI {
    internal static NativeEngineAPI* Api;
    public static bool IsKeyPressed(int key) => Api->IsKeyPressed(key);
    // ... wraps all 46
}
```

No `mono_add_internal_call`, no string-based registration. Blittable types pass with zero marshalling.

### Hot-Reload via Collectible AssemblyLoadContext

ScriptCore owns a `ScriptAssemblyLoadContext` extending `AssemblyLoadContext` with `isCollectible: true`. Only the user's game script assembly lives in this context. ScriptCore itself is in the default non-collectible context.

**Reload flow:**

1. FileWatch detects DLL change, sets reload flag (same mechanism as today)
2. C++ calls `bridge.UnloadAppAssembly()`:
   - Bridge destroys all script instances, clearing all references to user types
   - Bridge calls `context.Unload()` on the collectible ALC
   - GC collects all types from the old assembly
3. C++ calls `bridge.LoadAppAssembly(path)`:
   - Bridge creates a new collectible ALC
   - Loads the updated DLL into it
   - Scans for Entity subclasses via reflection
4. C++ re-creates script instances for active entities

Only the user assembly reloads. ScriptCore and the runtime stay warm. No re-registration of engine API. JIT cache for engine code persists. Faster than Mono domain unload.

**Caveat:** The bridge must hold zero references to user types after `UnloadAppAssembly`. Any leaked reference prevents GC from collecting the old ALC.

## File Changes

### Rewritten

| File | Change |
|------|--------|
| `Engine/src/Script/ScriptEngine.h` | Remove all Mono types. New API using ManagedBridge function pointers. |
| `Engine/src/Script/ScriptEngine.cpp` | Replace Mono init/invoke/reload with hostfxr boot + bridge calls. |
| `Engine/src/Script/ScriptGlue.h` | Simplified — just `Init()` to fill NativeEngineAPI struct. |
| `Engine/src/Script/ScriptGlue.cpp` | Remove mono_add_internal_call. Fill NativeEngineAPI function pointer struct. |
| `Engine/src/Project/ProjectSerializer.cpp` | `CreateScriptProject`: generate .csproj + `dotnet build` instead of premake + csc. |

### New Files

| File | Purpose |
|------|---------|
| `ScriptCore/ScriptCore.csproj` | .NET 10 class library project, replaces premake5.lua |
| `ScriptCore/Source/Helios/Bridge/ScriptHostBridge.cs` | Managed entry point with `[UnmanagedCallersOnly]` methods |
| `ScriptCore/Source/Helios/Bridge/ScriptAssemblyLoadContext.cs` | Collectible ALC subclass |
| `ScriptCore/Source/Helios/Bridge/NativeAPI.cs` | Internal wrapper around C++ function pointers (replaces InternalCalls.cs) |
| `ScriptCore/Source/Helios/Input.cs` | New static Input class (moved from Entity) |
| `Engine/src/Script/HostFXR.h` | hostfxr function pointer typedefs and loader |
| `Editor/Resources/Scripts/ScriptCore.runtimeconfig.json` | Runtime config for hostfxr initialization |
| `setup_linux_vendor.sh` (update) | Add .NET 10 runtime download |

### Deleted

| File/Directory | Reason |
|----------------|--------|
| `Engine/vendor/mono/` | Mono headers and libs no longer needed |
| `Editor/mono/` | Mono runtime no longer needed |
| `ScriptCore/premake5.lua` | Replaced by .csproj |
| `ScriptCore/Source/Helios/InternalCalls.cs` | Replaced by NativeAPI.cs |
| `ScriptCore/Source/Helios/CustomAttributes.cs` | Unused (field inspection was commented out) |

### Modified

| File | Change |
|------|--------|
| `Dependencies.lua` | Remove mono paths, add dotnet include path |
| `Engine/premake5.lua` | Remove mono links/includes, add hostfxr header path. Remove libmonosgen linking. Add dl/pthread (already present from Linux port). |
| `Editor/premake5.lua` | Remove mono links |
| `EditorLauncher/premake5.lua` | Remove mono links |
| `premake5.lua` | Remove ScriptCore from premake workspace (now built via dotnet) |
| `setup_linux_vendor.sh` | Add .NET 10 runtime download section |
| `ScriptCore/Source/Helios/Scene/Entity.cs` | Remove input methods, keep HasComponent/GetComponent/Destroy |
| `ScriptCore/Source/Helios/Scene/Components.cs` | Change InternalCalls.X() to NativeAPI.X() internally |
| `ScriptCore/Source/Helios/KeyCodes.cs` | No changes |
| `Engine/src/pch.h` | Remove `<mono/*.h>` includes |

## C# API Changes

**Entity class:**
- Remove: `IsKeyPressed()`, `IsMouseButtonPressed()`, `GetMousePosition()`
- Keep: `ID`, `HasComponent<T>()`, `GetComponent<T>()`, `Destroy()`

**New `Input` static class:**
```csharp
public static class Input {
    public static bool IsKeyPressed(KeyCode key);
    public static bool IsMouseButtonPressed(MouseButton button);
    public static Vector2 MousePosition { get; }
}
```

**Components:** Same public API. Internal implementation changes `InternalCalls.X()` to `NativeAPI.X()`.

**Removed:** `InternalCalls.cs`, `CustomAttributes.cs`

## Vendored Runtime Layout

```
Engine/vendor/dotnet/
├── host/
│   └── fxr/<version>/
│       ├── libhostfxr.so    (Linux)
│       └── hostfxr.dll      (Windows)
├── shared/
│   └── Microsoft.NETCore.App/<version>/
│       ├── libcoreclr.so    (Linux)
│       ├── coreclr.dll      (Windows)
│       ├── System.*.dll
│       └── ...
└── dotnet (executable, Linux)
    dotnet.exe (executable, Windows)
```

Downloaded by `setup_linux_vendor.sh` / `setup_win_vendor.bat` from Microsoft's official CDN. ~60-80MB.

## Build Flow

**ScriptCore (engine scripting API):**
```bash
dotnet build ScriptCore/ScriptCore.csproj -o Editor/Resources/Scripts/
```

**User script project (created by engine):**
- Engine generates a `.csproj` referencing ScriptCore
- Engine calls `dotnet build` via CommandLine to produce user DLL
- Output goes to project's `Binaries/` directory

**Engine itself:** Unchanged premake/make flow, just without mono linking.

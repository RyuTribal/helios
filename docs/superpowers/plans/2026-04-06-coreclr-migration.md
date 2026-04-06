# .NET 10 CoreCLR Migration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Mono runtime with .NET 10 CoreCLR using hostfxr hosting API and collectible AssemblyLoadContext for hot-reload.

**Architecture:** C++ engine loads hostfxr dynamically, boots CoreCLR, calls into a managed bridge (ScriptHostBridge) via function pointers. The bridge manages a collectible ALC for user scripts. C#→C++ calls use function pointer structs passed at init (replacing mono_add_internal_call).

**Tech Stack:** .NET 10, hostfxr native hosting API, C++20, premake5

**Spec:** `docs/superpowers/specs/2026-04-06-coreclr-migration-design.md`

---

## File Structure

### New Files
| File | Responsibility |
|------|---------------|
| `Engine/src/Script/HostFXR.h` | Load hostfxr dynamically, resolve function pointers |
| `Engine/src/Script/HostFXR.cpp` | Implementation of hostfxr loader |
| `Engine/src/Script/NativeEngineAPI.h` | C++ struct of function pointers passed to C# |
| `Engine/src/Script/ManagedBridge.h` | C++ struct of function pointers received from C# |
| `ScriptCore/ScriptCore.csproj` | .NET 10 class library project |
| `ScriptCore/Source/Helios/Bridge/ScriptHostBridge.cs` | Managed entry point with [UnmanagedCallersOnly] methods |
| `ScriptCore/Source/Helios/Bridge/ScriptAssemblyLoadContext.cs` | Collectible ALC subclass |
| `ScriptCore/Source/Helios/Bridge/NativeAPI.cs` | Internal wrapper around C++ function pointers |
| `ScriptCore/Source/Helios/Bridge/NativeEngineAPI.cs` | C# mirror of the C++ NativeEngineAPI struct |
| `ScriptCore/Source/Helios/Input.cs` | New static Input class |
| `Editor/Resources/Scripts/ScriptCore.runtimeconfig.json` | Runtime config for hostfxr |

### Rewritten Files
| File | Change |
|------|--------|
| `Engine/src/Script/ScriptEngine.h` | Remove all Mono types, new API with ManagedBridge |
| `Engine/src/Script/ScriptEngine.cpp` | Replace Mono init/invoke with hostfxr + bridge calls |
| `Engine/src/Script/ScriptGlue.h` | Simplified to Init() that fills NativeEngineAPI |
| `Engine/src/Script/ScriptGlue.cpp` | Remove mono_add_internal_call, fill function pointer struct |
| `ScriptCore/Source/Helios/Scene/Entity.cs` | Remove input methods |
| `ScriptCore/Source/Helios/Scene/Components.cs` | InternalCalls → NativeAPI |

### Deleted Files
| File | Reason |
|------|--------|
| `Engine/vendor/mono/` | Mono no longer used |
| `ScriptCore/premake5.lua` | Replaced by .csproj |
| `ScriptCore/Source/Helios/InternalCalls.cs` | Replaced by NativeAPI |
| `ScriptCore/Source/Helios/CustomAttributes.cs` | Unused |

### Modified Files
| File | Change |
|------|--------|
| `Dependencies.lua` | Remove mono paths |
| `Engine/premake5.lua` | Remove mono links/includes, add dl for dlopen |
| `Editor/premake5.lua` | Remove mono links |
| `EditorLauncher/premake5.lua` | Remove mono links |
| `premake5.lua` | Remove ScriptCore from premake workspace |
| `Engine/src/pch.h` | Remove mono includes |
| `Engine/src/Scene/Components.h` | ScriptComponent: remove Ref<ScriptClass>, keep string Name |
| `Engine/src/Project/ProjectSerializer.cpp` | dotnet new/build instead of premake+csc |
| `setup_linux_vendor.sh` | Add .NET 10 runtime download |

---

### Task 1: Remove Mono from build system and add hostfxr headers

**Files:**
- Modify: `Dependencies.lua`
- Modify: `Engine/premake5.lua`
- Modify: `Editor/premake5.lua`
- Modify: `EditorLauncher/premake5.lua`
- Modify: `premake5.lua`
- Modify: `Engine/src/pch.h`
- Delete: `Engine/vendor/mono/` (after this task)

- [ ] **Step 1: Update Dependencies.lua — remove all mono references**

Remove `IncludeDir["mono"]`, `LibraryDir["mono_win"]`, `LibraryDir["mono_linux"]`, `Library["mono_win"]`, `Library["mono_linux"]`. Add:

```lua
IncludeDir["nethost"] = "%{wks.location}/Engine/vendor/dotnet/include"
```

- [ ] **Step 2: Update Engine/premake5.lua — remove mono, add nethost include**

In `includedirs`, replace `"%{IncludeDir.mono}"` with `"%{IncludeDir.nethost}"`.

In `filter "system:windows"` links: remove `"%{Library.mono_win}"`. Remove `LibraryDir.mono_win` from `libdirs`.

In `filter "system:linux"` links: remove `"%{Library.mono_linux}"`. Remove `LibraryDir.mono_linux` from `libdirs`. Keep `dl` (needed for dlopen of hostfxr).

- [ ] **Step 3: Update Editor/premake5.lua — remove mono links**

In `filter "system:linux"`: remove `"%{Library.mono_linux}"` from links. Remove `LibraryDir.mono_linux` from libdirs.

- [ ] **Step 4: Update EditorLauncher/premake5.lua — same as Editor**

Remove mono links and libdirs from Linux filter.

- [ ] **Step 5: Update premake5.lua — remove ScriptCore from workspace**

Remove `include "ScriptCore"` from the `group "Core"` section. ScriptCore is now built via `dotnet build`, not premake.

- [ ] **Step 6: Update Engine/src/pch.h — remove mono includes**

Remove the `#ifdef PLATFORM_WINDOWS` block that includes `<Windows.h>` is fine (it's for other things). Remove any `#include <mono/*.h>` if present in pch.h. (Currently mono headers are only in ScriptEngine.cpp and ScriptGlue.cpp, not pch.h, so this step may be a no-op — verify.)

- [ ] **Step 7: Create Engine/vendor/dotnet/include/ directory with hostfxr headers**

Download the three required headers from the dotnet/runtime GitHub repo (these are stable public API headers):
- `nethost.h`
- `hostfxr.h`
- `coreclr_delegates.h`

Place them in `Engine/vendor/dotnet/include/`.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "build: remove Mono from build system, add hostfxr headers"
```

---

### Task 2: Create HostFXR loader (C++)

**Files:**
- Create: `Engine/src/Script/HostFXR.h`
- Create: `Engine/src/Script/HostFXR.cpp`

The HostFXR loader dynamically loads the hostfxr shared library, resolves the three entry points, and provides a function to initialize the runtime and obtain the `load_assembly_and_get_function_pointer` delegate.

- [ ] **Step 1: Create Engine/src/Script/HostFXR.h**

```cpp
#pragma once

#include <filesystem>

// Forward declarations from hostfxr.h / coreclr_delegates.h
// We use our own typedefs to avoid including the full headers in the engine header
namespace Engine {

    // Function pointer type: loads an assembly and gets a function pointer to a managed method
    using load_assembly_and_get_function_pointer_fn =
        int (*)(const char_t* assembly_path,
                const char_t* type_name,
                const char_t* method_name,
                const char_t* delegate_type_name,
                void* reserved,
                void** delegate);

    class HostFXR
    {
    public:
        static bool Init(const std::filesystem::path& runtime_config_path);
        static void Shutdown();

        // Get a function pointer to a managed static method
        // type_name: "Namespace.Class, AssemblyName"
        // method_name: "MethodName"
        // Returns function pointer or nullptr on failure
        static void* GetManagedFunctionPointer(
            const std::filesystem::path& assembly_path,
            const char* type_name,
            const char* method_name,
            const char* delegate_type_name = nullptr);

    private:
        static bool LoadHostFXR();
        static void* s_HostFXRHandle;
        static load_assembly_and_get_function_pointer_fn s_LoadAssemblyFn;
    };
}
```

- [ ] **Step 2: Create Engine/src/Script/HostFXR.cpp**

```cpp
#include "pch.h"
#include "HostFXR.h"

#include <hostfxr.h>
#include <coreclr_delegates.h>

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#define STR(s) L ## s
#define CH(c) L ## c
#define PLATFORM_LOAD_LIBRARY(path) LoadLibraryW(path)
#define PLATFORM_GET_PROC(handle, name) GetProcAddress((HMODULE)handle, name)
#define PLATFORM_FREE_LIBRARY(handle) FreeLibrary((HMODULE)handle)
using char_t = wchar_t;
#else
#include <dlfcn.h>
#define STR(s) s
#define CH(c) c
#define PLATFORM_LOAD_LIBRARY(path) dlopen(path, RTLD_NOW | RTLD_LOCAL)
#define PLATFORM_GET_PROC(handle, name) dlsym(handle, name)
#define PLATFORM_FREE_LIBRARY(handle) dlclose(handle)
using char_t = char;
#endif

namespace Engine {

    void* HostFXR::s_HostFXRHandle = nullptr;
    load_assembly_and_get_function_pointer_fn HostFXR::s_LoadAssemblyFn = nullptr;

    // hostfxr function pointer types
    using hostfxr_initialize_for_runtime_config_fn = int32_t(*)(
        const char_t* runtime_config_path,
        const void* parameters,
        void** host_context_handle);

    using hostfxr_get_runtime_delegate_fn = int32_t(*)(
        const void* host_context_handle,
        int32_t type,
        void** delegate);

    using hostfxr_close_fn = int32_t(*)(const void* host_context_handle);

    static hostfxr_initialize_for_runtime_config_fn s_InitFn = nullptr;
    static hostfxr_get_runtime_delegate_fn s_GetDelegateFn = nullptr;
    static hostfxr_close_fn s_CloseFn = nullptr;

    bool HostFXR::LoadHostFXR()
    {
        // Locate hostfxr relative to the engine vendor directory
        std::filesystem::path root = ROOT_PATH;
        root = root.parent_path();

#ifdef PLATFORM_WINDOWS
        std::filesystem::path hostfxr_path = root / "Engine/vendor/dotnet/host/fxr";
#else
        std::filesystem::path hostfxr_path = root / "Engine/vendor/dotnet/host/fxr";
#endif

        // Find the version directory (there should be exactly one)
        std::filesystem::path lib_path;
        for (const auto& entry : std::filesystem::directory_iterator(hostfxr_path))
        {
            if (entry.is_directory())
            {
#ifdef PLATFORM_WINDOWS
                lib_path = entry.path() / "hostfxr.dll";
#else
                lib_path = entry.path() / "libhostfxr.so";
#endif
                break;
            }
        }

        if (lib_path.empty() || !std::filesystem::exists(lib_path))
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Could not find hostfxr library at {}", hostfxr_path.string());
            return false;
        }

        s_HostFXRHandle = PLATFORM_LOAD_LIBRARY(lib_path.c_str());
        if (!s_HostFXRHandle)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Failed to load hostfxr from {}", lib_path.string());
            return false;
        }

        s_InitFn = (hostfxr_initialize_for_runtime_config_fn)
            PLATFORM_GET_PROC(s_HostFXRHandle, "hostfxr_initialize_for_runtime_config");
        s_GetDelegateFn = (hostfxr_get_runtime_delegate_fn)
            PLATFORM_GET_PROC(s_HostFXRHandle, "hostfxr_get_runtime_delegate");
        s_CloseFn = (hostfxr_close_fn)
            PLATFORM_GET_PROC(s_HostFXRHandle, "hostfxr_close");

        if (!s_InitFn || !s_GetDelegateFn || !s_CloseFn)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Failed to resolve hostfxr functions");
            return false;
        }

        HVE_CORE_TRACE_TAG("HostFXR", "Loaded hostfxr from {}", lib_path.string());
        return true;
    }

    bool HostFXR::Init(const std::filesystem::path& runtime_config_path)
    {
        if (!LoadHostFXR())
            return false;

        // Initialize the runtime
        void* host_context = nullptr;
        std::string config_str = runtime_config_path.string();
        int32_t rc = s_InitFn(config_str.c_str(), nullptr, &host_context);

        if (rc != 0 || !host_context)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "hostfxr_initialize_for_runtime_config failed: 0x{:x}", (uint32_t)rc);
            return false;
        }

        // Get the load_assembly_and_get_function_pointer delegate
        // hdt_load_assembly_and_get_function_pointer = 5
        rc = s_GetDelegateFn(host_context, 5, (void**)&s_LoadAssemblyFn);

        if (rc != 0 || !s_LoadAssemblyFn)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Failed to get load_assembly_and_get_function_pointer delegate: 0x{:x}", (uint32_t)rc);
            s_CloseFn(host_context);
            return false;
        }

        // We intentionally do NOT close the host context — the runtime must stay alive
        HVE_CORE_TRACE_TAG("HostFXR", "CoreCLR runtime initialized");
        return true;
    }

    void HostFXR::Shutdown()
    {
        // The runtime lives for the process lifetime — nothing to do here.
        // hostfxr_close would shut down the runtime which we don't want.
        s_LoadAssemblyFn = nullptr;
        if (s_HostFXRHandle)
        {
            PLATFORM_FREE_LIBRARY(s_HostFXRHandle);
            s_HostFXRHandle = nullptr;
        }
    }

    void* HostFXR::GetManagedFunctionPointer(
        const std::filesystem::path& assembly_path,
        const char* type_name,
        const char* method_name,
        const char* delegate_type_name)
    {
        if (!s_LoadAssemblyFn)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Runtime not initialized");
            return nullptr;
        }

        void* fn = nullptr;
        std::string asm_str = assembly_path.string();

        int32_t rc = s_LoadAssemblyFn(
            asm_str.c_str(),
            type_name,
            method_name,
            delegate_type_name,
            nullptr,
            &fn);

        if (rc != 0 || !fn)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Failed to get function pointer for {}.{}: 0x{:x}",
                type_name, method_name, (uint32_t)rc);
            return nullptr;
        }

        return fn;
    }
}
```

- [ ] **Step 3: Verify compilation**

```bash
vendor/premake/premake5 gmake2 && make config=debug Engine -j$(nproc) 2>&1 | grep "error:" | head -10
```

Expected: HostFXR.cpp compiles (no hostfxr runtime needed yet — it's loaded dynamically at runtime).

- [ ] **Step 4: Commit**

```bash
git add Engine/src/Script/HostFXR.h Engine/src/Script/HostFXR.cpp
git commit -m "feat: add HostFXR loader for .NET CoreCLR hosting"
```

---

### Task 3: Create interop struct headers (NativeEngineAPI + ManagedBridge)

**Files:**
- Create: `Engine/src/Script/NativeEngineAPI.h`
- Create: `Engine/src/Script/ManagedBridge.h`

These are the two structs that form the C++/C# boundary contract.

- [ ] **Step 1: Create Engine/src/Script/NativeEngineAPI.h**

This struct contains all 46 C++ engine function pointers that C# can call. Uses only blittable types (int, float, uint64_t, bool, float*).

```cpp
#pragma once
#include <cstdint>

namespace Engine {

    // C++ function pointers passed to C# at initialization.
    // C# stores these and calls them via delegates.
    // All parameters must be blittable (no managed types).
    struct NativeEngineAPI
    {
        // Input (3)
        bool (*IsKeyPressed)(int keycode);
        bool (*IsMouseButtonPressed)(int button);
        void (*GetMousePosition)(float* outX, float* outY);

        // Entity (2)
        bool (*EntityHasComponent)(uint64_t entityId, const char* componentName);
        void (*EntityDestroy)(uint64_t entityId);

        // Transform (6)
        void (*TransformGetTranslation)(uint64_t entityId, float* outXYZ);
        void (*TransformSetTranslation)(uint64_t entityId, float* inXYZ);
        void (*TransformGetRotation)(uint64_t entityId, float* outXYZ);
        void (*TransformSetRotation)(uint64_t entityId, float* inXYZ);
        void (*TransformGetScale)(uint64_t entityId, float* outXYZ);
        void (*TransformSetScale)(uint64_t entityId, float* inXYZ);

        // Camera (8)
        void (*CameraRotateAroundEntity)(uint64_t entityId, float* rotation2, float speed, bool inverse);
        void (*CameraRotate)(uint64_t entityId, float* rotation2, float speed, bool inverse);
        void (*CameraGetForwardDirection)(uint64_t entityId, float* outXYZ);
        void (*CameraGetRightDirection)(uint64_t entityId, float* outXYZ);
        void (*CameraGetPosition)(uint64_t entityId, float* outXYZ);
        void (*CameraGetRotation)(uint64_t entityId, float* outXYZ);
        void (*CameraSetPosition)(uint64_t entityId, float* inXYZ);
        void (*CameraSetRotation)(uint64_t entityId, float* inXYZ);

        // Sounds (2)
        void (*SoundsPlayGlobal)(uint64_t entityId, int index);
        void (*SoundsPlayLocal)(uint64_t entityId, int index);

        // Box Collider (7)
        void (*BoxColliderGetLinearVelocity)(uint64_t entityId, float* outXYZ);
        void (*BoxColliderSetLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddAngularVelocity)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddImpulse)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddAngularImpulse)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddLinearAngularImpulse)(uint64_t entityId, float* linear, float* angular);

        // Sphere Collider (7)
        void (*SphereColliderGetLinearVelocity)(uint64_t entityId, float* outXYZ);
        void (*SphereColliderSetLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddAngularVelocity)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddImpulse)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddAngularImpulse)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddLinearAngularImpulse)(uint64_t entityId, float* linear, float* angular);

        // Character Controller (11)
        void (*CharControllerGetLinearVelocity)(uint64_t entityId, float* outXYZ);
        void (*CharControllerSetLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddAngularVelocity)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddImpulse)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddAngularImpulse)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddLinearAngularImpulse)(uint64_t entityId, float* linear, float* angular);
        bool (*CharControllerIsGrounded)(uint64_t entityId);
        void (*CharControllerGetRotation)(uint64_t entityId, float* outXYZ);
        void (*CharControllerSetRotation)(uint64_t entityId, float* inXYZ);
        void (*CharControllerRotate)(uint64_t entityId, float* inXYZ);
    };

}
```

- [ ] **Step 2: Create Engine/src/Script/ManagedBridge.h**

```cpp
#pragma once
#include <cstdint>

namespace Engine {

    // Function pointers received FROM C# (the managed bridge).
    // C++ calls these to interact with the script runtime.
    struct ManagedBridge
    {
        // Assembly management
        void (*LoadAppAssembly)(const char* path);
        void (*UnloadAppAssembly)();

        // Class discovery
        int  (*GetEntityClassCount)();
        void (*GetEntityClassName)(int index, char* buffer, int bufferSize);
        bool (*EntityClassExists)(const char* fullName);

        // Instance lifecycle
        bool (*CreateInstance)(const char* className, uint64_t entityId);
        void (*DestroyInstance)(uint64_t entityId);
        void (*InvokeOnCreate)(uint64_t entityId);
        void (*InvokeOnUpdate)(uint64_t entityId, float deltaTime);
        void (*DestroyAllInstances)();

        // Component checks  
        bool (*EntityHasComponent)(uint64_t entityId, const char* componentName);
    };

}
```

- [ ] **Step 3: Commit**

```bash
git add Engine/src/Script/NativeEngineAPI.h Engine/src/Script/ManagedBridge.h
git commit -m "feat: add NativeEngineAPI and ManagedBridge interop structs"
```

---

### Task 4: Rewrite ScriptGlue to fill NativeEngineAPI

**Files:**
- Rewrite: `Engine/src/Script/ScriptGlue.h`
- Rewrite: `Engine/src/Script/ScriptGlue.cpp`

ScriptGlue's job changes from `mono_add_internal_call` registration to filling the `NativeEngineAPI` function pointer struct. The actual C++ functions that implement the engine API stay the same — they just get wired differently.

- [ ] **Step 1: Rewrite ScriptGlue.h**

```cpp
#pragma once

#include "NativeEngineAPI.h"

namespace Engine {

    class ScriptGlue
    {
    public:
        // Fill the NativeEngineAPI struct with function pointers to engine functions
        static void FillNativeAPI(NativeEngineAPI& api);
    };
}
```

- [ ] **Step 2: Rewrite ScriptGlue.cpp**

Keep all the static C++ functions (IsKeyPressed, TransformComponent_GetTranslation, etc.) but change their signatures to use blittable types (`float*` instead of `glm::vec3*`, no `MonoReflectionType*`). Remove all `mono_*` calls. Remove the `RegisterComponents` template machinery (component name resolution moves to C#).

The `Entity_HasComponent` function changes: instead of taking a `MonoReflectionType*`, it takes a `const char* componentName` string and does the lookup internally.

Wire all functions into the `NativeEngineAPI` struct in `FillNativeAPI()`.

Key changes per function category:
- **Input functions:** Signatures stay the same (already use int/bool).
- **Entity_HasComponent:** Change from `MonoReflectionType*` to `const char* componentName`. The function maps the string to the template check internally.
- **Vec3 functions:** Change `glm::vec3*` parameters to `float*` (3 floats). Read/write via pointer arithmetic: `float* xyz → glm::vec3(xyz[0], xyz[1], xyz[2])`.
- **Vec2 functions:** Change `glm::vec2*` to `float*` (2 floats).

The `FillNativeAPI` function simply assigns each function pointer:

```cpp
void ScriptGlue::FillNativeAPI(NativeEngineAPI& api)
{
    api.IsKeyPressed = IsKeyPressed;
    api.IsMouseButtonPressed = IsMouseButtonPressed;
    api.GetMousePosition = GetMousePosition;
    api.EntityHasComponent = EntityHasComponent;
    api.EntityDestroy = EntityDestroy;
    api.TransformGetTranslation = TransformGetTranslation;
    api.TransformSetTranslation = TransformSetTranslation;
    // ... all 46 function pointers
}
```

- [ ] **Step 3: Verify Engine compiles**

```bash
make config=debug Engine -j$(nproc) 2>&1 | grep "error:" | head -10
```

- [ ] **Step 4: Commit**

```bash
git add Engine/src/Script/ScriptGlue.h Engine/src/Script/ScriptGlue.cpp
git commit -m "feat: rewrite ScriptGlue to fill NativeEngineAPI function pointer struct"
```

---

### Task 5: Rewrite ScriptEngine

**Files:**
- Rewrite: `Engine/src/Script/ScriptEngine.h`
- Rewrite: `Engine/src/Script/ScriptEngine.cpp`
- Modify: `Engine/src/Scene/Components.h` (ScriptComponent)

- [ ] **Step 1: Update ScriptComponent in Components.h**

Remove the `Ref<ScriptClass>` member (ScriptClass is a Mono concept). Keep only the string name:

```cpp
struct ScriptComponent
{
    std::string Name = "";

    ScriptComponent() = default;
    ScriptComponent(const ScriptComponent&) = default;
    ScriptComponent(const std::string& name) : Name(name) {}
};
```

Remove the `#include "Script/ScriptEngine.h"` forward declaration for `ScriptClass` if present.

- [ ] **Step 2: Rewrite ScriptEngine.h**

Remove all Mono forward declarations. Remove ScriptClass and ScriptInstance classes (the managed bridge handles instantiation now). The public API stays similar but simplified:

```cpp
#pragma once

#include "ManagedBridge.h"
#include "NativeEngineAPI.h"

namespace Engine {

    class Scene;
    class Entity;

    class ScriptEngine
    {
    public:
        static void Init();
        static void Shutdown();

        static void LoadAppAssembly(const std::filesystem::path& filepath);
        static void UnloadAppAssembly();
        static void ReloadAssembly(const std::filesystem::path& app_assembly_path);

        static void OnRuntimeStart(Scene* scene);
        static void OnRuntimeStop();

        static bool EntityClassExists(const std::string& full_class_name);
        static void OnCreateEntityClass(Entity* entity);
        static void OnUpdate(float delta_time);

        static bool ShouldReload();
        static void MarkForReload();

        static Scene* GetSceneContext();

        static std::vector<std::string> GetEntityClassNames();

    private:
        static ManagedBridge s_Bridge;
        static NativeEngineAPI s_NativeAPI;
        static Scene* s_SceneContext;
        static bool s_ShouldReload;
        static std::vector<uint64_t> s_ActiveEntityIDs;
        static std::filesystem::path s_AppAssemblyPath;

        static std::unique_ptr<filewatch::FileWatch<
#ifdef PLATFORM_WINDOWS
            std::wstring
#else
            std::string
#endif
        >> s_WatcherHandle;
    };
}
```

- [ ] **Step 3: Rewrite ScriptEngine.cpp**

The implementation:

**Init():**
1. Call `ScriptGlue::FillNativeAPI(s_NativeAPI)` to populate the function pointer struct
2. Resolve path to `ScriptCore.runtimeconfig.json` and `ScriptCore.dll`
3. Call `HostFXR::Init(runtimeconfig_path)` to boot CoreCLR
4. Use `HostFXR::GetManagedFunctionPointer()` to get the bridge `Initialize` function
5. Call `Initialize(&s_NativeAPI)` which returns the `ManagedBridge` struct
6. Log success

**Shutdown():**
1. Call `s_Bridge.DestroyAllInstances()`
2. Call `s_Bridge.UnloadAppAssembly()`
3. Call `HostFXR::Shutdown()`

**LoadAppAssembly(path):**
1. Call `s_Bridge.LoadAppAssembly(path.string().c_str())`
2. Set up FileWatch on path that sets `s_ShouldReload = true`

**UnloadAppAssembly():**
1. Call `s_Bridge.DestroyAllInstances()`
2. Call `s_Bridge.UnloadAppAssembly()`

**ReloadAssembly(path):**
1. Call `UnloadAppAssembly()`
2. Call `LoadAppAssembly(path)`
3. Set `s_ShouldReload = false`
4. Log "Reloaded Scripts"

**OnRuntimeStart(scene):**
1. Set `s_SceneContext = scene`

**OnRuntimeStop():**
1. Call `s_Bridge.DestroyAllInstances()`
2. Clear `s_ActiveEntityIDs`
3. Set `s_SceneContext = nullptr`

**OnCreateEntityClass(entity):**
1. Get ScriptComponent from entity
2. Call `s_Bridge.CreateInstance(name.c_str(), entity->GetID())`
3. Call `s_Bridge.InvokeOnCreate(entity->GetID())`
4. Add entity ID to `s_ActiveEntityIDs`

**OnUpdate(delta_time):**
1. For each entity ID in `s_ActiveEntityIDs`:
   - Call `s_Bridge.InvokeOnUpdate(id, delta_time)`

**GetEntityClassNames():**
1. Call `s_Bridge.GetEntityClassCount()`
2. For each index, call `s_Bridge.GetEntityClassName(i, buffer, sizeof(buffer))`
3. Return vector of strings

- [ ] **Step 4: Verify Engine compiles**

The Engine won't link yet (no C# bridge exists), but it should compile.

```bash
make config=debug Engine -j$(nproc) 2>&1 | grep "error:" | head -10
```

- [ ] **Step 5: Commit**

```bash
git add Engine/src/Script/ScriptEngine.h Engine/src/Script/ScriptEngine.cpp Engine/src/Scene/Components.h
git commit -m "feat: rewrite ScriptEngine for CoreCLR hostfxr bridge"
```

---

### Task 6: Create ScriptCore .NET 10 project

**Files:**
- Create: `ScriptCore/ScriptCore.csproj`
- Create: `Editor/Resources/Scripts/ScriptCore.runtimeconfig.json`
- Delete: `ScriptCore/premake5.lua`

- [ ] **Step 1: Create ScriptCore/ScriptCore.csproj**

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <RootNamespace>Helios</RootNamespace>
    <AssemblyName>ScriptCore</AssemblyName>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <OutputPath>../Editor/Resources/Scripts/</OutputPath>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
    <AppendRuntimeIdentifierToOutputPath>false</AppendRuntimeIdentifierToOutputPath>
  </PropertyGroup>
</Project>
```

- [ ] **Step 2: Create Editor/Resources/Scripts/ScriptCore.runtimeconfig.json**

```json
{
  "runtimeOptions": {
    "tfm": "net10.0",
    "framework": {
      "name": "Microsoft.NETCore.App",
      "version": "10.0.0"
    }
  }
}
```

- [ ] **Step 3: Delete ScriptCore/premake5.lua**

```bash
rm ScriptCore/premake5.lua
```

- [ ] **Step 4: Delete old InternalCalls.cs and CustomAttributes.cs**

```bash
rm ScriptCore/Source/Helios/InternalCalls.cs
rm ScriptCore/Source/Helios/CustomAttributes.cs
```

- [ ] **Step 5: Commit**

```bash
git add ScriptCore/ScriptCore.csproj Editor/Resources/Scripts/ScriptCore.runtimeconfig.json
git rm ScriptCore/premake5.lua ScriptCore/Source/Helios/InternalCalls.cs ScriptCore/Source/Helios/CustomAttributes.cs
git commit -m "feat: create ScriptCore .NET 10 project, remove Mono build files"
```

---

### Task 7: Create C# bridge (ScriptHostBridge, ALC, NativeAPI)

**Files:**
- Create: `ScriptCore/Source/Helios/Bridge/NativeEngineAPI.cs`
- Create: `ScriptCore/Source/Helios/Bridge/NativeAPI.cs`
- Create: `ScriptCore/Source/Helios/Bridge/ScriptAssemblyLoadContext.cs`
- Create: `ScriptCore/Source/Helios/Bridge/ScriptHostBridge.cs`

This is the core of the migration — the managed side of the C++/C# bridge.

- [ ] **Step 1: Create NativeEngineAPI.cs — C# mirror of the C++ struct**

```csharp
using System.Runtime.InteropServices;

namespace Helios.Bridge;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeEngineAPI
{
    // Input
    public delegate* unmanaged<int, bool> IsKeyPressed;
    public delegate* unmanaged<int, bool> IsMouseButtonPressed;
    public delegate* unmanaged<float*, float*, void> GetMousePosition;

    // Entity
    public delegate* unmanaged<ulong, byte*, bool> EntityHasComponent;
    public delegate* unmanaged<ulong, void> EntityDestroy;

    // Transform
    public delegate* unmanaged<ulong, float*, void> TransformGetTranslation;
    public delegate* unmanaged<ulong, float*, void> TransformSetTranslation;
    public delegate* unmanaged<ulong, float*, void> TransformGetRotation;
    public delegate* unmanaged<ulong, float*, void> TransformSetRotation;
    public delegate* unmanaged<ulong, float*, void> TransformGetScale;
    public delegate* unmanaged<ulong, float*, void> TransformSetScale;

    // Camera
    public delegate* unmanaged<ulong, float*, float, bool, void> CameraRotateAroundEntity;
    public delegate* unmanaged<ulong, float*, float, bool, void> CameraRotate;
    public delegate* unmanaged<ulong, float*, void> CameraGetForwardDirection;
    public delegate* unmanaged<ulong, float*, void> CameraGetRightDirection;
    public delegate* unmanaged<ulong, float*, void> CameraGetPosition;
    public delegate* unmanaged<ulong, float*, void> CameraGetRotation;
    public delegate* unmanaged<ulong, float*, void> CameraSetPosition;
    public delegate* unmanaged<ulong, float*, void> CameraSetRotation;

    // Sounds
    public delegate* unmanaged<ulong, int, void> SoundsPlayGlobal;
    public delegate* unmanaged<ulong, int, void> SoundsPlayLocal;

    // Box Collider
    public delegate* unmanaged<ulong, float*, void> BoxColliderGetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderSetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddAngularVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddImpulse;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddAngularImpulse;
    public delegate* unmanaged<ulong, float*, float*, void> BoxColliderAddLinearAngularImpulse;

    // Sphere Collider
    public delegate* unmanaged<ulong, float*, void> SphereColliderGetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderSetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddAngularVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddImpulse;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddAngularImpulse;
    public delegate* unmanaged<ulong, float*, float*, void> SphereColliderAddLinearAngularImpulse;

    // Character Controller
    public delegate* unmanaged<ulong, float*, void> CharControllerGetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerSetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddAngularVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddImpulse;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddAngularImpulse;
    public delegate* unmanaged<ulong, float*, float*, void> CharControllerAddLinearAngularImpulse;
    public delegate* unmanaged<ulong, bool> CharControllerIsGrounded;
    public delegate* unmanaged<ulong, float*, void> CharControllerGetRotation;
    public delegate* unmanaged<ulong, float*, void> CharControllerSetRotation;
    public delegate* unmanaged<ulong, float*, void> CharControllerRotate;
}
```

- [ ] **Step 2: Create NativeAPI.cs — public wrapper for engine calls**

```csharp
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Text;

namespace Helios;

public static unsafe class NativeAPI
{
    internal static NativeEngineAPI* Api;

    internal static void Init(NativeEngineAPI* api) => Api = api;

    // Input
    public static bool IsKeyPressed(int key) => Api->IsKeyPressed(key);
    public static bool IsMouseButtonPressed(int button) => Api->IsMouseButtonPressed(button);
    public static Vector2 GetMousePosition()
    {
        float x, y;
        Api->GetMousePosition(&x, &y);
        return new Vector2(x, y);
    }

    // Entity
    internal static bool EntityHasComponent(ulong entityId, string componentName)
    {
        var bytes = Encoding.UTF8.GetBytes(componentName + '\0');
        fixed (byte* ptr = bytes)
            return Api->EntityHasComponent(entityId, ptr);
    }

    internal static void EntityDestroy(ulong entityId) => Api->EntityDestroy(entityId);

    // Transform
    internal static Vector3 TransformGetTranslation(ulong id)
    {
        Vector3 v; Api->TransformGetTranslation(id, (float*)&v); return v;
    }
    internal static void TransformSetTranslation(ulong id, Vector3 v)
        => Api->TransformSetTranslation(id, (float*)&v);
    internal static Vector3 TransformGetRotation(ulong id)
    {
        Vector3 v; Api->TransformGetRotation(id, (float*)&v); return v;
    }
    internal static void TransformSetRotation(ulong id, Vector3 v)
        => Api->TransformSetRotation(id, (float*)&v);
    internal static Vector3 TransformGetScale(ulong id)
    {
        Vector3 v; Api->TransformGetScale(id, (float*)&v); return v;
    }
    internal static void TransformSetScale(ulong id, Vector3 v)
        => Api->TransformSetScale(id, (float*)&v);

    // Camera
    internal static void CameraRotateAroundEntity(ulong id, Vector2 rot, float speed, bool inv)
        => Api->CameraRotateAroundEntity(id, (float*)&rot, speed, inv);
    internal static void CameraRotate(ulong id, Vector2 rot, float speed, bool inv)
        => Api->CameraRotate(id, (float*)&rot, speed, inv);
    internal static Vector3 CameraGetForwardDirection(ulong id)
    {
        Vector3 v; Api->CameraGetForwardDirection(id, (float*)&v); return v;
    }
    internal static Vector3 CameraGetRightDirection(ulong id)
    {
        Vector3 v; Api->CameraGetRightDirection(id, (float*)&v); return v;
    }
    internal static Vector3 CameraGetPosition(ulong id)
    {
        Vector3 v; Api->CameraGetPosition(id, (float*)&v); return v;
    }
    internal static void CameraSetPosition(ulong id, Vector3 v)
        => Api->CameraSetPosition(id, (float*)&v);
    internal static Vector3 CameraGetRotation(ulong id)
    {
        Vector3 v; Api->CameraGetRotation(id, (float*)&v); return v;
    }
    internal static void CameraSetRotation(ulong id, Vector3 v)
        => Api->CameraSetRotation(id, (float*)&v);

    // Sounds
    internal static void SoundsPlayGlobal(ulong id, int index) => Api->SoundsPlayGlobal(id, index);
    internal static void SoundsPlayLocal(ulong id, int index) => Api->SoundsPlayLocal(id, index);

    // Box Collider
    internal static Vector3 BoxColliderGetLinearVelocity(ulong id)
    {
        Vector3 v; Api->BoxColliderGetLinearVelocity(id, (float*)&v); return v;
    }
    internal static void BoxColliderSetLinearVelocity(ulong id, Vector3 v)
        => Api->BoxColliderSetLinearVelocity(id, (float*)&v);
    internal static void BoxColliderAddLinearVelocity(ulong id, Vector3 v)
        => Api->BoxColliderAddLinearVelocity(id, (float*)&v);
    internal static void BoxColliderAddAngularVelocity(ulong id, Vector3 v)
        => Api->BoxColliderAddAngularVelocity(id, (float*)&v);
    internal static void BoxColliderAddImpulse(ulong id, Vector3 v)
        => Api->BoxColliderAddImpulse(id, (float*)&v);
    internal static void BoxColliderAddAngularImpulse(ulong id, Vector3 v)
        => Api->BoxColliderAddAngularImpulse(id, (float*)&v);
    internal static void BoxColliderAddLinearAngularImpulse(ulong id, Vector3 lin, Vector3 ang)
        => Api->BoxColliderAddLinearAngularImpulse(id, (float*)&lin, (float*)&ang);

    // Sphere Collider (same pattern as Box)
    internal static Vector3 SphereColliderGetLinearVelocity(ulong id)
    {
        Vector3 v; Api->SphereColliderGetLinearVelocity(id, (float*)&v); return v;
    }
    internal static void SphereColliderSetLinearVelocity(ulong id, Vector3 v)
        => Api->SphereColliderSetLinearVelocity(id, (float*)&v);
    internal static void SphereColliderAddLinearVelocity(ulong id, Vector3 v)
        => Api->SphereColliderAddLinearVelocity(id, (float*)&v);
    internal static void SphereColliderAddAngularVelocity(ulong id, Vector3 v)
        => Api->SphereColliderAddAngularVelocity(id, (float*)&v);
    internal static void SphereColliderAddImpulse(ulong id, Vector3 v)
        => Api->SphereColliderAddImpulse(id, (float*)&v);
    internal static void SphereColliderAddAngularImpulse(ulong id, Vector3 v)
        => Api->SphereColliderAddAngularImpulse(id, (float*)&v);
    internal static void SphereColliderAddLinearAngularImpulse(ulong id, Vector3 lin, Vector3 ang)
        => Api->SphereColliderAddLinearAngularImpulse(id, (float*)&lin, (float*)&ang);

    // Character Controller
    internal static Vector3 CharControllerGetLinearVelocity(ulong id)
    {
        Vector3 v; Api->CharControllerGetLinearVelocity(id, (float*)&v); return v;
    }
    internal static void CharControllerSetLinearVelocity(ulong id, Vector3 v)
        => Api->CharControllerSetLinearVelocity(id, (float*)&v);
    internal static void CharControllerAddLinearVelocity(ulong id, Vector3 v)
        => Api->CharControllerAddLinearVelocity(id, (float*)&v);
    internal static void CharControllerAddAngularVelocity(ulong id, Vector3 v)
        => Api->CharControllerAddAngularVelocity(id, (float*)&v);
    internal static void CharControllerAddImpulse(ulong id, Vector3 v)
        => Api->CharControllerAddImpulse(id, (float*)&v);
    internal static void CharControllerAddAngularImpulse(ulong id, Vector3 v)
        => Api->CharControllerAddAngularImpulse(id, (float*)&v);
    internal static void CharControllerAddLinearAngularImpulse(ulong id, Vector3 lin, Vector3 ang)
        => Api->CharControllerAddLinearAngularImpulse(id, (float*)&lin, (float*)&ang);
    internal static bool CharControllerIsGrounded(ulong id)
        => Api->CharControllerIsGrounded(id);
    internal static Vector3 CharControllerGetRotation(ulong id)
    {
        Vector3 v; Api->CharControllerGetRotation(id, (float*)&v); return v;
    }
    internal static void CharControllerSetRotation(ulong id, Vector3 v)
        => Api->CharControllerSetRotation(id, (float*)&v);
    internal static void CharControllerRotate(ulong id, Vector3 v)
        => Api->CharControllerRotate(id, (float*)&v);
}
```

- [ ] **Step 3: Create ScriptAssemblyLoadContext.cs**

```csharp
using System.Reflection;
using System.Runtime.Loader;

namespace Helios.Bridge;

internal class ScriptAssemblyLoadContext : AssemblyLoadContext
{
    public ScriptAssemblyLoadContext() : base(isCollectible: true) { }

    protected override Assembly? Load(AssemblyName assemblyName) => null;
}
```

- [ ] **Step 4: Create ScriptHostBridge.cs**

This is the main entry point called from C++. It initializes the NativeAPI, manages the collectible ALC, and exposes all bridge methods as `[UnmanagedCallersOnly]`.

```csharp
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Helios.Bridge;

public static unsafe class ScriptHostBridge
{
    private static ScriptAssemblyLoadContext? s_ScriptALC;
    private static Assembly? s_AppAssembly;
    private static Type? s_EntityBaseType;

    // Discovered entity classes from the loaded app assembly
    private static readonly List<Type> s_EntityClasses = new();

    // Active script instances keyed by entity ID
    private static readonly Dictionary<ulong, object> s_Instances = new();

    // Cached methods per type
    private static readonly Dictionary<Type, MethodInfo?> s_OnCreateMethods = new();
    private static readonly Dictionary<Type, MethodInfo?> s_OnUpdateMethods = new();

    /// <summary>
    /// Called from C++ to initialize the bridge.
    /// Receives a pointer to NativeEngineAPI and returns a ManagedBridge struct.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void Initialize(NativeEngineAPI* nativeApi, ManagedBridgeNative* outBridge)
    {
        NativeAPI.Init(nativeApi);

        // Cache the Entity base type from ScriptCore
        s_EntityBaseType = typeof(Entity);

        // Fill the outgoing bridge struct with function pointers
        outBridge->LoadAppAssembly = &LoadAppAssembly;
        outBridge->UnloadAppAssembly = &UnloadAppAssembly;
        outBridge->GetEntityClassCount = &GetEntityClassCount;
        outBridge->GetEntityClassName = &GetEntityClassName;
        outBridge->EntityClassExists = &EntityClassExists;
        outBridge->CreateInstance = &CreateInstance;
        outBridge->DestroyInstance = &DestroyInstance;
        outBridge->InvokeOnCreate = &InvokeOnCreate;
        outBridge->InvokeOnUpdate = &InvokeOnUpdate;
        outBridge->DestroyAllInstances = &DestroyAllInstances;
        outBridge->EntityHasComponent = &EntityHasComponent;
    }

    [UnmanagedCallersOnly]
    public static void LoadAppAssembly(byte* pathUtf8)
    {
        string path = Marshal.PtrToStringUTF8((IntPtr)pathUtf8)!;

        s_ScriptALC = new ScriptAssemblyLoadContext();

        using var stream = File.OpenRead(path);
        s_AppAssembly = s_ScriptALC.LoadFromStream(stream);

        // Discover entity subclasses
        s_EntityClasses.Clear();
        s_OnCreateMethods.Clear();
        s_OnUpdateMethods.Clear();

        foreach (var type in s_AppAssembly.GetTypes())
        {
            if (type.IsSubclassOf(s_EntityBaseType!) && !type.IsAbstract)
            {
                s_EntityClasses.Add(type);
                s_OnCreateMethods[type] = type.GetMethod("OnCreate", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic, Type.EmptyTypes);
                s_OnUpdateMethods[type] = type.GetMethod("OnUpdate", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic, new[] { typeof(float) });
            }
        }
    }

    [UnmanagedCallersOnly]
    public static void UnloadAppAssembly()
    {
        s_Instances.Clear();
        s_EntityClasses.Clear();
        s_OnCreateMethods.Clear();
        s_OnUpdateMethods.Clear();
        s_AppAssembly = null;

        s_ScriptALC?.Unload();
        s_ScriptALC = null;
    }

    [UnmanagedCallersOnly]
    public static int GetEntityClassCount() => s_EntityClasses.Count;

    [UnmanagedCallersOnly]
    public static void GetEntityClassName(int index, byte* buffer, int bufferSize)
    {
        if (index < 0 || index >= s_EntityClasses.Count) return;
        var name = s_EntityClasses[index].FullName ?? s_EntityClasses[index].Name;
        var bytes = System.Text.Encoding.UTF8.GetBytes(name);
        int len = Math.Min(bytes.Length, bufferSize - 1);
        Marshal.Copy(bytes, 0, (IntPtr)buffer, len);
        buffer[len] = 0;
    }

    [UnmanagedCallersOnly]
    public static bool EntityClassExists(byte* nameUtf8)
    {
        string name = Marshal.PtrToStringUTF8((IntPtr)nameUtf8)!;
        return s_EntityClasses.Any(t => t.FullName == name || t.Name == name);
    }

    [UnmanagedCallersOnly]
    public static bool CreateInstance(byte* classNameUtf8, ulong entityId)
    {
        string className = Marshal.PtrToStringUTF8((IntPtr)classNameUtf8)!;
        var type = s_EntityClasses.FirstOrDefault(t => t.FullName == className || t.Name == className);
        if (type == null) return false;

        // Create instance via the internal Entity(ulong) constructor
        var ctor = type.GetConstructor(
            BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public,
            null, Type.EmptyTypes, null);

        if (ctor == null) return false;

        var instance = ctor.Invoke(null);

        // Set the ID field via reflection (it's readonly, use the internal constructor pattern)
        // The Entity base class has a protected parameterless constructor that sets ID = 0.
        // We need to set it after construction.
        var idField = typeof(Entity).GetField("ID", BindingFlags.Instance | BindingFlags.Public);
        idField?.SetValue(instance, entityId);

        s_Instances[entityId] = instance;
        return true;
    }

    [UnmanagedCallersOnly]
    public static void DestroyInstance(ulong entityId)
    {
        s_Instances.Remove(entityId);
    }

    [UnmanagedCallersOnly]
    public static void InvokeOnCreate(ulong entityId)
    {
        if (!s_Instances.TryGetValue(entityId, out var instance)) return;
        var type = instance.GetType();
        if (s_OnCreateMethods.TryGetValue(type, out var method) && method != null)
        {
            method.Invoke(instance, null);
        }
    }

    [UnmanagedCallersOnly]
    public static void InvokeOnUpdate(ulong entityId, float deltaTime)
    {
        if (!s_Instances.TryGetValue(entityId, out var instance)) return;
        var type = instance.GetType();
        if (s_OnUpdateMethods.TryGetValue(type, out var method) && method != null)
        {
            method.Invoke(instance, new object[] { deltaTime });
        }
    }

    [UnmanagedCallersOnly]
    public static void DestroyAllInstances()
    {
        s_Instances.Clear();
    }

    [UnmanagedCallersOnly]
    public static bool EntityHasComponent(ulong entityId, byte* componentNameUtf8)
    {
        string name = Marshal.PtrToStringUTF8((IntPtr)componentNameUtf8)!;
        return NativeAPI.EntityHasComponent(entityId, name);
    }
}

/// <summary>
/// C# mirror of the C++ ManagedBridge struct.
/// Filled by Initialize() and read by C++.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ManagedBridgeNative
{
    public delegate* unmanaged<byte*, void> LoadAppAssembly;
    public delegate* unmanaged<void> UnloadAppAssembly;
    public delegate* unmanaged<int> GetEntityClassCount;
    public delegate* unmanaged<int, byte*, int, void> GetEntityClassName;
    public delegate* unmanaged<byte*, bool> EntityClassExists;
    public delegate* unmanaged<byte*, ulong, bool> CreateInstance;
    public delegate* unmanaged<ulong, void> DestroyInstance;
    public delegate* unmanaged<ulong, void> InvokeOnCreate;
    public delegate* unmanaged<ulong, float, void> InvokeOnUpdate;
    public delegate* unmanaged<void> DestroyAllInstances;
    public delegate* unmanaged<ulong, byte*, bool> EntityHasComponent;
}
```

- [ ] **Step 5: Commit**

```bash
git add ScriptCore/Source/Helios/Bridge/
git commit -m "feat: add C# managed bridge (ScriptHostBridge, NativeAPI, ALC)"
```

---

### Task 8: Rewrite C# API (Entity, Components, Input)

**Files:**
- Rewrite: `ScriptCore/Source/Helios/Scene/Entity.cs`
- Rewrite: `ScriptCore/Source/Helios/Scene/Components.cs`
- Create: `ScriptCore/Source/Helios/Input.cs`
- Keep unchanged: `ScriptCore/Source/Helios/KeyCodes.cs`

- [ ] **Step 1: Create Input.cs**

```csharp
using System.Numerics;

namespace Helios;

public static class Input
{
    public static bool IsKeyPressed(KeyCode key) => NativeAPI.IsKeyPressed((int)key);
    public static bool IsMouseButtonPressed(MouseButton button) => NativeAPI.IsMouseButtonPressed((int)button);
    public static Vector2 MousePosition => NativeAPI.GetMousePosition();
}
```

- [ ] **Step 2: Rewrite Entity.cs — remove input methods**

```csharp
using System;
using System.Numerics;

namespace Helios;

public class Entity
{
    protected Entity() { ID = 0; }
    internal Entity(ulong id) { ID = id; }

    public ulong ID;

    public void Destroy() => NativeAPI.EntityDestroy(ID);

    public void DestroyEntity(ulong id) => NativeAPI.EntityDestroy(id);

    public bool HasComponent<T>() where T : Component, new()
    {
        return NativeAPI.EntityHasComponent(ID, typeof(T).Name);
    }

    public T? GetComponent<T>() where T : Component, new()
    {
        if (!HasComponent<T>()) return null;
        return new T() { Entity = this };
    }
}
```

Note: `ID` changed from `readonly` to mutable so the bridge can set it after construction.

- [ ] **Step 3: Rewrite Components.cs — replace InternalCalls with NativeAPI**

Every `InternalCalls.X()` call becomes `NativeAPI.X()`. The public API of each component stays identical. For example:

```csharp
using System;
using System.Numerics;

namespace Helios;

public abstract class Component
{
    public Entity Entity { get; internal set; } = null!;
}

public class TransformComponent : Component
{
    public Vector3 Translation
    {
        get => NativeAPI.TransformGetTranslation(Entity.ID);
        set => NativeAPI.TransformSetTranslation(Entity.ID, value);
    }

    public Vector3 Rotation
    {
        get => NativeAPI.TransformGetRotation(Entity.ID);
        set => NativeAPI.TransformSetRotation(Entity.ID, value);
    }

    public Vector3 Scale
    {
        get => NativeAPI.TransformGetScale(Entity.ID);
        set => NativeAPI.TransformSetScale(Entity.ID, value);
    }
}

// CameraComponent, BoxColliderComponent, SphereColliderComponent,
// CharacterControllerComponent, GlobalSoundsComponent, LocalSoundsComponent
// follow the same pattern: replace InternalCalls.X with NativeAPI.X
```

All component classes follow the same mechanical transformation. The full Components.cs must include all components with their complete implementations.

- [ ] **Step 4: Build ScriptCore**

```bash
dotnet build ScriptCore/ScriptCore.csproj
```

Expected: Build succeeds, `Editor/Resources/Scripts/ScriptCore.dll` is produced.

- [ ] **Step 5: Commit**

```bash
git add ScriptCore/Source/Helios/
git commit -m "feat: rewrite C# API with NativeAPI bridge (Entity, Components, Input)"
```

---

### Task 9: Update ProjectSerializer for dotnet build

**Files:**
- Modify: `Engine/src/Project/ProjectSerializer.cpp`

- [ ] **Step 1: Rewrite CreateScriptProject()**

Replace the premake+csc project generation with `dotnet new classlib` + `dotnet build`:

```cpp
void ProjectSerializer::CreateScriptProject()
{
    auto& project_settings = Project::GetActive()->GetSettings();

    std::filesystem::path script_project_path = project_settings.RootPath / "ScriptProject";
    std::filesystem::create_directory(script_project_path);

    // Create .csproj file
    std::ofstream csproj(script_project_path / (project_settings.ProjectName + ".csproj"));
    csproj << "<Project Sdk=\"Microsoft.NET.Sdk\">\n";
    csproj << "  <PropertyGroup>\n";
    csproj << "    <TargetFramework>net10.0</TargetFramework>\n";
    csproj << "    <RootNamespace>" << project_settings.ProjectName << "</RootNamespace>\n";
    csproj << "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n";
    csproj << "    <OutputPath>../Binaries/</OutputPath>\n";
    csproj << "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n";
    csproj << "    <AppendRuntimeIdentifierToOutputPath>false</AppendRuntimeIdentifierToOutputPath>\n";
    csproj << "  </PropertyGroup>\n";
    csproj << "  <ItemGroup>\n";

    std::filesystem::path root_path = ROOT_PATH;
    root_path = root_path.parent_path();
    std::filesystem::path scriptcore_dll = root_path / "Editor/Resources/Scripts/ScriptCore.dll";
    csproj << "    <Reference Include=\"ScriptCore\">\n";
    csproj << "      <HintPath>" << scriptcore_dll.string() << "</HintPath>\n";
    csproj << "    </Reference>\n";

    csproj << "  </ItemGroup>\n";
    csproj << "  <ItemGroup>\n";
    csproj << "    <Compile Include=\"../Assets/Scripts/**/*.cs\" />\n";
    csproj << "  </ItemGroup>\n";
    csproj << "</Project>\n";
    csproj.close();

    // Build
    CommandArgs args{};
    args.SleepUntilFinished = true;
    std::string command = "dotnet build " + (script_project_path / (project_settings.ProjectName + ".csproj")).string();
    CommandLine::Create()->ExecuteCommand(command, args);

    project_settings.ScriptAssemblyPath = std::filesystem::path("Binaries/" + project_settings.ProjectName + ".dll");
}
```

Also remove the `CreatePremakeFile` function entirely — it's no longer needed.

- [ ] **Step 2: Commit**

```bash
git add Engine/src/Project/ProjectSerializer.cpp Engine/src/Project/ProjectSerializer.h
git commit -m "feat: replace premake+csc script project generation with dotnet build"
```

---

### Task 10: Update vendor setup script for .NET 10 runtime

**Files:**
- Modify: `setup_linux_vendor.sh`

- [ ] **Step 1: Add .NET 10 runtime download section**

Add after the Mono section (or replace it). Download the self-contained .NET 10 runtime:

```bash
# ─── Download .NET 10 Runtime ─────────────────────────────────────────────────
DOTNET_DIR="$VENDOR_DIR/dotnet"
if [ -d "$DOTNET_DIR/shared/Microsoft.NETCore.App" ]; then
    log_info ".NET runtime already exists, skipping download."
else
    log_info "Downloading .NET 10 runtime..."
    DOTNET_VERSION="10.0.0-preview.4.25258.110"
    DOTNET_URL="https://download.visualstudio.microsoft.com/download/pr/.../dotnet-runtime-${DOTNET_VERSION}-linux-x64.tar.gz"
    # Use the dotnet-install script for reliability
    curl -sSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
    chmod +x /tmp/dotnet-install.sh
    /tmp/dotnet-install.sh --channel 10.0 --runtime dotnet --install-dir "$DOTNET_DIR"
    log_info ".NET 10 runtime installed to $DOTNET_DIR"
fi

# Create include directory with hostfxr headers
mkdir -p "$DOTNET_DIR/include"
if [ ! -f "$DOTNET_DIR/include/hostfxr.h" ]; then
    log_info "Downloading hostfxr headers..."
    DOTNET_HEADERS_BASE="https://raw.githubusercontent.com/dotnet/runtime/main/src/native/corehost"
    curl -sSL "$DOTNET_HEADERS_BASE/hostfxr.h" -o "$DOTNET_DIR/include/hostfxr.h"
    curl -sSL "$DOTNET_HEADERS_BASE/coreclr_delegates.h" -o "$DOTNET_DIR/include/coreclr_delegates.h"
    curl -sSL "$DOTNET_HEADERS_BASE/nethost/nethost.h" -o "$DOTNET_DIR/include/nethost.h"
    log_info "hostfxr headers downloaded."
fi
```

Remove the Mono setup section entirely.

- [ ] **Step 2: Commit**

```bash
git add setup_linux_vendor.sh
git commit -m "feat: replace Mono with .NET 10 runtime in vendor setup script"
```

---

### Task 11: Integration build and test

- [ ] **Step 1: Run the vendor setup script**

```bash
./setup_linux_vendor.sh
```

Expected: .NET 10 runtime downloaded to `Engine/vendor/dotnet/`, assimp built.

- [ ] **Step 2: Build ScriptCore**

```bash
dotnet build ScriptCore/ScriptCore.csproj
```

Expected: `Editor/Resources/Scripts/ScriptCore.dll` produced.

- [ ] **Step 3: Generate and build C++ engine**

```bash
./generate_linux_projects.sh
make config=debug -j$(nproc)
```

Expected: All C++ targets compile and link. Editor and EditorLauncher are valid ELF executables.

- [ ] **Step 4: Run the Editor**

```bash
cd Editor && ../bin/Debug-linux-x86_64/Editor/Editor
```

Expected: Window opens, CoreCLR initializes (check log for "CoreCLR runtime initialized"), scene loads. Script components won't run until a user script assembly is built, but the engine should not crash.

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "feat: complete Mono to .NET 10 CoreCLR migration"
```

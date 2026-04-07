# C# Scripting System -- Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a C# scripting system that loads .NET assemblies via CoreCLR (HostFXR), bridges C++/C# through blittable function pointer structs, executes per-entity script instances as ECS systems, and supports hot reload. Adapted from the existing `Engine/src/Script/` code, rewritten to eliminate all global/static state and integrate with the new ECS architecture.

**Architecture:** `ScriptRuntime` is an abstract interface (for testing/mocking). `CoreCLRRuntime` implements it using HostFXR -- RAII lifetime (constructor boots the runtime, destructor shuts it down). `ManagedBridge` holds function pointers received FROM C# for C++ to call into managed code. `NativeEngineAPI` holds function pointers passed TO C# for managed code to call back into C++. `ScriptGlue` populates `NativeEngineAPI` with implementations that operate on a `World*` context (no globals). `ScriptExecutionSystem` runs in `Schedule::Update`, groups entities by `script_type_id`, and invokes updates in batches. A `std::jthread` file watcher monitors for .cs/.dll changes and triggers hot reload via collectible `AssemblyLoadContext`.

**Tech Stack:** C++20, .NET 10 (CoreCLR via HostFXR), C# 13, `std::jthread` (file watcher), `std::atomic` (reload flag)

**Spec:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` -- Section 7 (Scripting)

**Existing code to adapt:**
- `Engine/src/Script/HostFXR.h/.cpp` -- dynamic library loading, runtime init, managed function resolution
- `Engine/src/Script/ScriptEngine.h/.cpp` -- assembly load, entity instance lifecycle, update loop
- `Engine/src/Script/ManagedBridge.h` -- C++ -> C# function pointer struct
- `Engine/src/Script/NativeEngineAPI.h` -- C# -> C++ function pointer struct
- `Engine/src/Script/ScriptGlue.h/.cpp` -- fills NativeEngineAPI with implementations
- `ScriptCore/` -- C# side: Entity, Component wrappers, bridge, assembly load context

**Key design changes from existing code:**
1. **No static state.** `ScriptRuntime` is a resource in the World, not a static class. `ScriptGlue` receives `World*` as context, not `ScriptEngine::GetSceneContext()`.
2. **RAII.** `CoreCLRRuntime` constructor boots HostFXR + initializes runtime. Destructor shuts down. No `Init()`/`Shutdown()`.
3. **Abstract interface.** `ScriptRuntime` is pure virtual for testability. `CoreCLRRuntime` is the real implementation.
4. **Batch execution.** `ScriptExecutionSystem` groups by `script_type_id` and calls `invoke_update(type_id, handles[], delta)` per batch. Different types can run in parallel.
5. **ECS integration.** `ScriptingPlugin` registers systems. `ScriptInstance` is a component. `ScriptRuntime` is a resource.

**Dependencies (assumed complete):**
- Plans 1-2: ECS (World, Entity, Query, Commands, Res/ResMut, EventReader/EventWriter, App, Plugin, Scheduler, Schedule)
- Plan 9: Physics events (CollisionEvent for collision callbacks from scripts)

---

## Task 1: Create `helios-script` CMake target structure

**Files:**
- Create: `helios-script/CMakeLists.txt`
- Create: `helios-script/include/helios/script/script_runtime.h`
- Create: `helios-script/include/helios/script/managed_bridge.h`
- Create: `helios-script/include/helios/script/native_engine_api.h`
- Create: `helios-script/include/helios/script/script_glue.h`
- Create: `helios-script/include/helios/script/script_instance.h`
- Create: `helios-script/include/helios/script/scripting_plugin.h`
- Modify: top-level `CMakeLists.txt` (add `add_subdirectory(helios-script)`)

This task creates the directory layout and CMake target. All headers start as stubs -- content is filled in subsequent tasks.

- [ ] **Step 1: Create the directory structure**

```bash
mkdir -p helios-script/include/helios/script
mkdir -p helios-script/src
```

- [ ] **Step 2: Create `helios-script/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)

add_library(helios-script STATIC
    src/coreclr_runtime.cpp
    src/script_glue.cpp
    src/script_execution_system.cpp
    src/scripting_plugin.cpp
    src/file_watcher.cpp
)

target_include_directories(helios-script
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(helios-script
    PUBLIC  helios-core
    PRIVATE ${CMAKE_DL_LIBS}   # dlopen/dlsym on Linux
)

target_compile_features(helios-script PUBLIC cxx_std_20)

# HostFXR headers -- vendored .NET SDK headers
# The runtime locates hostfxr dynamically at runtime via dlopen,
# but we need the header-only types at compile time.
target_include_directories(helios-script
    PRIVATE ${CMAKE_SOURCE_DIR}/Engine/vendor/dotnet/include
)
```

- [ ] **Step 3: Add `helios-script` to the top-level CMakeLists.txt**

Add after the `helios-core` subdirectory:

```cmake
add_subdirectory(helios-script)
```

- [ ] **Step 4: Create stub headers**

Create each header file with `#pragma once` and a forward-declaring namespace block. Content for each is provided in subsequent tasks. Example for `script_runtime.h`:

```cpp
// helios-script/include/helios/script/script_runtime.h
#pragma once

namespace helios {
    class ScriptRuntime;
} // namespace helios
```

Repeat for all headers listed above.

- [ ] **Step 5: Create empty `.cpp` stubs**

Create the following files with just an `#include` of their corresponding header so the CMake target has something to compile:
- `helios-script/src/coreclr_runtime.cpp`
- `helios-script/src/script_glue.cpp`
- `helios-script/src/script_execution_system.cpp`
- `helios-script/src/scripting_plugin.cpp`
- `helios-script/src/file_watcher.cpp`

- [ ] **Step 6: Verify the target builds (empty stubs)**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target helios-script -j$(nproc) 2>&1 | tail -5
```

---

## Task 2: `ScriptRuntime` abstract interface

**Files:**
- Modify: `helios-script/include/helios/script/script_runtime.h`

This is the testable abstract interface. All interactions with the managed runtime go through this. `CoreCLRRuntime` (Task 4) is the production implementation. Tests use a mock (Task 14).

- [ ] **Step 1: Define the `ScriptRuntime` interface**

```cpp
// helios-script/include/helios/script/script_runtime.h
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace helios {

/// Abstract interface for a managed scripting runtime.
/// Production implementation: CoreCLRRuntime.
/// Test implementation: MockScriptRuntime (see tests).
class ScriptRuntime {
public:
    virtual ~ScriptRuntime() = default;

    // ── Assembly management ──────────────────────────────────────

    /// Load a game assembly (.dll) from disk.
    /// Returns true on success.
    virtual bool load_assembly(const std::filesystem::path& assembly_path) = 0;

    /// Unload the currently loaded game assembly.
    /// All script instances must be destroyed first.
    virtual void unload_assembly() = 0;

    /// Reload the game assembly. Internally: unload -> load.
    /// Caller is responsible for serializing/deserializing script state.
    virtual bool reload_assembly(const std::filesystem::path& assembly_path) = 0;

    // ── Class discovery ──────────────────────────────────────────

    /// Returns true if the given fully-qualified class name exists
    /// as a Script subclass in the loaded assembly.
    virtual bool class_exists(const std::string& full_class_name) const = 0;

    /// Returns all Script subclass names found in the loaded assembly.
    virtual std::vector<std::string> get_script_class_names() const = 0;

    /// Returns a numeric type ID for a script class name.
    /// The same name always produces the same ID within a single
    /// assembly load (but IDs may change across reloads).
    virtual uint32_t get_script_type_id(const std::string& full_class_name) const = 0;

    // ── Instance lifecycle ───────────────────────────────────────

    /// Create a managed script instance for the given entity.
    /// Returns a managed handle (opaque uint64_t), or 0 on failure.
    virtual uint64_t invoke_create(const std::string& class_name, uint64_t entity_id) = 0;

    /// Call OnUpdate on a batch of entities sharing the same script type.
    /// entity_ids and managed_handles are parallel arrays of size count.
    virtual void invoke_update(uint32_t script_type_id,
                               const uint64_t* entity_ids,
                               const uint64_t* managed_handles,
                               size_t count,
                               float delta) = 0;

    /// Destroy the managed instance for a single entity.
    virtual void invoke_destroy(uint64_t entity_id, uint64_t managed_handle) = 0;

    /// Destroy all managed instances (e.g., on scene unload).
    virtual void destroy_all_instances() = 0;

    // ── Hot reload support ───────────────────────────────────────

    /// Request a reload on the next check_script_reload tick.
    virtual void request_reload() = 0;

    /// Returns true if a reload has been requested.
    virtual bool reload_requested() const = 0;

    /// Clear the reload request flag (called after reload completes).
    virtual void clear_reload_request() = 0;
};

} // namespace helios
```

---

## Task 3: `ManagedBridge` struct (C++ -> C# calls)

**Files:**
- Modify: `helios-script/include/helios/script/managed_bridge.h`

Function pointers received FROM C# during initialization. C++ calls these to interact with the managed runtime. Adapted from existing `Engine/src/Script/ManagedBridge.h` with additions for batch update and OnDestroy.

- [ ] **Step 1: Define the `ManagedBridge` struct**

```cpp
// helios-script/include/helios/script/managed_bridge.h
#pragma once

#include <cstdint>

namespace helios {

/// Function pointers received FROM the C# managed bridge.
/// C++ calls these to control script instances.
/// All pointer types are blittable -- no managed types, no C++ objects.
/// Layout must exactly match the C# ManagedBridgeNative struct.
struct ManagedBridge {
    // ── Assembly management ──────────────────────────────────────
    void (*LoadAppAssembly)(const char* path)                              = nullptr;
    void (*UnloadAppAssembly)()                                            = nullptr;

    // ── Class discovery ──────────────────────────────────────────
    int  (*GetEntityClassCount)()                                          = nullptr;
    void (*GetEntityClassName)(int index, char* buffer, int buffer_size)   = nullptr;
    bool (*EntityClassExists)(const char* full_name)                       = nullptr;
    /// Returns a stable hash for the class name (used as script_type_id).
    uint32_t (*GetScriptTypeId)(const char* full_name)                     = nullptr;

    // ── Instance lifecycle ───────────────────────────────────────
    /// Create an instance of the named class, bound to entity_id.
    /// Returns a managed handle (GCHandle), or 0 on failure.
    uint64_t (*CreateInstance)(const char* class_name, uint64_t entity_id) = nullptr;

    /// Invoke OnCreate on a single entity instance.
    void (*InvokeOnCreate)(uint64_t entity_id)                             = nullptr;

    /// Invoke OnUpdate on a batch of entities sharing the same script type.
    /// entity_ids and managed_handles are parallel arrays of size count.
    void (*InvokeOnUpdateBatch)(uint32_t script_type_id,
                                const uint64_t* entity_ids,
                                const uint64_t* managed_handles,
                                int count,
                                float delta_time)                          = nullptr;

    /// Invoke OnDestroy on a single entity instance, then release its GCHandle.
    void (*InvokeOnDestroy)(uint64_t entity_id, uint64_t managed_handle)   = nullptr;

    /// Destroy all managed instances (bulk cleanup).
    void (*DestroyAllInstances)()                                          = nullptr;

    // ── Method invocation (generic) ──────────────────────────────
    /// Invoke a named method on a specific entity's script instance.
    /// Used for collision callbacks, custom events, etc.
    void (*InvokeMethod)(uint64_t entity_id,
                         const char* method_name,
                         const void* args,
                         int args_size_bytes)                              = nullptr;
};

} // namespace helios
```

---

## Task 4: `NativeEngineAPI` struct (C# -> C++ calls)

**Files:**
- Modify: `helios-script/include/helios/script/native_engine_api.h`

Function pointers passed TO C# so managed code can call back into C++. Redesigned from existing `Engine/src/Script/NativeEngineAPI.h` to be World-context-aware (the `void* world_context` is stored on the C# side and passed back on every call, removing the need for any global state).

- [ ] **Step 1: Define the `NativeEngineAPI` struct**

```cpp
// helios-script/include/helios/script/native_engine_api.h
#pragma once

#include <cstdint>

namespace helios {

/// Function pointers passed TO C# at initialization.
/// C# stores these and calls back via unmanaged function pointer delegates.
/// All parameters are blittable (no managed types, no C++ objects).
///
/// IMPORTANT: Every callback receives `void* world_ctx` as its first argument.
/// This is the World* pointer that was passed to ScriptGlue::fill().
/// This eliminates all global/static state -- the glue functions
/// cast world_ctx back to World* and operate on it directly.
struct NativeEngineAPI {
    // ── World context ─────────────────────────────────────────────
    /// Opaque pointer to the World. Stored by C# and passed back
    /// on every native call. Set by ScriptGlue::fill().
    void* world_context = nullptr;

    // ── Logging ───────────────────────────────────────────────────
    void (*Log)(void* world_ctx, int level, const char* message)                          = nullptr;

    // ── Entity operations ─────────────────────────────────────────
    /// Spawn a new entity. Returns its raw ID (generation + index packed).
    uint64_t (*Spawn)(void* world_ctx)                                                    = nullptr;
    /// Despawn an entity by ID (deferred via Commands).
    void (*Despawn)(void* world_ctx, uint64_t entity_id)                                  = nullptr;
    /// Returns true if the entity is still alive.
    bool (*IsAlive)(void* world_ctx, uint64_t entity_id)                                  = nullptr;

    // ── Component access (generic, string-based) ──────────────────
    /// Returns true if entity has the named component.
    bool (*HasComponent)(void* world_ctx, uint64_t entity_id, const char* component_name) = nullptr;

    /// Get component data. Writes raw bytes into out_data (up to out_size).
    /// Returns actual size written, or 0 if entity lacks the component.
    int (*GetComponent)(void* world_ctx, uint64_t entity_id,
                        const char* component_name,
                        void* out_data, int out_size)                                     = nullptr;

    /// Set component data from raw bytes.
    void (*SetComponent)(void* world_ctx, uint64_t entity_id,
                         const char* component_name,
                         const void* in_data, int in_size)                                = nullptr;

    // ── Transform (convenience shortcuts) ─────────────────────────
    void (*TransformGetTranslation)(void* world_ctx, uint64_t entity_id, float* out_xyz)  = nullptr;
    void (*TransformSetTranslation)(void* world_ctx, uint64_t entity_id, float* in_xyz)   = nullptr;
    void (*TransformGetRotation)(void* world_ctx, uint64_t entity_id, float* out_xyz)     = nullptr;
    void (*TransformSetRotation)(void* world_ctx, uint64_t entity_id, float* in_xyz)      = nullptr;
    void (*TransformGetScale)(void* world_ctx, uint64_t entity_id, float* out_xyz)        = nullptr;
    void (*TransformSetScale)(void* world_ctx, uint64_t entity_id, float* in_xyz)         = nullptr;

    // ── Input ─────────────────────────────────────────────────────
    bool (*IsKeyPressed)(void* world_ctx, int keycode)                                    = nullptr;
    bool (*IsMouseButtonPressed)(void* world_ctx, int button)                             = nullptr;
    void (*GetMousePosition)(void* world_ctx, float* out_x, float* out_y)                = nullptr;

    // ── Physics (rigid body operations) ───────────────────────────
    void (*PhysicsGetLinearVelocity)(void* world_ctx, uint64_t entity_id, float* out_xyz) = nullptr;
    void (*PhysicsSetLinearVelocity)(void* world_ctx, uint64_t entity_id, float* in_xyz)  = nullptr;
    void (*PhysicsAddImpulse)(void* world_ctx, uint64_t entity_id, float* in_xyz)         = nullptr;
    void (*PhysicsAddForce)(void* world_ctx, uint64_t entity_id, float* in_xyz)           = nullptr;
    bool (*PhysicsIsGrounded)(void* world_ctx, uint64_t entity_id)                        = nullptr;

    // ── Audio ─────────────────────────────────────────────────────
    void (*AudioPlayGlobal)(void* world_ctx, uint64_t entity_id, int clip_index)          = nullptr;
    void (*AudioPlaySpatial)(void* world_ctx, uint64_t entity_id, int clip_index)         = nullptr;
};

} // namespace helios
```

---

## Task 5: `ScriptGlue` -- populate NativeEngineAPI with World-based implementations

**Files:**
- Modify: `helios-script/include/helios/script/script_glue.h`
- Modify: `helios-script/src/script_glue.cpp`

This replaces the existing `Engine/src/Script/ScriptGlue.cpp` which used `ScriptEngine::GetSceneContext()` (global state). The new version receives `World*` and stores it in `NativeEngineAPI::world_context`. Every callback casts `void* world_ctx` back to `World&` and uses ECS queries/resources directly.

- [ ] **Step 1: Define the `ScriptGlue` header**

```cpp
// helios-script/include/helios/script/script_glue.h
#pragma once

#include <helios/script/native_engine_api.h>

namespace helios {

class World;

/// Fills a NativeEngineAPI struct with function pointers that
/// operate on the given World. The World pointer is stored in
/// api.world_context and passed back by C# on every call.
///
/// IMPORTANT: The World must outlive the NativeEngineAPI.
struct ScriptGlue {
    static void fill(NativeEngineAPI& api, World& world);
};

} // namespace helios
```

- [ ] **Step 2: Implement the glue functions**

```cpp
// helios-script/src/script_glue.cpp
#include <helios/script/script_glue.h>
#include <helios/core/world.h>
#include <helios/core/components.h>    // Transform, etc.
#include <helios/core/input.h>         // Input resource
#include <helios/core/log.h>           // HVE_CORE_* macros
// Physics and Audio headers from Plans 9 / 6
// #include <helios/physics/physics_world.h>
// #include <helios/audio/audio_device.h>

namespace helios {
namespace {

// ── Helper: recover World& from opaque pointer ──────────────────
inline World& world_from(void* ctx) {
    return *static_cast<World*>(ctx);
}

// ── Logging ─────────────────────────────────────────────────────

void GlueLog(void* world_ctx, int level, const char* message) {
    (void)world_ctx;
    switch (level) {
        case 0: HVE_CORE_TRACE_TAG("Script", "{}", message); break;
        case 1: HVE_CORE_INFO_TAG("Script",  "{}", message); break;
        case 2: HVE_CORE_WARN_TAG("Script",  "{}", message); break;
        case 3: HVE_CORE_ERROR_TAG("Script", "{}", message); break;
        default: HVE_CORE_INFO_TAG("Script", "{}", message); break;
    }
}

// ── Entity operations ───────────────────────────────────────────

uint64_t GlueSpawn(void* world_ctx) {
    auto& w = world_from(world_ctx);
    Entity e = w.spawn();
    return e.raw();   // Entity packs generation+index into uint64_t
}

void GlueDespawn(void* world_ctx, uint64_t entity_id) {
    auto& w = world_from(world_ctx);
    w.despawn(Entity::from_raw(entity_id));
}

bool GlueIsAlive(void* world_ctx, uint64_t entity_id) {
    auto& w = world_from(world_ctx);
    return w.is_alive(Entity::from_raw(entity_id));
}

// ── Component access (string-based dispatch) ────────────────────
// Uses a static registry of component name -> typed accessor.
// Registration happens in fill() so it can capture the World.

// Component check: delegates to a name->lambda map
using HasComponentFn = bool(*)(World&, Entity);
static std::unordered_map<std::string, HasComponentFn> s_has_component_fns;

bool GlueHasComponent(void* world_ctx, uint64_t entity_id, const char* name) {
    auto& w = world_from(world_ctx);
    auto it = s_has_component_fns.find(name);
    if (it == s_has_component_fns.end()) return false;
    return it->second(w, Entity::from_raw(entity_id));
}

// GetComponent / SetComponent -- generic byte-copy through reflection
// For now, provide typed shortcuts (Transform, etc.) and stub the generic path.
int GlueGetComponent(void* world_ctx, uint64_t entity_id,
                     const char* component_name,
                     void* out_data, int out_size) {
    // TODO: implement via reflection (qlibs/reflect) in a future task
    (void)world_ctx; (void)entity_id; (void)component_name;
    (void)out_data; (void)out_size;
    return 0;
}

void GlueSetComponent(void* world_ctx, uint64_t entity_id,
                      const char* component_name,
                      const void* in_data, int in_size) {
    // TODO: implement via reflection (qlibs/reflect) in a future task
    (void)world_ctx; (void)entity_id; (void)component_name;
    (void)in_data; (void)in_size;
}

// ── Transform shortcuts ─────────────────────────────────────────

void GlueTransformGetTranslation(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = Entity::from_raw(id);
    auto& t = w.get<Transform>(entity);
    out[0] = t.position.x;
    out[1] = t.position.y;
    out[2] = t.position.z;
}

void GlueTransformSetTranslation(void* world_ctx, uint64_t id, float* in) {
    auto& w = world_from(world_ctx);
    auto entity = Entity::from_raw(id);
    auto& t = w.get<Transform>(entity);
    t.position = {in[0], in[1], in[2]};
}

void GlueTransformGetRotation(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = Entity::from_raw(id);
    auto& t = w.get<Transform>(entity);
    // rotation stored as quat, expose as euler for C# convenience
    auto euler = glm::eulerAngles(t.rotation);
    out[0] = euler.x; out[1] = euler.y; out[2] = euler.z;
}

void GlueTransformSetRotation(void* world_ctx, uint64_t id, float* in) {
    auto& w = world_from(world_ctx);
    auto entity = Entity::from_raw(id);
    auto& t = w.get<Transform>(entity);
    t.rotation = glm::quat(glm::vec3(in[0], in[1], in[2]));
}

void GlueTransformGetScale(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = Entity::from_raw(id);
    auto& t = w.get<Transform>(entity);
    out[0] = t.scale.x; out[1] = t.scale.y; out[2] = t.scale.z;
}

void GlueTransformSetScale(void* world_ctx, uint64_t id, float* in) {
    auto& w = world_from(world_ctx);
    auto entity = Entity::from_raw(id);
    auto& t = w.get<Transform>(entity);
    t.scale = {in[0], in[1], in[2]};
}

// ── Input ───────────────────────────────────────────────────────

bool GlueIsKeyPressed(void* world_ctx, int keycode) {
    auto& w = world_from(world_ctx);
    auto& input = w.resource<Input>();
    return input.is_key_pressed(keycode);
}

bool GlueIsMouseButtonPressed(void* world_ctx, int button) {
    auto& w = world_from(world_ctx);
    auto& input = w.resource<Input>();
    return input.is_mouse_button_pressed(button);
}

void GlueGetMousePosition(void* world_ctx, float* out_x, float* out_y) {
    auto& w = world_from(world_ctx);
    auto& input = w.resource<Input>();
    auto pos = input.mouse_position();
    *out_x = pos.x;
    *out_y = pos.y;
}

// ── Physics stubs ───────────────────────────────────────────────
// Implementations will call into PhysicsWorld resource from Plan 9.

void GluePhysicsGetLinearVelocity(void* world_ctx, uint64_t id, float* out) {
    // auto& w = world_from(world_ctx);
    // auto& physics = w.resource<PhysicsWorld>();
    // auto vel = physics.get_linear_velocity(BodyHandle{id});
    // out[0] = vel.x; out[1] = vel.y; out[2] = vel.z;
    (void)world_ctx; (void)id; (void)out;
}

void GluePhysicsSetLinearVelocity(void* world_ctx, uint64_t id, float* in) {
    (void)world_ctx; (void)id; (void)in;
}

void GluePhysicsAddImpulse(void* world_ctx, uint64_t id, float* in) {
    (void)world_ctx; (void)id; (void)in;
}

void GluePhysicsAddForce(void* world_ctx, uint64_t id, float* in) {
    (void)world_ctx; (void)id; (void)in;
}

bool GluePhysicsIsGrounded(void* world_ctx, uint64_t id) {
    (void)world_ctx; (void)id;
    return false;
}

// ── Audio stubs ─────────────────────────────────────────────────

void GlueAudioPlayGlobal(void* world_ctx, uint64_t id, int clip_index) {
    (void)world_ctx; (void)id; (void)clip_index;
}

void GlueAudioPlaySpatial(void* world_ctx, uint64_t id, int clip_index) {
    (void)world_ctx; (void)id; (void)clip_index;
}

// ── Component registration ──────────────────────────────────────

void register_component_checks() {
    s_has_component_fns["Transform"] = [](World& w, Entity e) -> bool {
        return w.has<Transform>(e);
    };
    s_has_component_fns["ScriptInstance"] = [](World& w, Entity e) -> bool {
        return w.has<ScriptInstance>(e);
    };
    // Add more as components are defined in Plan 1.
    // Each new component type gets a one-liner here.
}

} // anonymous namespace

// ── Public entry point ──────────────────────────────────────────

void ScriptGlue::fill(NativeEngineAPI& api, World& world) {
    register_component_checks();

    api.world_context = static_cast<void*>(&world);

    // Logging
    api.Log = GlueLog;

    // Entity
    api.Spawn     = GlueSpawn;
    api.Despawn   = GlueDespawn;
    api.IsAlive   = GlueIsAlive;

    // Component (generic)
    api.HasComponent  = GlueHasComponent;
    api.GetComponent  = GlueGetComponent;
    api.SetComponent  = GlueSetComponent;

    // Transform shortcuts
    api.TransformGetTranslation = GlueTransformGetTranslation;
    api.TransformSetTranslation = GlueTransformSetTranslation;
    api.TransformGetRotation    = GlueTransformGetRotation;
    api.TransformSetRotation    = GlueTransformSetRotation;
    api.TransformGetScale       = GlueTransformGetScale;
    api.TransformSetScale       = GlueTransformSetScale;

    // Input
    api.IsKeyPressed        = GlueIsKeyPressed;
    api.IsMouseButtonPressed = GlueIsMouseButtonPressed;
    api.GetMousePosition    = GlueGetMousePosition;

    // Physics
    api.PhysicsGetLinearVelocity = GluePhysicsGetLinearVelocity;
    api.PhysicsSetLinearVelocity = GluePhysicsSetLinearVelocity;
    api.PhysicsAddImpulse        = GluePhysicsAddImpulse;
    api.PhysicsAddForce          = GluePhysicsAddForce;
    api.PhysicsIsGrounded        = GluePhysicsIsGrounded;

    // Audio
    api.AudioPlayGlobal  = GlueAudioPlayGlobal;
    api.AudioPlaySpatial = GlueAudioPlaySpatial;
}

} // namespace helios
```

- [ ] **Step 3: Verify compilation**

```bash
cmake --build build --target helios-script -j$(nproc) 2>&1 | tail -5
```

---

## Task 6: `CoreCLRRuntime` -- HostFXR-based `ScriptRuntime` implementation

**Files:**
- Create: `helios-script/src/coreclr_runtime.h`
- Modify: `helios-script/src/coreclr_runtime.cpp`

RAII implementation of `ScriptRuntime`. Constructor loads the hostfxr shared library, initializes the CoreCLR runtime, resolves the C# `ScriptHostBridge.Initialize` method, and calls it to exchange function pointers. Destructor shuts everything down. Adapted from `Engine/src/Script/HostFXR.cpp` and `ScriptEngine.cpp`, but as an instance (no statics).

- [ ] **Step 1: Define the `CoreCLRRuntime` class**

```cpp
// helios-script/src/coreclr_runtime.h
#pragma once

#include <helios/script/script_runtime.h>
#include <helios/script/managed_bridge.h>
#include <helios/script/native_engine_api.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

// Forward-declare HostFXR types (from <hostfxr.h>)
using hostfxr_handle = void*;
using load_assembly_and_get_function_pointer_fn =
    int(*)(const char* assembly_path,
           const char* type_name,
           const char* method_name,
           const char* delegate_type_name,
           void* reserved,
           void** delegate);

namespace helios {

class World;

/// Production ScriptRuntime backed by .NET CoreCLR via HostFXR.
///
/// RAII lifetime:
///   Constructor: loads hostfxr, boots CoreCLR, resolves ScriptHostBridge.Initialize,
///                calls Initialize to exchange function pointer structs.
///   Destructor:  destroys all instances, unloads assembly, shuts down CoreCLR,
///                frees the hostfxr dynamic library.
///
/// Throws std::runtime_error from constructor if any init step fails.
class CoreCLRRuntime final : public ScriptRuntime {
public:
    /// Construct and fully initialize the CoreCLR runtime.
    ///
    /// @param runtime_config_path  Path to the .runtimeconfig.json for ScriptCore.
    /// @param script_core_path     Path to ScriptCore.dll (the engine-side C# assembly).
    /// @param world                The World whose NativeEngineAPI will be populated.
    CoreCLRRuntime(const std::filesystem::path& runtime_config_path,
                   const std::filesystem::path& script_core_path,
                   World& world);

    ~CoreCLRRuntime() override;

    // Non-copyable, non-movable (owns native handles)
    CoreCLRRuntime(const CoreCLRRuntime&) = delete;
    CoreCLRRuntime& operator=(const CoreCLRRuntime&) = delete;
    CoreCLRRuntime(CoreCLRRuntime&&) = delete;
    CoreCLRRuntime& operator=(CoreCLRRuntime&&) = delete;

    // ── ScriptRuntime interface ─────────────────────────────────

    bool load_assembly(const std::filesystem::path& assembly_path) override;
    void unload_assembly() override;
    bool reload_assembly(const std::filesystem::path& assembly_path) override;

    bool class_exists(const std::string& full_class_name) const override;
    std::vector<std::string> get_script_class_names() const override;
    uint32_t get_script_type_id(const std::string& full_class_name) const override;

    uint64_t invoke_create(const std::string& class_name, uint64_t entity_id) override;
    void invoke_update(uint32_t script_type_id,
                       const uint64_t* entity_ids,
                       const uint64_t* managed_handles,
                       size_t count,
                       float delta) override;
    void invoke_destroy(uint64_t entity_id, uint64_t managed_handle) override;
    void destroy_all_instances() override;

    void request_reload() override;
    bool reload_requested() const override;
    void clear_reload_request() override;

private:
    // ── HostFXR handles ─────────────────────────────────────────
    void* m_hostfxr_handle = nullptr;           // dlopen handle to libhostfxr.so
    hostfxr_handle m_host_context = nullptr;     // runtime context handle

    // ── HostFXR function pointers ───────────────────────────────
    void* (*m_close_fn)(hostfxr_handle) = nullptr;
    load_assembly_and_get_function_pointer_fn m_load_assembly_fn = nullptr;

    // ── Bridge structs ──────────────────────────────────────────
    ManagedBridge m_bridge{};
    NativeEngineAPI m_native_api{};

    // ── State ───────────────────────────────────────────────────
    std::filesystem::path m_app_assembly_path;
    std::atomic<bool> m_reload_requested{false};

    // ── Internal helpers ────────────────────────────────────────
    bool load_hostfxr();
    void* get_managed_fn(const std::filesystem::path& assembly_path,
                         const char* type_name,
                         const char* method_name);
};

} // namespace helios
```

- [ ] **Step 2: Implement `CoreCLRRuntime`**

Adapt from `Engine/src/Script/HostFXR.cpp` and `ScriptEngine.cpp`. Key differences from existing code:
- All state is per-instance (no statics).
- Constructor does all init; throws on failure.
- Destructor does all cleanup.
- `load_hostfxr()` uses the same platform helpers (dlopen/LoadLibrary) but stores handles in `m_hostfxr_handle`.
- `get_managed_fn()` wraps the `load_assembly_and_get_function_pointer` call.
- Constructor calls `get_managed_fn()` to resolve `ScriptHostBridge.Initialize`, then calls it passing `&m_native_api` and `&m_bridge`.
- `invoke_update` delegates to `m_bridge.InvokeOnUpdateBatch`.
- `invoke_create` calls `m_bridge.CreateInstance` then `m_bridge.InvokeOnCreate`.
- `invoke_destroy` calls `m_bridge.InvokeOnDestroy`.
- `get_script_type_id` calls `m_bridge.GetScriptTypeId` (new bridge function).

```cpp
// helios-script/src/coreclr_runtime.cpp
#include "coreclr_runtime.h"
#include <helios/script/script_glue.h>
#include <helios/core/log.h>

#include <stdexcept>
#include <string>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

// Forward-declare hostfxr types we need
using hostfxr_initialize_for_runtime_config_fn =
    int32_t(*)(const char* runtime_config_path, void* parameters, hostfxr_handle* host_context_handle);
using hostfxr_get_runtime_delegate_fn =
    int32_t(*)(hostfxr_handle host_context_handle, int type, void** delegate);
using hostfxr_close_fn = int32_t(*)(hostfxr_handle host_context_handle);

// hdt_load_assembly_and_get_function_pointer = 5
static constexpr int HDT_LOAD_ASSEMBLY = 5;
// UNMANAGEDCALLERSONLY_METHOD sentinel
static constexpr const char* UNMANAGEDCALLERSONLY = "System.Runtime.InteropServices.UnmanagedCallersOnlyAttribute, System.Runtime.InteropServices";

namespace helios {

// ── Platform helpers ────────────────────────────────────────────

namespace {

void* load_dynamic_library(const std::filesystem::path& path) {
#ifdef _WIN32
    return static_cast<void*>(LoadLibraryW(path.wstring().c_str()));
#else
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h)
        HVE_CORE_ERROR_TAG("CoreCLR", "dlopen failed: {}", dlerror());
    return h;
#endif
}

void* get_export(void* lib, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
#else
    void* sym = dlsym(lib, name);
    if (!sym)
        HVE_CORE_ERROR_TAG("CoreCLR", "dlsym failed for '{}': {}", name, dlerror());
    return sym;
#endif
}

void free_dynamic_library(void* lib) {
    if (!lib) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(lib));
#else
    dlclose(lib);
#endif
}

} // anonymous namespace

// ── Constructor ─────────────────────────────────────────────────

CoreCLRRuntime::CoreCLRRuntime(
    const std::filesystem::path& runtime_config_path,
    const std::filesystem::path& script_core_path,
    World& world)
{
    // 1. Fill native API with World-based implementations
    ScriptGlue::fill(m_native_api, world);

    // 2. Load hostfxr shared library
    if (!load_hostfxr()) {
        throw std::runtime_error("CoreCLRRuntime: failed to load hostfxr");
    }

    // 3. Initialize runtime from config
    auto init_fn = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
        get_export(m_hostfxr_handle, "hostfxr_initialize_for_runtime_config"));
    auto get_delegate_fn = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
        get_export(m_hostfxr_handle, "hostfxr_get_runtime_delegate"));
    m_close_fn = reinterpret_cast<decltype(m_close_fn)>(
        get_export(m_hostfxr_handle, "hostfxr_close"));

    if (!init_fn || !get_delegate_fn || !m_close_fn) {
        free_dynamic_library(m_hostfxr_handle);
        throw std::runtime_error("CoreCLRRuntime: failed to resolve hostfxr exports");
    }

    std::string config_str = runtime_config_path.string();
    int32_t rc = init_fn(config_str.c_str(), nullptr, &m_host_context);
    if (rc != 0 || !m_host_context) {
        free_dynamic_library(m_hostfxr_handle);
        throw std::runtime_error("CoreCLRRuntime: hostfxr_initialize_for_runtime_config failed (rc=0x"
                                 + std::to_string(static_cast<uint32_t>(rc)) + ")");
    }

    // 4. Get load_assembly_and_get_function_pointer delegate
    void* delegate = nullptr;
    rc = get_delegate_fn(m_host_context, HDT_LOAD_ASSEMBLY, &delegate);
    if (rc != 0 || !delegate) {
        reinterpret_cast<hostfxr_close_fn>(m_close_fn)(m_host_context);
        free_dynamic_library(m_hostfxr_handle);
        throw std::runtime_error("CoreCLRRuntime: failed to get load_assembly delegate");
    }
    m_load_assembly_fn = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(delegate);

    // 5. Resolve ScriptHostBridge.Initialize from ScriptCore.dll
    using InitializeFn = void(*)(NativeEngineAPI*, ManagedBridge*);
    auto init_bridge = reinterpret_cast<InitializeFn>(
        get_managed_fn(script_core_path,
                       "Helios.Bridge.ScriptHostBridge, ScriptCore",
                       "Initialize"));
    if (!init_bridge) {
        reinterpret_cast<hostfxr_close_fn>(m_close_fn)(m_host_context);
        free_dynamic_library(m_hostfxr_handle);
        throw std::runtime_error("CoreCLRRuntime: failed to resolve ScriptHostBridge.Initialize");
    }

    // 6. Call Initialize -- passes our native API, receives bridge function pointers
    init_bridge(&m_native_api, &m_bridge);

    HVE_CORE_TRACE_TAG("CoreCLR", "C# scripting runtime initialized");
}

// ── Destructor ──────────────────────────────────────────────────

CoreCLRRuntime::~CoreCLRRuntime() {
    // Destroy all managed instances
    if (m_bridge.DestroyAllInstances)
        m_bridge.DestroyAllInstances();

    // Unload app assembly
    if (m_bridge.UnloadAppAssembly)
        m_bridge.UnloadAppAssembly();

    // Close hostfxr context
    if (m_close_fn && m_host_context) {
        reinterpret_cast<hostfxr_close_fn>(m_close_fn)(m_host_context);
        m_host_context = nullptr;
    }

    // Free dynamic library
    free_dynamic_library(m_hostfxr_handle);
    m_hostfxr_handle = nullptr;

    HVE_CORE_TRACE_TAG("CoreCLR", "C# scripting runtime shut down");
}

// ── load_hostfxr ────────────────────────────────────────────────

bool CoreCLRRuntime::load_hostfxr() {
    // Locate hostfxr under the vendored .NET SDK
    // Adapt path resolution from existing HostFXR.cpp
    std::filesystem::path fxr_base =
        std::filesystem::path(HELIOS_DOTNET_ROOT) / "host" / "fxr";

    if (!std::filesystem::exists(fxr_base)) {
        HVE_CORE_ERROR_TAG("CoreCLR", "hostfxr base not found: {}", fxr_base.string());
        return false;
    }

    // Find first version subdirectory
    std::filesystem::path fxr_dir;
    for (auto& entry : std::filesystem::directory_iterator(fxr_base)) {
        if (entry.is_directory()) { fxr_dir = entry.path(); break; }
    }
    if (fxr_dir.empty()) {
        HVE_CORE_ERROR_TAG("CoreCLR", "No version subdir under {}", fxr_base.string());
        return false;
    }

#ifdef _WIN32
    auto fxr_path = fxr_dir / "hostfxr.dll";
#else
    auto fxr_path = fxr_dir / "libhostfxr.so";
#endif

    m_hostfxr_handle = load_dynamic_library(fxr_path);
    if (!m_hostfxr_handle) {
        HVE_CORE_ERROR_TAG("CoreCLR", "Failed to load hostfxr from {}", fxr_path.string());
        return false;
    }

    HVE_CORE_TRACE_TAG("CoreCLR", "Loaded hostfxr from {}", fxr_path.string());
    return true;
}

// ── get_managed_fn ──────────────────────────────────────────────

void* CoreCLRRuntime::get_managed_fn(
    const std::filesystem::path& assembly_path,
    const char* type_name,
    const char* method_name)
{
    if (!m_load_assembly_fn) return nullptr;

    void* fn = nullptr;
    std::string asm_str = assembly_path.string();

    // UNMANAGEDCALLERSONLY_METHOD is defined as a special sentinel in hostfxr
    int rc = m_load_assembly_fn(
        asm_str.c_str(), type_name, method_name,
        UNMANAGEDCALLERSONLY,
        nullptr, &fn);

    if (rc != 0 || !fn) {
        HVE_CORE_ERROR_TAG("CoreCLR",
            "Failed to resolve {}::{} (rc=0x{:X})",
            type_name, method_name, static_cast<uint32_t>(rc));
        return nullptr;
    }
    return fn;
}

// ── Assembly management ─────────────────────────────────────────

bool CoreCLRRuntime::load_assembly(const std::filesystem::path& assembly_path) {
    if (!m_bridge.LoadAppAssembly) return false;
    m_app_assembly_path = assembly_path;
    std::string path_str = assembly_path.string();
    m_bridge.LoadAppAssembly(path_str.c_str());
    HVE_CORE_TRACE_TAG("CoreCLR", "Loaded app assembly: {}", path_str);
    return true;
}

void CoreCLRRuntime::unload_assembly() {
    if (m_bridge.DestroyAllInstances) m_bridge.DestroyAllInstances();
    if (m_bridge.UnloadAppAssembly)   m_bridge.UnloadAppAssembly();
}

bool CoreCLRRuntime::reload_assembly(const std::filesystem::path& assembly_path) {
    unload_assembly();
    return load_assembly(assembly_path);
}

// ── Class discovery ─────────────────────────────────────────────

bool CoreCLRRuntime::class_exists(const std::string& full_class_name) const {
    if (!m_bridge.EntityClassExists) return false;
    return m_bridge.EntityClassExists(full_class_name.c_str());
}

std::vector<std::string> CoreCLRRuntime::get_script_class_names() const {
    std::vector<std::string> names;
    if (!m_bridge.GetEntityClassCount || !m_bridge.GetEntityClassName)
        return names;

    int count = m_bridge.GetEntityClassCount();
    names.reserve(count);

    char buffer[256];
    for (int i = 0; i < count; i++) {
        m_bridge.GetEntityClassName(i, buffer, sizeof(buffer));
        names.emplace_back(buffer);
    }
    return names;
}

uint32_t CoreCLRRuntime::get_script_type_id(const std::string& full_class_name) const {
    if (!m_bridge.GetScriptTypeId) {
        // Fallback: hash the class name
        return static_cast<uint32_t>(std::hash<std::string>{}(full_class_name));
    }
    return m_bridge.GetScriptTypeId(full_class_name.c_str());
}

// ── Instance lifecycle ──────────────────────────────────────────

uint64_t CoreCLRRuntime::invoke_create(const std::string& class_name, uint64_t entity_id) {
    if (!m_bridge.CreateInstance) return 0;

    uint64_t handle = m_bridge.CreateInstance(class_name.c_str(), entity_id);
    if (handle != 0 && m_bridge.InvokeOnCreate) {
        m_bridge.InvokeOnCreate(entity_id);
    }
    return handle;
}

void CoreCLRRuntime::invoke_update(
    uint32_t script_type_id,
    const uint64_t* entity_ids,
    const uint64_t* managed_handles,
    size_t count,
    float delta)
{
    if (!m_bridge.InvokeOnUpdateBatch) return;
    m_bridge.InvokeOnUpdateBatch(
        script_type_id, entity_ids, managed_handles,
        static_cast<int>(count), delta);
}

void CoreCLRRuntime::invoke_destroy(uint64_t entity_id, uint64_t managed_handle) {
    if (m_bridge.InvokeOnDestroy) {
        m_bridge.InvokeOnDestroy(entity_id, managed_handle);
    }
}

void CoreCLRRuntime::destroy_all_instances() {
    if (m_bridge.DestroyAllInstances) m_bridge.DestroyAllInstances();
}

// ── Hot reload ──────────────────────────────────────────────────

void CoreCLRRuntime::request_reload() {
    m_reload_requested.store(true, std::memory_order_release);
}

bool CoreCLRRuntime::reload_requested() const {
    return m_reload_requested.load(std::memory_order_acquire);
}

void CoreCLRRuntime::clear_reload_request() {
    m_reload_requested.store(false, std::memory_order_release);
}

} // namespace helios
```

- [ ] **Step 3: Add `HELIOS_DOTNET_ROOT` CMake definition**

In `helios-script/CMakeLists.txt`, add:

```cmake
# Define the path to the vendored .NET SDK for hostfxr resolution at runtime.
# This gets baked into the binary as a compile definition.
target_compile_definitions(helios-script PRIVATE
    HELIOS_DOTNET_ROOT="${CMAKE_SOURCE_DIR}/Engine/vendor/dotnet"
)
```

- [ ] **Step 4: Verify compilation**

```bash
cmake --build build --target helios-script -j$(nproc) 2>&1 | tail -10
```

---

## Task 7: `ScriptInstance` component

**Files:**
- Modify: `helios-script/include/helios/script/script_instance.h`

The `ScriptInstance` component is attached to entities that have a C# script. Plain data, no inheritance, no virtuals. Defined in the spec at line 279.

- [ ] **Step 1: Define `ScriptInstance`**

```cpp
// helios-script/include/helios/script/script_instance.h
#pragma once

#include <cstdint>
#include <string>

namespace helios {

/// Component for entities that have a C# script attached.
/// Plain data struct -- no methods, no inheritance.
///
/// script_class_name: Fully-qualified C# class name (e.g., "Game.EnemyAI").
///                    Used to look up the script_type_id and create instances.
///
/// script_type_id: Numeric ID for the C# class type. Assigned by
///                 ScriptRuntime::get_script_type_id() during instance creation.
///                 Entities with the same script_type_id are batched together
///                 for update calls.
///
/// managed_handle: Opaque handle to the managed object (GCHandle on C# side).
///                 0 means no instance has been created yet.
///                 Set by ScriptRuntime::invoke_create().
struct ScriptInstance {
    std::string script_class_name;
    uint32_t script_type_id = 0;
    uint64_t managed_handle = 0;
};

/// Marker component added by script_create_system after the managed instance
/// has been created. Prevents re-creation on subsequent frames.
struct ScriptInitialized {};

} // namespace helios
```

---

## Task 8: `ScriptExecutionSystem` -- batch update system

**Files:**
- Create: `helios-script/src/script_execution_system.h`
- Modify: `helios-script/src/script_execution_system.cpp`

Runs in `Schedule::Update`. Queries all entities with `ScriptInstance` + `ScriptInitialized`. Groups them by `script_type_id`. For each group, calls `runtime.invoke_update(type_id, entity_ids[], managed_handles[], count, delta)`. Different type groups are independent and could run in parallel (the scheduler handles this via access metadata).

- [ ] **Step 1: Define the system header**

```cpp
// helios-script/src/script_execution_system.h
#pragma once

namespace helios {
class World;

/// ECS system: executes OnUpdate for all scripted entities, grouped by type.
/// Registered in Schedule::Update by ScriptingPlugin.
void script_execution_system(World& world);

/// ECS system: creates managed instances for new ScriptInstance entities.
/// Runs in Schedule::Update, before script_execution_system.
void script_create_system(World& world);

/// ECS system: destroys managed instances for despawned entities.
/// Runs in Schedule::PostUpdate.
void script_destroy_system(World& world);

} // namespace helios
```

- [ ] **Step 2: Implement `script_create_system`**

```cpp
// helios-script/src/script_execution_system.cpp
#include "script_execution_system.h"

#include <helios/core/world.h>
#include <helios/core/time.h>
#include <helios/script/script_runtime.h>
#include <helios/script/script_instance.h>

#include <unordered_map>
#include <vector>

namespace helios {

void script_create_system(World& world) {
    auto* runtime = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime || !*runtime) return;

    // Query entities that have ScriptInstance but NOT ScriptInitialized
    // (i.e., newly added script components that need instance creation)
    auto query = world.query<ScriptInstance, Without<ScriptInitialized>>();

    for (auto [entity, script] : query) {
        if (script.script_class_name.empty()) continue;
        if (!(*runtime)->class_exists(script.script_class_name)) {
            HVE_CORE_WARN_TAG("Script",
                "Script class '{}' not found in loaded assembly",
                script.script_class_name);
            continue;
        }

        // Assign type ID
        script.script_type_id =
            (*runtime)->get_script_type_id(script.script_class_name);

        // Create managed instance
        uint64_t handle =
            (*runtime)->invoke_create(script.script_class_name, entity.raw());

        if (handle == 0) {
            HVE_CORE_ERROR_TAG("Script",
                "Failed to create instance of '{}'", script.script_class_name);
            continue;
        }

        script.managed_handle = handle;

        // Mark as initialized so we don't re-create next frame
        world.add(entity, ScriptInitialized{});
    }
}
```

- [ ] **Step 3: Implement `script_execution_system`**

```cpp
void script_execution_system(World& world) {
    auto* runtime = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime || !*runtime) return;

    auto& time = world.resource<Time>();
    float delta = time.delta();

    // Query all initialized script entities
    auto query = world.query<ScriptInstance, ScriptInitialized>();

    // Group by script_type_id
    struct BatchEntry {
        uint64_t entity_id;
        uint64_t managed_handle;
    };

    std::unordered_map<uint32_t, std::vector<BatchEntry>> batches;

    for (auto [entity, script] : query) {
        if (script.managed_handle == 0) continue;
        batches[script.script_type_id].push_back({entity.raw(), script.managed_handle});
    }

    // Execute each batch
    // NOTE: Different batches could run in parallel if the scheduler
    // dispatches them. For now we execute sequentially within this system.
    // True parallelism comes from the scheduler running multiple systems
    // that don't conflict.
    for (auto& [type_id, entries] : batches) {
        // Build parallel arrays for the bridge call
        std::vector<uint64_t> entity_ids;
        std::vector<uint64_t> handles;
        entity_ids.reserve(entries.size());
        handles.reserve(entries.size());

        for (auto& e : entries) {
            entity_ids.push_back(e.entity_id);
            handles.push_back(e.managed_handle);
        }

        (*runtime)->invoke_update(
            type_id,
            entity_ids.data(),
            handles.data(),
            entity_ids.size(),
            delta);
    }
}
```

- [ ] **Step 4: Implement `script_destroy_system`**

```cpp
void script_destroy_system(World& world) {
    auto* runtime = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime || !*runtime) return;

    // This system reads a "PendingDespawn" event or marker.
    // When an entity with ScriptInstance is about to be despawned,
    // we need to destroy its managed instance first.
    //
    // Approach: query entities with ScriptInstance + a Despawning marker
    // (set by Commands::despawn before actual removal).
    // OR: use an EventReader<DespawnEvent> that contains the entity + components.
    //
    // For now, we use a simple approach: check a DespawnQueue resource
    // that Commands populates. Each entry has the Entity and its components
    // snapshot before removal.

    // TODO: Integrate with the Commands deferred despawn mechanism from Plan 1.
    // For the initial implementation, ScriptingPlugin will register a
    // pre-despawn hook that calls invoke_destroy before the entity is removed.
    //
    // Placeholder: iterate ScriptInstance entities and check is_alive.
    // This is O(n) but correct. Optimize with events later.
}

} // namespace helios
```

- [ ] **Step 5: Verify compilation**

---

## Task 9: File watcher (hot reload trigger)

**Files:**
- Create: `helios-script/src/file_watcher.h`
- Modify: `helios-script/src/file_watcher.cpp`

A lightweight file watcher using `std::jthread` that monitors a directory for `.cs` and `.dll` changes. When a change is detected, it sets an `std::atomic<bool>` flag. The `check_script_reload` system reads this flag.

Replaces the existing `FileWatch.hpp` dependency with a simpler, self-contained implementation using `std::filesystem::last_write_time`.

- [ ] **Step 1: Define the `FileWatcher` class**

```cpp
// helios-script/src/file_watcher.h
#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <thread>
#include <vector>

namespace helios {

/// Watches a directory for file modifications.
/// Uses std::jthread (auto-joins on destruction) and polls
/// std::filesystem::last_write_time at a configurable interval.
///
/// RAII: starts watching in constructor, stops in destructor.
class FileWatcher {
public:
    using Callback = std::function<void(const std::filesystem::path& changed_file)>;

    /// Start watching the given directory for files matching the extensions.
    /// callback is invoked from the watcher thread when a change is detected.
    ///
    /// @param watch_dir     Directory to watch (recursively).
    /// @param extensions    File extensions to monitor (e.g., {".cs", ".dll"}).
    /// @param callback      Called with the path of each changed file.
    /// @param poll_interval How often to check for changes (default 500ms).
    FileWatcher(const std::filesystem::path& watch_dir,
                std::vector<std::string> extensions,
                Callback callback,
                std::chrono::milliseconds poll_interval = std::chrono::milliseconds(500));

    ~FileWatcher() = default;  // std::jthread auto-joins

    // Non-copyable, movable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) noexcept = default;
    FileWatcher& operator=(FileWatcher&&) noexcept = default;

private:
    void watch_loop(std::stop_token stop_token);

    std::filesystem::path m_watch_dir;
    std::vector<std::string> m_extensions;
    Callback m_callback;
    std::chrono::milliseconds m_poll_interval;
    std::jthread m_thread;

    // Tracks last_write_time for each watched file
    std::unordered_map<std::string, std::filesystem::file_time_type> m_file_times;
};

} // namespace helios
```

- [ ] **Step 2: Implement the watch loop**

```cpp
// helios-script/src/file_watcher.cpp
#include "file_watcher.h"

#include <algorithm>
#include <helios/core/log.h>

namespace helios {

FileWatcher::FileWatcher(
    const std::filesystem::path& watch_dir,
    std::vector<std::string> extensions,
    Callback callback,
    std::chrono::milliseconds poll_interval)
    : m_watch_dir(watch_dir)
    , m_extensions(std::move(extensions))
    , m_callback(std::move(callback))
    , m_poll_interval(poll_interval)
{
    // Build initial snapshot of file times
    if (std::filesystem::exists(m_watch_dir)) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(m_watch_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            bool match = std::any_of(m_extensions.begin(), m_extensions.end(),
                                     [&](const auto& e) { return e == ext; });
            if (match) {
                m_file_times[entry.path().string()] = entry.last_write_time();
            }
        }
    }

    // Start watcher thread
    m_thread = std::jthread([this](std::stop_token st) { watch_loop(st); });
}

void FileWatcher::watch_loop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(m_poll_interval);
        if (stop_token.stop_requested()) break;

        if (!std::filesystem::exists(m_watch_dir)) continue;

        for (auto& entry : std::filesystem::recursive_directory_iterator(m_watch_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            bool match = std::any_of(m_extensions.begin(), m_extensions.end(),
                                     [&](const auto& e) { return e == ext; });
            if (!match) continue;

            auto path_str = entry.path().string();
            auto current_time = entry.last_write_time();

            auto it = m_file_times.find(path_str);
            if (it == m_file_times.end()) {
                // New file
                m_file_times[path_str] = current_time;
                m_callback(entry.path());
            } else if (it->second != current_time) {
                // Modified file
                it->second = current_time;
                m_callback(entry.path());
            }
        }
    }
}

} // namespace helios
```

---

## Task 10: Hot reload system (`check_script_reload`)

**Files:**
- Create: `helios-script/src/hot_reload_system.h`
- Create: `helios-script/src/hot_reload_system.cpp`

The reload system runs in `Schedule::PreUpdate`. When `ScriptRuntime::reload_requested()` is true, it serializes script state, unloads, reloads, and deserializes.

- [ ] **Step 1: Define the system**

```cpp
// helios-script/src/hot_reload_system.h
#pragma once

namespace helios {
class World;

/// ECS system: checks if a script reload has been requested.
/// If so, serializes script instance state, reloads the assembly,
/// and re-creates instances with the saved state.
/// Runs in Schedule::PreUpdate.
void check_script_reload(World& world);

} // namespace helios
```

- [ ] **Step 2: Implement the reload flow**

```cpp
// helios-script/src/hot_reload_system.cpp
#include "hot_reload_system.h"

#include <helios/core/world.h>
#include <helios/script/script_runtime.h>
#include <helios/script/script_instance.h>
#include <helios/core/log.h>

#include <vector>

namespace helios {

void check_script_reload(World& world) {
    auto* runtime_ptr = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime_ptr || !*runtime_ptr) return;

    auto& runtime = **runtime_ptr;
    if (!runtime.reload_requested()) return;

    HVE_CORE_WARN_TAG("Script", "Hot reload triggered -- reloading scripts...");

    // ── Step 1: Snapshot all script instances ────────────────────
    // Save (entity, class_name) pairs so we can re-create them after reload.
    struct ScriptSnapshot {
        Entity entity;
        std::string class_name;
        // TODO: serialize per-instance public field values here
        //       once reflection is available (qlibs/reflect).
    };

    std::vector<ScriptSnapshot> snapshots;

    auto query = world.query<ScriptInstance, ScriptInitialized>();
    for (auto [entity, script] : query) {
        snapshots.push_back({entity, script.script_class_name});
    }

    // ── Step 2: Destroy all managed instances ────────────────────
    runtime.destroy_all_instances();

    // Remove ScriptInitialized markers and zero out managed handles
    for (auto& snap : snapshots) {
        if (world.has<ScriptInitialized>(snap.entity)) {
            world.remove<ScriptInitialized>(snap.entity);
        }
        auto& script = world.get<ScriptInstance>(snap.entity);
        script.managed_handle = 0;
        script.script_type_id = 0;
    }

    // ── Step 3: Reload the assembly ──────────────────────────────
    auto* assembly_path = world.try_resource<ScriptAssemblyPath>();
    if (!assembly_path) {
        HVE_CORE_ERROR_TAG("Script", "No ScriptAssemblyPath resource -- cannot reload");
        runtime.clear_reload_request();
        return;
    }

    if (!runtime.reload_assembly(assembly_path->path)) {
        HVE_CORE_ERROR_TAG("Script", "Assembly reload failed");
        runtime.clear_reload_request();
        return;
    }

    // ── Step 4: Re-create instances ──────────────────────────────
    // script_create_system will pick them up next frame because
    // we removed ScriptInitialized and zeroed managed_handle.
    // The system will call invoke_create for each one.

    runtime.clear_reload_request();
    HVE_CORE_WARN_TAG("Script", "Hot reload complete -- {} scripts will be re-created",
                       snapshots.size());
}

/// Simple resource to store the path to the game assembly.
/// Inserted by ScriptingPlugin.
struct ScriptAssemblyPath {
    std::filesystem::path path;
};

} // namespace helios
```

---

## Task 11: `ScriptingPlugin`

**Files:**
- Modify: `helios-script/include/helios/script/scripting_plugin.h`
- Modify: `helios-script/src/scripting_plugin.cpp`

Registers all script systems and inserts the `ScriptRuntime` resource.

- [ ] **Step 1: Define the plugin**

```cpp
// helios-script/include/helios/script/scripting_plugin.h
#pragma once

#include <filesystem>
#include <string>

namespace helios {

class App;

/// Configuration for the scripting plugin.
struct ScriptingPluginConfig {
    /// Path to ScriptCore.runtimeconfig.json
    std::filesystem::path runtime_config_path;

    /// Path to ScriptCore.dll (engine-side C# bridge assembly)
    std::filesystem::path script_core_dll_path;

    /// Path to the game's script assembly (.dll)
    /// If empty, no assembly is loaded at startup.
    std::filesystem::path app_assembly_path;

    /// Directory to watch for .cs/.dll changes (hot reload).
    /// If empty, hot reload is disabled.
    std::filesystem::path watch_directory;
};

/// Plugin that integrates C# scripting into the engine.
///
/// Inserts:
///   - ScriptRuntime resource (std::unique_ptr<ScriptRuntime>)
///   - ScriptAssemblyPath resource (if app_assembly_path is set)
///   - FileWatcher (owned internally, triggers reload via ScriptRuntime)
///
/// Registers systems:
///   - Schedule::PreUpdate:  check_script_reload
///   - Schedule::Update:     script_create_system (before execution)
///   - Schedule::Update:     script_execution_system
///   - Schedule::PostUpdate: script_destroy_system
struct ScriptingPlugin {
    ScriptingPluginConfig config;

    void build(App& app);
};

} // namespace helios
```

- [ ] **Step 2: Implement the plugin**

```cpp
// helios-script/src/scripting_plugin.cpp
#include <helios/script/scripting_plugin.h>

#include <helios/core/app.h>
#include <helios/core/world.h>
#include <helios/core/log.h>
#include <helios/script/script_runtime.h>
#include <helios/script/script_instance.h>

#include "coreclr_runtime.h"
#include "script_execution_system.h"
#include "hot_reload_system.h"
#include "file_watcher.h"

#include <memory>

namespace helios {

void ScriptingPlugin::build(App& app) {
    auto& world = app.world();

    // ── Create CoreCLR runtime ──────────────────────────────────
    try {
        auto runtime = std::make_unique<CoreCLRRuntime>(
            config.runtime_config_path,
            config.script_core_dll_path,
            world);

        // Load the game assembly if specified
        if (!config.app_assembly_path.empty()) {
            runtime->load_assembly(config.app_assembly_path);
            world.insert_resource(ScriptAssemblyPath{config.app_assembly_path});
        }

        // Insert as resource (stored as std::unique_ptr<ScriptRuntime>)
        world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(runtime));

    } catch (const std::exception& e) {
        HVE_CORE_ERROR_TAG("Scripting",
            "Failed to initialize C# scripting: {}. "
            "Scripting will be disabled.", e.what());
        // Insert a null runtime so systems can safely check
        world.insert_resource<std::unique_ptr<ScriptRuntime>>(nullptr);
    }

    // ── File watcher for hot reload ─────────────────────────────
    if (!config.watch_directory.empty()) {
        auto* runtime_ptr = world.try_resource<std::unique_ptr<ScriptRuntime>>();
        if (runtime_ptr && *runtime_ptr) {
            // Capture a raw pointer to the runtime for the callback.
            // Safe because: runtime outlives the watcher (both are in World),
            // and watcher's jthread stops before World destruction.
            ScriptRuntime* rt = runtime_ptr->get();

            auto watcher = std::make_unique<FileWatcher>(
                config.watch_directory,
                std::vector<std::string>{".cs", ".dll"},
                [rt](const std::filesystem::path& changed) {
                    HVE_CORE_TRACE_TAG("Script", "File changed: {}", changed.string());
                    rt->request_reload();
                });

            // Store watcher as a resource so it lives as long as World
            world.insert_resource(std::move(watcher));
        }
    }

    // ── Register systems ────────────────────────────────────────

    // Hot reload check runs early each frame
    app.add_system(Schedule::PreUpdate, check_script_reload);

    // Instance creation runs before execution
    app.add_system(Schedule::Update, script_create_system);

    // Script execution (batched by type)
    app.add_system(Schedule::Update,
        script_execution_system/*.after(script_create_system)*/);
    // NOTE: uncomment .after() once the scheduler supports it.
    // For now, registration order determines execution order within
    // the same schedule when there are no access conflicts.

    // Cleanup on despawn
    app.add_system(Schedule::PostUpdate, script_destroy_system);

    HVE_CORE_INFO_TAG("Scripting", "ScriptingPlugin registered");
}

} // namespace helios
```

---

## Task 12: C# side -- `Script` base class and updated bridge

**Files:**
- Modify: `ScriptCore/Source/Helios/Scene/Entity.cs` (rename to Script base class pattern)
- Create: `ScriptCore/Source/Helios/Script.cs`
- Modify: `ScriptCore/Source/Helios/Bridge/ScriptHostBridge.cs` (add batch update, OnDestroy, type ID)
- Modify: `ScriptCore/Source/Helios/Bridge/NativeEngineAPI.cs` (add world_context, new functions)
- Modify: `ScriptCore/Source/Helios/Bridge/NativeAPI.cs` (add world_context passthrough)
- Create: `ScriptCore/Source/Helios/Scene/World.cs` (query access from scripts)

This task updates the C# managed code to match the new C++ bridge design. The key changes are:
1. `Script` base class (replaces `Entity` as the user-facing base class for scripts)
2. Batch `OnUpdate` support (C++ sends arrays of entity IDs + handles)
3. `OnDestroy` lifecycle callback
4. `GetScriptTypeId` bridge function
5. World context passthrough on all native API calls
6. `World` query wrapper for C# scripts

- [ ] **Step 1: Create `Script` base class**

```csharp
// ScriptCore/Source/Helios/Script.cs
using System;
using System.Numerics;

namespace Helios;

/// <summary>
/// Base class for all user scripts. Attach to entities via ScriptInstance component.
/// Subclass this and override OnCreate/OnUpdate/OnDestroy.
/// Access the entity via Entity property; access the world via World property.
/// </summary>
public abstract class Script
{
    /// <summary>Raw entity ID (generation + index packed into uint64).</summary>
    public ulong EntityId { get; internal set; }

    /// <summary>Entity wrapper for convenient component access.</summary>
    public Entity Entity => new Entity(EntityId);

    /// <summary>Called once when the script instance is created.</summary>
    public virtual void OnCreate() { }

    /// <summary>Called every frame during Schedule.Update.</summary>
    public virtual void OnUpdate(float delta) { }

    /// <summary>Called when the entity is despawned or the script is removed.</summary>
    public virtual void OnDestroy() { }

    /// <summary>Called when this entity collides with another.</summary>
    public virtual void OnCollisionEnter(ulong otherEntityId) { }

    /// <summary>Called every frame while colliding with another entity.</summary>
    public virtual void OnCollisionStay(ulong otherEntityId) { }

    /// <summary>Called when this entity stops colliding with another.</summary>
    public virtual void OnCollisionExit(ulong otherEntityId) { }
}
```

- [ ] **Step 2: Update `ScriptHostBridge.cs` for batch update and new functions**

Add the following to `ManagedBridgeNative`:

```csharp
// Add to ManagedBridgeNative struct (after existing fields):
public delegate* unmanaged<byte*, uint> GetScriptTypeId;
public delegate* unmanaged<uint, ulong*, ulong*, int, float, void> InvokeOnUpdateBatch;
public delegate* unmanaged<ulong, ulong, void> InvokeOnDestroy;
public delegate* unmanaged<ulong, byte*, void*, int, void> InvokeMethod;
```

Add the corresponding bridge implementations:

```csharp
// In ScriptHostBridge.Initialize, add:
outBridge->GetScriptTypeId = &BridgeGetScriptTypeId;
outBridge->InvokeOnUpdateBatch = &BridgeInvokeOnUpdateBatch;
outBridge->InvokeOnDestroy = &BridgeInvokeOnDestroy;
outBridge->InvokeMethod = &BridgeInvokeMethod;

// Change s_EntityClasses to s_ScriptClasses (scan for Script subclasses, not Entity)
// In BridgeLoadAppAssembly, change:
//   if (type.IsSubclassOf(typeof(Entity)) && !type.IsAbstract)
// to:
//   if (type.IsSubclassOf(typeof(Script)) && !type.IsAbstract)

// Change s_Instances type from Dictionary<ulong, Entity> to Dictionary<ulong, Script>

// Add type ID generation:
[UnmanagedCallersOnly]
private static uint BridgeGetScriptTypeId(byte* fullNamePtr)
{
    string fullName = PtrToString(fullNamePtr);
    // Simple stable hash
    uint hash = 2166136261u;
    foreach (char c in fullName)
    {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

// Batch update -- iterates the arrays, calls OnUpdate on each:
[UnmanagedCallersOnly]
private static void BridgeInvokeOnUpdateBatch(
    uint scriptTypeId,
    ulong* entityIds,
    ulong* managedHandles,
    int count,
    float deltaTime)
{
    for (int i = 0; i < count; i++)
    {
        ulong entityId = entityIds[i];
        if (s_Instances.TryGetValue(entityId, out var instance))
        {
            instance.OnUpdate(deltaTime);
        }
    }
}

// OnDestroy lifecycle:
[UnmanagedCallersOnly]
private static void BridgeInvokeOnDestroy(ulong entityId, ulong managedHandle)
{
    if (s_Instances.TryGetValue(entityId, out var instance))
    {
        instance.OnDestroy();
        s_Instances.Remove(entityId);
    }
}

// Generic method invocation (for collision callbacks, etc.):
[UnmanagedCallersOnly]
private static void BridgeInvokeMethod(
    ulong entityId, byte* methodNamePtr,
    void* args, int argsSizeBytes)
{
    if (!s_Instances.TryGetValue(entityId, out var instance)) return;
    string methodName = PtrToString(methodNamePtr);

    // For collision callbacks, args is a uint64 (other entity ID)
    if (methodName == "OnCollisionEnter" && argsSizeBytes >= 8)
        instance.OnCollisionEnter(*(ulong*)args);
    else if (methodName == "OnCollisionStay" && argsSizeBytes >= 8)
        instance.OnCollisionStay(*(ulong*)args);
    else if (methodName == "OnCollisionExit" && argsSizeBytes >= 8)
        instance.OnCollisionExit(*(ulong*)args);
}
```

- [ ] **Step 3: Update `NativeEngineAPI.cs` for world_context**

Update all function pointer signatures to include `void*` as the first parameter (the world_context). Add new function pointers for Log, Spawn, Despawn, IsAlive, HasComponent, GetComponent, SetComponent, Physics, and Audio.

```csharp
// ScriptCore/Source/Helios/Bridge/NativeEngineAPI.cs
using System.Runtime.InteropServices;

namespace Helios.Bridge;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeEngineAPI
{
    // World context (opaque pointer passed back on every call)
    public void* WorldContext;

    // Logging
    public delegate* unmanaged<void*, int, byte*, void> Log;

    // Entity
    public delegate* unmanaged<void*, ulong> Spawn;
    public delegate* unmanaged<void*, ulong, void> Despawn;
    public delegate* unmanaged<void*, ulong, bool> IsAlive;

    // Component (generic)
    public delegate* unmanaged<void*, ulong, byte*, bool> HasComponent;
    public delegate* unmanaged<void*, ulong, byte*, void*, int, int> GetComponent;
    public delegate* unmanaged<void*, ulong, byte*, void*, int, void> SetComponent;

    // Transform shortcuts
    public delegate* unmanaged<void*, ulong, float*, void> TransformGetTranslation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformSetTranslation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformGetRotation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformSetRotation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformGetScale;
    public delegate* unmanaged<void*, ulong, float*, void> TransformSetScale;

    // Input
    public delegate* unmanaged<void*, int, bool> IsKeyPressed;
    public delegate* unmanaged<void*, int, bool> IsMouseButtonPressed;
    public delegate* unmanaged<void*, float*, float*, void> GetMousePosition;

    // Physics
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsGetLinearVelocity;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsSetLinearVelocity;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsAddImpulse;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsAddForce;
    public delegate* unmanaged<void*, ulong, bool> PhysicsIsGrounded;

    // Audio
    public delegate* unmanaged<void*, ulong, int, void> AudioPlayGlobal;
    public delegate* unmanaged<void*, ulong, int, void> AudioPlaySpatial;
}
```

- [ ] **Step 4: Update `NativeAPI.cs` to pass world_context**

Every method now passes `Api->WorldContext` as the first argument:

```csharp
// Example for Transform:
internal static Vector3 TransformGetTranslation(ulong id)
{
    Vector3 v;
    Api->TransformGetTranslation(Api->WorldContext, id, (float*)&v);
    return v;
}

// Example for Input:
internal static bool IsKeyPressed(int keycode)
    => Api->IsKeyPressed(Api->WorldContext, keycode);

// New methods:
internal static void Log(int level, string message)
{
    var bytes = System.Text.Encoding.UTF8.GetBytes(message + '\0');
    fixed (byte* ptr = bytes)
        Api->Log(Api->WorldContext, level, ptr);
}

internal static ulong Spawn()
    => Api->Spawn(Api->WorldContext);

internal static void Despawn(ulong entityId)
    => Api->Despawn(Api->WorldContext, entityId);
```

- [ ] **Step 5: Create `World.cs` query wrapper**

```csharp
// ScriptCore/Source/Helios/Scene/World.cs
using System;
using System.Numerics;

namespace Helios;

/// <summary>
/// Provides read-only world query access from C# scripts.
/// Currently a placeholder -- full query support requires
/// C# generics mapped to C++ template queries via the bridge.
/// For now, scripts use Entity.GetComponent<T>() for data access.
/// </summary>
public static class World
{
    /// <summary>Spawn a new entity.</summary>
    public static Entity Spawn() => new Entity(NativeAPI.Spawn());

    /// <summary>Despawn an entity.</summary>
    public static void Despawn(Entity entity) => NativeAPI.Despawn(entity.ID);

    /// <summary>Check if an entity is alive.</summary>
    public static bool IsAlive(Entity entity) => NativeAPI.IsAlive(entity.ID);

    /// <summary>Log a message from script code.</summary>
    public static void Log(string message) => NativeAPI.Log(1, message);
    public static void LogWarning(string message) => NativeAPI.Log(2, message);
    public static void LogError(string message) => NativeAPI.Log(3, message);

    // TODO: QuerySingle<T, Filter>() -- requires generic bridge support.
    // For the design-spec pattern:
    //   var player = World.QuerySingle<Transform, With<Player>>();
    // This needs the C++ side to expose a query-by-component-name API
    // through NativeEngineAPI. Planned for a future iteration.
}
```

- [ ] **Step 6: Verify C# compilation**

```bash
cd ScriptCore && dotnet build 2>&1 | tail -10
```

---

## Task 13: Integration test -- `ScriptingPlugin` in a minimal `App`

**Files:**
- Create: `helios-script/tests/integration_test_scripting.cpp`

This test creates a minimal App with ScriptingPlugin, spawns an entity with ScriptInstance, and verifies the full lifecycle. Requires a working .NET SDK and ScriptCore.dll. This is an integration test, not a unit test -- it boots CoreCLR.

- [ ] **Step 1: Create the integration test**

```cpp
// helios-script/tests/integration_test_scripting.cpp
//
// Integration test: boots CoreCLR, loads a test assembly, creates/updates/destroys
// a script instance through the full ECS pipeline.
//
// Requires:
//   - ScriptCore.dll built in Editor/Resources/Scripts/
//   - A test assembly (TestScripts.dll) with a simple Script subclass

#include <helios/core/app.h>
#include <helios/script/scripting_plugin.h>
#include <helios/script/script_instance.h>
#include <helios/core/world.h>
#include <helios/core/time.h>

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace helios;

int main() {
    // Paths -- adjust for your build environment
    std::filesystem::path root = HELIOS_PROJECT_ROOT;
    std::filesystem::path runtime_config = root / "Editor/Resources/Scripts/ScriptCore.runtimeconfig.json";
    std::filesystem::path script_core = root / "Editor/Resources/Scripts/ScriptCore.dll";
    std::filesystem::path test_assembly = root / "test_data/TestScripts.dll";

    if (!std::filesystem::exists(runtime_config)) {
        std::cerr << "SKIP: runtime config not found at " << runtime_config << std::endl;
        return 0;
    }

    App app;

    app.add_plugin(ScriptingPlugin{{
        .runtime_config_path = runtime_config,
        .script_core_dll_path = script_core,
        .app_assembly_path = test_assembly,
        .watch_directory = {},  // no hot reload in test
    }});

    auto& world = app.world();

    // Spawn entity with a test script
    Entity e = world.spawn();
    world.add(e, ScriptInstance{
        .script_class_name = "TestScripts.SimpleScript",
        .script_type_id = 0,
        .managed_handle = 0,
    });

    // Run one frame manually
    // script_create_system should create the managed instance
    // script_execution_system should call OnUpdate
    world.insert_resource(Time{}); // ensure Time resource exists

    // Simulate one tick
    // (In a real app, App::run() drives the scheduler. Here we call systems directly.)
    script_create_system(world);

    auto& script = world.get<ScriptInstance>(e);
    assert(script.managed_handle != 0 && "Managed instance should be created");
    assert(script.script_type_id != 0 && "Type ID should be assigned");

    script_execution_system(world);

    // Cleanup
    script_destroy_system(world);

    std::cout << "Integration test passed." << std::endl;
    return 0;
}
```

- [ ] **Step 2: Add integration test to CMake**

In `helios-script/CMakeLists.txt`, add (gated behind a flag):

```cmake
option(HELIOS_BUILD_TESTS "Build helios tests" ON)

if(HELIOS_BUILD_TESTS)
    add_executable(test-scripting tests/integration_test_scripting.cpp)
    target_link_libraries(test-scripting PRIVATE helios-script helios-core)
    target_compile_definitions(test-scripting PRIVATE
        HELIOS_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    )
endif()
```

---

## Task 14: Unit tests with MockScriptRuntime

**Files:**
- Create: `helios-script/tests/mock_script_runtime.h`
- Create: `helios-script/tests/test_script_lifecycle.cpp`
- Create: `helios-script/tests/test_hot_reload.cpp`

Unit tests that do NOT require CoreCLR. They use a `MockScriptRuntime` that tracks calls and simulates behavior.

- [ ] **Step 1: Create `MockScriptRuntime`**

```cpp
// helios-script/tests/mock_script_runtime.h
#pragma once

#include <helios/script/script_runtime.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace helios::test {

/// Mock ScriptRuntime for unit testing.
/// Records all calls for verification. Does not require CoreCLR.
class MockScriptRuntime : public ScriptRuntime {
public:
    // ── State tracking ──────────────────────────────────────────

    struct CreateCall {
        std::string class_name;
        uint64_t entity_id;
    };

    struct UpdateCall {
        uint32_t type_id;
        std::vector<uint64_t> entity_ids;
        std::vector<uint64_t> managed_handles;
        float delta;
    };

    struct DestroyCall {
        uint64_t entity_id;
        uint64_t managed_handle;
    };

    std::vector<CreateCall> create_calls;
    std::vector<UpdateCall> update_calls;
    std::vector<DestroyCall> destroy_calls;
    int destroy_all_count = 0;

    bool assembly_loaded = false;
    std::string loaded_assembly_path;
    int reload_count = 0;

    // Classes to simulate as existing
    std::vector<std::string> available_classes;

    // Next handle to return from invoke_create
    uint64_t next_handle = 1;

    // ── ScriptRuntime implementation ────────────────────────────

    bool load_assembly(const std::filesystem::path& path) override {
        loaded_assembly_path = path.string();
        assembly_loaded = true;
        return true;
    }

    void unload_assembly() override {
        assembly_loaded = false;
        loaded_assembly_path.clear();
    }

    bool reload_assembly(const std::filesystem::path& path) override {
        unload_assembly();
        reload_count++;
        return load_assembly(path);
    }

    bool class_exists(const std::string& name) const override {
        for (auto& c : available_classes)
            if (c == name) return true;
        return false;
    }

    std::vector<std::string> get_script_class_names() const override {
        return available_classes;
    }

    uint32_t get_script_type_id(const std::string& name) const override {
        return static_cast<uint32_t>(std::hash<std::string>{}(name));
    }

    uint64_t invoke_create(const std::string& class_name, uint64_t entity_id) override {
        create_calls.push_back({class_name, entity_id});
        return next_handle++;
    }

    void invoke_update(uint32_t type_id,
                       const uint64_t* entity_ids,
                       const uint64_t* handles,
                       size_t count,
                       float delta) override {
        UpdateCall call;
        call.type_id = type_id;
        call.delta = delta;
        call.entity_ids.assign(entity_ids, entity_ids + count);
        call.managed_handles.assign(handles, handles + count);
        update_calls.push_back(std::move(call));
    }

    void invoke_destroy(uint64_t entity_id, uint64_t handle) override {
        destroy_calls.push_back({entity_id, handle});
    }

    void destroy_all_instances() override {
        destroy_all_count++;
    }

    void request_reload() override { m_reload = true; }
    bool reload_requested() const override { return m_reload; }
    void clear_reload_request() override { m_reload = false; }

private:
    bool m_reload = false;
};

} // namespace helios::test
```

- [ ] **Step 2: Create `test_script_lifecycle.cpp`**

Tests the create/update/destroy cycle using MockScriptRuntime:

```cpp
// helios-script/tests/test_script_lifecycle.cpp
#include "mock_script_runtime.h"

#include <helios/core/world.h>
#include <helios/core/time.h>
#include <helios/script/script_instance.h>

// Include the system implementations directly (they are internal)
#include "../src/script_execution_system.h"

#include <cassert>
#include <iostream>
#include <memory>

using namespace helios;
using namespace helios::test;

void test_create_system() {
    World world;
    world.insert_resource(Time{});

    auto mock = std::make_unique<MockScriptRuntime>();
    mock->available_classes.push_back("Game.TestScript");
    auto* mock_ptr = mock.get();

    world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(mock));

    // Spawn entity with ScriptInstance
    Entity e = world.spawn();
    world.add(e, ScriptInstance{
        .script_class_name = "Game.TestScript",
    });

    // Run create system
    script_create_system(world);

    // Verify
    assert(mock_ptr->create_calls.size() == 1);
    assert(mock_ptr->create_calls[0].class_name == "Game.TestScript");

    auto& script = world.get<ScriptInstance>(e);
    assert(script.managed_handle != 0);
    assert(script.script_type_id != 0);
    assert(world.has<ScriptInitialized>(e));

    std::cout << "  PASS: test_create_system" << std::endl;
}

void test_execution_batching() {
    World world;
    Time time_res;
    // Simulate 16ms frame
    // time_res.set_delta(0.016f);  // depends on Time API from Plan 1
    world.insert_resource(time_res);

    auto mock = std::make_unique<MockScriptRuntime>();
    mock->available_classes = {"Game.ScriptA", "Game.ScriptB"};
    auto* mock_ptr = mock.get();
    world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(mock));

    // Spawn 3 entities: 2 with ScriptA, 1 with ScriptB
    Entity e1 = world.spawn();
    world.add(e1, ScriptInstance{.script_class_name = "Game.ScriptA"});

    Entity e2 = world.spawn();
    world.add(e2, ScriptInstance{.script_class_name = "Game.ScriptA"});

    Entity e3 = world.spawn();
    world.add(e3, ScriptInstance{.script_class_name = "Game.ScriptB"});

    // Create instances
    script_create_system(world);
    assert(mock_ptr->create_calls.size() == 3);

    // Run execution system
    script_execution_system(world);

    // Should have 2 batch update calls (one per type)
    assert(mock_ptr->update_calls.size() == 2);

    // Find the ScriptA batch -- should have 2 entities
    bool found_a = false, found_b = false;
    for (auto& call : mock_ptr->update_calls) {
        if (call.entity_ids.size() == 2) found_a = true;
        if (call.entity_ids.size() == 1) found_b = true;
    }
    assert(found_a && "ScriptA batch should have 2 entities");
    assert(found_b && "ScriptB batch should have 1 entity");

    std::cout << "  PASS: test_execution_batching" << std::endl;
}

void test_no_double_create() {
    World world;
    world.insert_resource(Time{});

    auto mock = std::make_unique<MockScriptRuntime>();
    mock->available_classes.push_back("Game.TestScript");
    auto* mock_ptr = mock.get();
    world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(mock));

    Entity e = world.spawn();
    world.add(e, ScriptInstance{.script_class_name = "Game.TestScript"});

    // Run create system twice
    script_create_system(world);
    script_create_system(world);

    // Should only create once (ScriptInitialized marker prevents re-creation)
    assert(mock_ptr->create_calls.size() == 1);

    std::cout << "  PASS: test_no_double_create" << std::endl;
}

void test_unknown_class() {
    World world;
    world.insert_resource(Time{});

    auto mock = std::make_unique<MockScriptRuntime>();
    // available_classes is empty -- class does not exist
    auto* mock_ptr = mock.get();
    world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(mock));

    Entity e = world.spawn();
    world.add(e, ScriptInstance{.script_class_name = "Game.NonExistent"});

    script_create_system(world);

    // Should NOT create an instance
    assert(mock_ptr->create_calls.empty());
    auto& script = world.get<ScriptInstance>(e);
    assert(script.managed_handle == 0);

    std::cout << "  PASS: test_unknown_class" << std::endl;
}

int main() {
    std::cout << "Running script lifecycle tests..." << std::endl;
    test_create_system();
    test_execution_batching();
    test_no_double_create();
    test_unknown_class();
    std::cout << "All script lifecycle tests passed." << std::endl;
    return 0;
}
```

- [ ] **Step 3: Create `test_hot_reload.cpp`**

```cpp
// helios-script/tests/test_hot_reload.cpp
#include "mock_script_runtime.h"

#include <helios/core/world.h>
#include <helios/core/time.h>
#include <helios/script/script_instance.h>

#include "../src/script_execution_system.h"
#include "../src/hot_reload_system.h"

#include <cassert>
#include <iostream>
#include <memory>

using namespace helios;
using namespace helios::test;

void test_hot_reload_cycle() {
    World world;
    world.insert_resource(Time{});
    world.insert_resource(ScriptAssemblyPath{"/fake/path/TestScripts.dll"});

    auto mock = std::make_unique<MockScriptRuntime>();
    mock->available_classes.push_back("Game.TestScript");
    auto* mock_ptr = mock.get();
    world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(mock));

    // Spawn entity with script
    Entity e = world.spawn();
    world.add(e, ScriptInstance{.script_class_name = "Game.TestScript"});

    // Create instance
    script_create_system(world);
    assert(mock_ptr->create_calls.size() == 1);
    auto& script = world.get<ScriptInstance>(e);
    assert(script.managed_handle != 0);

    // Request reload
    mock_ptr->request_reload();
    assert(mock_ptr->reload_requested());

    // Run hot reload system
    check_script_reload(world);

    // Verify:
    // 1. destroy_all was called
    assert(mock_ptr->destroy_all_count == 1);
    // 2. reload was performed
    assert(mock_ptr->reload_count == 1);
    // 3. reload request was cleared
    assert(!mock_ptr->reload_requested());
    // 4. ScriptInitialized marker was removed (script_create_system will re-create)
    assert(!world.has<ScriptInitialized>(e));
    // 5. managed_handle was zeroed
    auto& script_after = world.get<ScriptInstance>(e);
    assert(script_after.managed_handle == 0);
    assert(script_after.script_type_id == 0);

    // Run create system again -- should re-create
    script_create_system(world);
    assert(mock_ptr->create_calls.size() == 2); // second creation
    assert(world.get<ScriptInstance>(e).managed_handle != 0);

    std::cout << "  PASS: test_hot_reload_cycle" << std::endl;
}

void test_no_reload_when_not_requested() {
    World world;
    world.insert_resource(Time{});

    auto mock = std::make_unique<MockScriptRuntime>();
    auto* mock_ptr = mock.get();
    world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(mock));

    // Do NOT request reload
    check_script_reload(world);

    assert(mock_ptr->reload_count == 0);
    assert(mock_ptr->destroy_all_count == 0);

    std::cout << "  PASS: test_no_reload_when_not_requested" << std::endl;
}

int main() {
    std::cout << "Running hot reload tests..." << std::endl;
    test_hot_reload_cycle();
    test_no_reload_when_not_requested();
    std::cout << "All hot reload tests passed." << std::endl;
    return 0;
}
```

- [ ] **Step 4: Add tests to CMake**

```cmake
# In helios-script/CMakeLists.txt, inside the if(HELIOS_BUILD_TESTS) block:
add_executable(test-script-lifecycle tests/test_script_lifecycle.cpp)
target_link_libraries(test-script-lifecycle PRIVATE helios-script helios-core)

add_executable(test-hot-reload tests/test_hot_reload.cpp)
target_link_libraries(test-hot-reload PRIVATE helios-script helios-core)
```

- [ ] **Step 5: Verify tests compile and pass**

```bash
cmake --build build --target test-script-lifecycle test-hot-reload -j$(nproc) \
  && ./build/test-script-lifecycle \
  && ./build/test-hot-reload
```

---

## Task 15: Update C# project structure for the new architecture

**Files:**
- Modify: `ScriptCore/ScriptCore.csproj`
- Modify: `ScriptCore/Source/Helios/Scene/Entity.cs` (update to use NativeAPI with world_context)
- Modify: `ScriptCore/Source/Helios/Scene/Components.cs` (update all NativeAPI calls)
- Modify: `ScriptCore/Source/Helios/Input.cs`

Update all existing C# files to pass `WorldContext` through the updated `NativeAPI`. The Entity class remains but is now a lightweight wrapper (not the base class for user scripts -- that role moves to `Script`).

- [ ] **Step 1: Update `Entity.cs`**

```csharp
// ScriptCore/Source/Helios/Scene/Entity.cs
using System;

namespace Helios;

/// <summary>
/// Lightweight entity wrapper. Provides component access via NativeEngineAPI.
/// This is NOT the base class for user scripts (that is Script).
/// </summary>
public class Entity
{
    public Entity() { ID = 0; }
    public Entity(ulong id) { ID = id; }

    public ulong ID;

    public void Destroy() => NativeAPI.Despawn(ID);

    public bool HasComponent<T>() where T : Component, new()
        => NativeAPI.HasComponent(ID, typeof(T).Name);

    public T? GetComponent<T>() where T : Component, new()
    {
        if (!HasComponent<T>()) return null;
        return new T() { Entity = this };
    }

    public bool IsAlive() => NativeAPI.IsAlive(ID);
}
```

- [ ] **Step 2: Update `Input.cs`**

```csharp
// All calls now go through NativeAPI which handles world_context internally.
// No changes needed in the public API -- NativeAPI.IsKeyPressed() etc.
// already delegate correctly after Task 12 Step 4 updates.
```

- [ ] **Step 3: Update `Components.cs`**

All component wrapper methods already call `NativeAPI.TransformGetTranslation(Entity.ID)` etc. After Task 12 Step 4 updates `NativeAPI.cs` to pass `WorldContext`, these continue to work without changes to `Components.cs`. Verify compilation.

- [ ] **Step 4: Verify full C# build**

```bash
cd ScriptCore && dotnet build -c Release 2>&1 | tail -5
```

---

## Summary

| Task | Description | Files | Dependencies |
|------|-------------|-------|--------------|
| 1 | CMake target structure | `helios-script/CMakeLists.txt`, stubs | None |
| 2 | `ScriptRuntime` abstract interface | `script_runtime.h` | None |
| 3 | `ManagedBridge` struct | `managed_bridge.h` | None |
| 4 | `NativeEngineAPI` struct | `native_engine_api.h` | None |
| 5 | `ScriptGlue` implementation | `script_glue.h/.cpp` | Tasks 2, 4 |
| 6 | `CoreCLRRuntime` (HostFXR RAII) | `coreclr_runtime.h/.cpp` | Tasks 2, 3, 5 |
| 7 | `ScriptInstance` component | `script_instance.h` | None |
| 8 | `ScriptExecutionSystem` | `script_execution_system.h/.cpp` | Tasks 2, 7 |
| 9 | File watcher | `file_watcher.h/.cpp` | None |
| 10 | Hot reload system | `hot_reload_system.h/.cpp` | Tasks 2, 7, 9 |
| 11 | `ScriptingPlugin` | `scripting_plugin.h/.cpp` | Tasks 6, 8, 9, 10 |
| 12 | C# side (Script, bridge updates) | `ScriptCore/` multiple files | Tasks 3, 4 |
| 13 | Integration test | `tests/integration_test_scripting.cpp` | Tasks 1-12 |
| 14 | Unit tests with mock | `tests/test_*.cpp`, `mock_script_runtime.h` | Tasks 2, 7, 8, 10 |
| 15 | C# project updates | `ScriptCore/` existing files | Task 12 |

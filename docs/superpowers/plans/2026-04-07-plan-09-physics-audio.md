# Physics and Audio Backend Abstraction -- Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create backend-abstracted physics and audio subsystems behind `helios-physics/` and `helios-audio/` CMake targets. Physics uses Jolt Physics behind a `PhysicsWorld` virtual interface. Audio uses SoLoud behind an `AudioDevice` virtual interface. Both integrate with the ECS via typed plugins that register systems, resources, and events.

**Architecture:** Engine code talks to abstract interfaces (`physics::PhysicsWorld`, `audio::AudioDevice`). Jolt and SoLoud implement these behind `std::unique_ptr`. Physics and audio components (`RigidBody`, `BoxCollider`, `SphereCollider`, `AudioSource`) are plain aggregate structs defined in `helios-core`. Plugins wire everything together: inserting resources, registering systems into the correct schedules, and emitting ECS events for collision callbacks.

**Tech Stack:** Jolt Physics (vendored), SoLoud (vendored), glm, C++20

**Spec:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` -- sections 5 (Physics) and 6 (Audio)

**Dependencies:** Plans 1--2 (ECS Core, App & Plugin System). These provide `World`, `App`, `Scheduler`, `Entity`, `Query`, `Res`/`ResMut`, `EventWriter`/`EventReader`, `Commands`, `Schedule` enum, and the `Plugin` concept (`void build(App&)`).

**IMPORTANT -- Design Principles:**
- **RAII everywhere.** `JoltPhysicsWorld` constructor initializes the Jolt system (register allocator, factory, types, create `JPH::PhysicsSystem`). Destructor tears it all down. No `Init()`/`Shutdown()` methods.
- **No global state.** The existing `PhysicsEngine` singleton and its static members are eliminated. All state lives inside `JoltPhysicsWorld` (owned by the ECS World as a resource).
- **Prefer std library.** Use `std::unique_ptr`, `std::unordered_map`, `std::vector`, `std::variant`, `std::optional`. No custom `Ref<T>`/`Scope<T>`.
- **Components are plain data.** `RigidBody`, `BoxCollider`, `SphereCollider`, `AudioSource` are aggregates with no inheritance, no virtual methods, no smart pointers.
- **Never edit vendored source.** Jolt and SoLoud `.h/.cpp` files in `vendor/` are untouched. Use compiler flags (`-DJPH_*`, `-DWITH_MINIAUDIO`) in CMake instead.

**Existing code reference:** The current Jolt integration lives in `Engine/src/Physics/` (`HPhysicsScene.cpp`, `PhysicsEngine.cpp`, `HContactListener.cpp`). The new implementation adapts the same Jolt API patterns (body creation, contact listener, broad phase layers) but wraps them behind the abstract interface and eliminates all singletons, static state, and raw `new`/`delete`.

---

## Phase 1: Physics Interface and Types

### Task 1: Create helios-physics CMake target with directory skeleton

**Files:**
- Create: `helios-physics/CMakeLists.txt`
- Create: `helios-physics/src/interface/body_types.h`
- Create: `helios-physics/src/interface/contact_event.h`
- Create: `helios-physics/src/interface/physics_world.h`
- Create: `helios-physics/src/interface/physics_plugin.h`
- Create: `helios-physics/src/jolt/jolt_physics_world.h`
- Create: `helios-physics/src/jolt/jolt_physics_world.cpp`
- Create: `helios-physics/src/jolt/jolt_utils.h`
- Modify: root `CMakeLists.txt` (add `add_subdirectory(helios-physics)`)

- [ ] **Step 1: Create directory skeleton**

```bash
mkdir -p helios-physics/src/interface
mkdir -p helios-physics/src/jolt
```

- [ ] **Step 2: Create helios-physics/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)

add_library(helios-physics STATIC
    src/jolt/jolt_physics_world.cpp
)

target_include_directories(helios-physics
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/jolt
)

# Jolt Physics vendored dependency
target_include_directories(helios-physics PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/jolt
)

target_link_libraries(helios-physics
    PUBLIC  helios-core
    PRIVATE JoltPhysics
)

# Jolt compile definitions -- match the vendored build
target_compile_definitions(helios-physics PRIVATE
    JPH_PROFILE_ENABLED=$<BOOL:$<CONFIG:Debug>>
    JPH_DEBUG_RENDERER=$<BOOL:$<CONFIG:Debug>>
    JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=0
)

target_compile_features(helios-physics PUBLIC cxx_std_20)
```

- [ ] **Step 3: Add to root CMakeLists.txt**

Add the following line after the existing `add_subdirectory(helios-core)`:

```cmake
add_subdirectory(helios-physics)
```

- [ ] **Step 4: Verify directory structure compiles (placeholder files)**

Create empty placeholder `.cpp` so CMake can configure:

```bash
touch helios-physics/src/jolt/jolt_physics_world.cpp
```

Run:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target helios-physics 2>&1 | head -20
```

Expected: CMake configures. Build may fail because the source file is empty, but the target must be recognized. This verifies the CMake plumbing.

- [ ] **Step 5: Commit**

```bash
git add helios-physics/
git commit -m "build: add helios-physics CMake target with directory skeleton"
```

---

### Task 2: Define physics types -- BodyHandle, BodyType, ColliderShape, BodyDesc

**Files:**
- Create: `helios-physics/src/interface/body_types.h`

- [ ] **Step 1: Create body_types.h**

```cpp
// helios-physics/src/interface/body_types.h
#pragma once

#include <cstdint>
#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace helios::physics {

// Opaque handle into the physics backend. Zero means invalid.
using BodyHandle = uint64_t;

enum class BodyType : uint8_t {
    Static,       // Non-movable. Infinite mass. Placed once.
    Dynamic,      // Responds to forces, gravity, impulses.
    Kinematic,    // Movable by code, does not respond to forces. Pushes dynamic bodies.
};

// --- Collider shape descriptors (plain data, no Jolt types) ---

struct BoxShape {
    glm::vec3 half_extents{0.5f};
};

struct SphereShape {
    float radius = 0.5f;
};

struct CapsuleShape {
    float half_height = 0.5f;
    float radius      = 0.25f;
};

// Variant of all supported collider shapes.
// Extend this variant when adding new shape types (MeshShape, ConvexHullShape, etc.).
using ColliderShape = std::variant<BoxShape, SphereShape, CapsuleShape>;

// Complete description needed to create a physics body in the backend.
struct BodyDesc {
    BodyType       type        = BodyType::Static;
    glm::vec3      position    {0.0f};
    glm::quat      rotation    {1.0f, 0.0f, 0.0f, 0.0f};  // w, x, y, z
    float          mass        = 1.0f;
    float          friction    = 0.5f;
    float          restitution = 0.3f;
    ColliderShape  shape       = BoxShape{};
};

} // namespace helios::physics
```

- [ ] **Step 2: Verify header compiles standalone**

Create a minimal test translation unit:
```bash
echo '#include "interface/body_types.h"' > helios-physics/src/jolt/jolt_physics_world.cpp
```

Rebuild:
```bash
cmake --build build --target helios-physics 2>&1 | grep "error:" | head -5
```

Expected: no errors from body_types.h parsing.

- [ ] **Step 3: Commit**

```bash
git add helios-physics/src/interface/body_types.h
git commit -m "feat(physics): add BodyHandle, BodyType, ColliderShape, BodyDesc types"
```

---

### Task 3: Define ContactEvent and RayHit

**Files:**
- Create: `helios-physics/src/interface/contact_event.h`

- [ ] **Step 1: Create contact_event.h**

```cpp
// helios-physics/src/interface/contact_event.h
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

// Forward-declare Entity from helios-core. The physics interface uses Entity
// to identify which ECS entities are involved in contacts, but does not
// depend on full ECS headers.
namespace helios::ecs {
    struct Entity;
}

namespace helios::physics {

// Contact reported by the physics backend after a simulation step.
// entity_a and entity_b are the ECS entities whose bodies collided.
// The backend maps BodyHandle -> Entity via user data stored on each body.
struct ContactEvent {
    uint64_t   entity_a    = 0;    // Entity ID of first body
    uint64_t   entity_b    = 0;    // Entity ID of second body
    glm::vec3  world_point {0.0f}; // Contact point in world space
    glm::vec3  normal      {0.0f}; // Contact normal (from A toward B)
    float      impulse     = 0.0f; // Normal impulse magnitude
};

// Result of a single ray-vs-world intersection.
struct RayHit {
    uint64_t   entity   = 0;      // Entity ID of the hit body
    glm::vec3  point    {0.0f};   // Hit point in world space
    glm::vec3  normal   {0.0f};   // Surface normal at hit point
    float      distance = 0.0f;   // Distance from ray origin to hit point
};

} // namespace helios::physics
```

- [ ] **Step 2: Commit**

```bash
git add helios-physics/src/interface/contact_event.h
git commit -m "feat(physics): add ContactEvent and RayHit structs"
```

---

### Task 4: Define PhysicsWorld abstract interface

**Files:**
- Create: `helios-physics/src/interface/physics_world.h`

- [ ] **Step 1: Create physics_world.h**

This is the backend-agnostic abstract class. All engine code and systems program against this interface. The Jolt implementation is behind `std::unique_ptr<PhysicsWorld>`.

```cpp
// helios-physics/src/interface/physics_world.h
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "interface/body_types.h"
#include "interface/contact_event.h"

namespace helios::physics {

// PhysicsConfig resource inserted by the plugin. Systems read this
// for fixed timestep, gravity, etc.
struct PhysicsConfig {
    float     fixed_timestep = 1.0f / 60.0f;
    glm::vec3 gravity        {0.0f, -9.81f, 0.0f};
    int       collision_steps = 1;
};

// Abstract physics world interface.
//
// Implementations:
//   JoltPhysicsWorld -- production Jolt backend
//   MockPhysicsWorld -- in-memory stub for unit tests (no Jolt dependency)
//
// Ownership: created by the plugin, stored as a World resource via
//   std::unique_ptr<PhysicsWorld>.
class PhysicsWorld {
public:
    virtual ~PhysicsWorld() = default;

    // --- Body lifecycle ---

    // Create a body from a descriptor. Returns an opaque handle.
    // The backend stores the entity_id as user data on the body so that
    // contacts can be mapped back to ECS entities.
    virtual BodyHandle create_body(const BodyDesc& desc, uint64_t entity_id = 0) = 0;

    // Remove and destroy a body. The handle becomes invalid.
    virtual void destroy_body(BodyHandle handle) = 0;

    // --- Transform ---

    virtual void      set_transform(BodyHandle handle,
                                    const glm::vec3& pos,
                                    const glm::quat& rot) = 0;
    virtual glm::vec3 get_position(BodyHandle handle) const = 0;
    virtual glm::quat get_rotation(BodyHandle handle) const = 0;

    // --- Velocity / forces ---

    virtual void set_velocity(BodyHandle handle, const glm::vec3& linear) = 0;
    virtual void apply_force(BodyHandle handle, const glm::vec3& force) = 0;
    virtual void apply_impulse(BodyHandle handle, const glm::vec3& impulse) = 0;

    // --- Simulation ---

    // Advance the simulation by dt seconds. Internally may sub-step.
    virtual void step(float dt) = 0;

    // Drain all contact events accumulated during the last step().
    // The returned vector is moved out; the internal buffer is cleared.
    virtual std::vector<ContactEvent> drain_contacts() = 0;

    // --- Raycasting ---

    // Cast a ray and return the closest hit, or std::nullopt.
    virtual std::optional<RayHit> raycast(const glm::vec3& origin,
                                          const glm::vec3& direction,
                                          float max_distance) const = 0;

    // Cast a ray and return all hits sorted by distance.
    virtual std::vector<RayHit> raycast_all(const glm::vec3& origin,
                                            const glm::vec3& direction,
                                            float max_distance) const = 0;

    // --- Configuration ---

    virtual void set_gravity(const glm::vec3& gravity) = 0;
};

} // namespace helios::physics
```

- [ ] **Step 2: Commit**

```bash
git add helios-physics/src/interface/physics_world.h
git commit -m "feat(physics): add PhysicsWorld abstract interface"
```

---

## Phase 2: Jolt Backend Implementation

### Task 5: Create Jolt utility helpers

**Files:**
- Create: `helios-physics/src/jolt/jolt_utils.h`

- [ ] **Step 1: Create jolt_utils.h**

Conversion functions between glm and Jolt types, broad phase layer definitions, and the filter implementations. These are adapted from the existing `HPhysicsScene.h` (`BPLayerInterfaceImpl`, `ObjectVsBroadPhaseLayerFilterImpl`, `ObjectLayerPairFilterImpl`) and the static conversion functions in `HPhysicsScene.cpp`.

```cpp
// helios-physics/src/jolt/jolt_utils.h
#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "interface/body_types.h"

namespace helios::physics::jolt {

// ---- glm <-> Jolt conversions ----

inline JPH::Vec3 to_jph(const glm::vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

inline JPH::RVec3 to_jph_rvec(const glm::vec3& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}

inline JPH::Quat to_jph(const glm::quat& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

inline glm::vec3 to_glm(const JPH::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

inline glm::vec3 to_glm(const JPH::RVec3& v) {
    return glm::vec3(
        static_cast<float>(v.GetX()),
        static_cast<float>(v.GetY()),
        static_cast<float>(v.GetZ())
    );
}

inline glm::quat to_glm(const JPH::Quat& q) {
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

// ---- BodyType -> JPH::EMotionType ----

inline JPH::EMotionType to_jph_motion_type(BodyType type) {
    switch (type) {
        case BodyType::Static:    return JPH::EMotionType::Static;
        case BodyType::Dynamic:   return JPH::EMotionType::Dynamic;
        case BodyType::Kinematic: return JPH::EMotionType::Kinematic;
    }
    return JPH::EMotionType::Static;
}

// ---- Broad phase layers (adapted from existing Layers.h) ----

namespace layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING     = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace broad_phase_layers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS = 2;
}

// Maps object layers to broad phase layers.
// Adapted from existing BPLayerInterfaceImpl.
class BroadPhaseLayerMapping final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerMapping() {
        m_object_to_broad_phase[layers::NON_MOVING] = broad_phase_layers::NON_MOVING;
        m_object_to_broad_phase[layers::MOVING]     = broad_phase_layers::MOVING;
    }

    JPH::uint GetNumBroadPhaseLayers() const override {
        return broad_phase_layers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return m_object_to_broad_phase[layer];
    }

private:
    JPH::BroadPhaseLayer m_object_to_broad_phase[layers::NUM_LAYERS];
};

// Determines if an object layer can collide with a broad phase layer.
class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broad) const override {
        switch (object) {
            case layers::NON_MOVING: return broad == broad_phase_layers::MOVING;
            case layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

// Determines if two object layers can collide.
class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        switch (a) {
            case layers::NON_MOVING: return b == layers::MOVING;
            case layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

// Returns the object layer for a given body type.
inline JPH::ObjectLayer object_layer_for(BodyType type) {
    return (type == BodyType::Static) ? layers::NON_MOVING : layers::MOVING;
}

} // namespace helios::physics::jolt
```

- [ ] **Step 2: Commit**

```bash
git add helios-physics/src/jolt/jolt_utils.h
git commit -m "feat(physics): add Jolt utility helpers (conversions, layer filters)"
```

---

### Task 6: Implement JoltPhysicsWorld -- construction, destruction, body lifecycle

**Files:**
- Create: `helios-physics/src/jolt/jolt_physics_world.h`
- Create: `helios-physics/src/jolt/jolt_physics_world.cpp`

This is the core task. `JoltPhysicsWorld` implements `PhysicsWorld` using Jolt. The constructor fully initializes Jolt (register allocator, factory, types, create `JPH::PhysicsSystem`, set up contact listener). The destructor tears everything down. No `Init()`/`Shutdown()`.

Adapted from the existing `PhysicsEngine::Init()` + `HPhysicsScene` constructor. Key differences from old code:
- No singleton (`PhysicsEngine::Get()`) -- this is a regular RAII object
- No static allocator/thread pool -- each world owns its own
- No `Ref<T>` / `CreateRef` -- uses `std::unique_ptr` and `std::make_unique`
- No raw `new`/`delete` for bodies -- Jolt body interface manages lifecycle
- Contact listener stores contacts in a thread-safe buffer, drained after `step()`
- Entity IDs stored as Jolt user data on bodies (same pattern as old code: `body->SetUserData(entity_id)`)

- [ ] **Step 1: Create jolt_physics_world.h**

```cpp
// helios-physics/src/jolt/jolt_physics_world.h
#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include "interface/physics_world.h"
#include "jolt/jolt_utils.h"

namespace helios::physics {

// Thread-safe contact listener that accumulates ContactEvents during
// simulation. Contacts are drained after each step() call.
class JoltContactListener final : public JPH::ContactListener {
public:
    JPH::ValidateResult OnContactValidate(
        const JPH::Body& body1,
        const JPH::Body& body2,
        JPH::RVec3Arg base_offset,
        const JPH::CollideShapeResult& collision_result) override;

    void OnContactAdded(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override;

    void OnContactPersisted(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override;

    void OnContactRemoved(
        const JPH::SubShapeIDPair& sub_shape_pair) override;

    // Move-drain all accumulated contacts. Called from the main thread
    // after step(). Clears the internal buffer.
    std::vector<ContactEvent> drain();

private:
    std::mutex              m_mutex;
    std::vector<ContactEvent> m_contacts;
};

// Jolt body activation listener (logging only, no gameplay logic).
class JoltActivationListener final : public JPH::BodyActivationListener {
public:
    void OnBodyActivated(const JPH::BodyID& body_id, JPH::uint64 user_data) override;
    void OnBodyDeactivated(const JPH::BodyID& body_id, JPH::uint64 user_data) override;
};

// Production PhysicsWorld implementation backed by Jolt Physics.
//
// RAII: constructor initializes the entire Jolt subsystem. Destructor
// cleans up all bodies and shuts down Jolt. No Init()/Shutdown().
//
// Thread safety: step() must be called from one thread. create_body /
// destroy_body should be called outside of step() (during system
// execution on the main thread, which is the standard ECS pattern).
class JoltPhysicsWorld final : public PhysicsWorld {
public:
    // temp_allocator_size_mb: size of the per-frame temporary allocator in MB.
    //                         10 MB is a reasonable default.
    explicit JoltPhysicsWorld(const PhysicsConfig& config,
                              uint32_t temp_allocator_size_mb = 10);
    ~JoltPhysicsWorld() override;

    // Non-copyable, non-movable (Jolt state is not trivially relocatable)
    JoltPhysicsWorld(const JoltPhysicsWorld&) = delete;
    JoltPhysicsWorld& operator=(const JoltPhysicsWorld&) = delete;
    JoltPhysicsWorld(JoltPhysicsWorld&&) = delete;
    JoltPhysicsWorld& operator=(JoltPhysicsWorld&&) = delete;

    // --- PhysicsWorld interface ---

    BodyHandle create_body(const BodyDesc& desc, uint64_t entity_id = 0) override;
    void       destroy_body(BodyHandle handle) override;

    void       set_transform(BodyHandle handle,
                             const glm::vec3& pos,
                             const glm::quat& rot) override;
    glm::vec3  get_position(BodyHandle handle) const override;
    glm::quat  get_rotation(BodyHandle handle) const override;

    void       set_velocity(BodyHandle handle, const glm::vec3& linear) override;
    void       apply_force(BodyHandle handle, const glm::vec3& force) override;
    void       apply_impulse(BodyHandle handle, const glm::vec3& impulse) override;

    void       step(float dt) override;
    std::vector<ContactEvent> drain_contacts() override;

    std::optional<RayHit> raycast(const glm::vec3& origin,
                                  const glm::vec3& direction,
                                  float max_distance) const override;
    std::vector<RayHit>   raycast_all(const glm::vec3& origin,
                                      const glm::vec3& direction,
                                      float max_distance) const override;

    void set_gravity(const glm::vec3& gravity) override;

private:
    // Jolt subsystem objects -- order matters for destruction
    std::unique_ptr<JPH::TempAllocator>       m_temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool>  m_job_system;
    std::unique_ptr<JPH::PhysicsSystem>        m_physics_system;

    // Layer filters (owned, non-polymorphic)
    jolt::BroadPhaseLayerMapping    m_broad_phase_layer_mapping;
    jolt::ObjectVsBroadPhaseFilter  m_object_vs_broad_phase_filter;
    jolt::ObjectLayerPairFilter     m_object_layer_pair_filter;

    // Listeners
    JoltContactListener     m_contact_listener;
    JoltActivationListener  m_activation_listener;

    // BodyHandle -> JPH::BodyID mapping. BodyHandle is the entity_id
    // stored as Jolt user data. This map allows O(1) lookup from our
    // opaque handle to the Jolt BodyID for API calls.
    std::unordered_map<BodyHandle, JPH::BodyID> m_handle_to_body;

    // Configuration
    PhysicsConfig m_config;
    bool          m_broad_phase_optimized = false;

    // Jolt system limits (matching existing HPhysicsScene defaults)
    static constexpr uint32_t MAX_BODIES             = 65536;
    static constexpr uint32_t NUM_BODY_MUTEXES       = 0;     // auto
    static constexpr uint32_t MAX_BODY_PAIRS          = 65536;
    static constexpr uint32_t MAX_CONTACT_CONSTRAINTS = 10240;

    // Internal: get the body interface (non-locking for single-threaded access)
    JPH::BodyInterface& body_interface();
    const JPH::BodyInterface& body_interface() const;
};

} // namespace helios::physics
```

- [ ] **Step 2: Create jolt_physics_world.cpp**

```cpp
// helios-physics/src/jolt/jolt_physics_world.cpp

#include "jolt/jolt_physics_world.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <thread>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

namespace helios::physics {

// ============================================================
// JoltContactListener
// ============================================================

JPH::ValidateResult JoltContactListener::OnContactValidate(
    const JPH::Body& /*body1*/,
    const JPH::Body& /*body2*/,
    JPH::RVec3Arg /*base_offset*/,
    const JPH::CollideShapeResult& /*collision_result*/)
{
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void JoltContactListener::OnContactAdded(
    const JPH::Body& body1,
    const JPH::Body& body2,
    const JPH::ContactManifold& manifold,
    JPH::ContactSettings& /*settings*/)
{
    ContactEvent event;
    event.entity_a    = body1.GetUserData();
    event.entity_b    = body2.GetUserData();
    event.world_point = jolt::to_glm(manifold.mBaseOffset +
                        manifold.mRelativeContactPointsOn1[0]);
    event.normal      = jolt::to_glm(manifold.mWorldSpaceNormal);
    event.impulse     = 0.0f; // Impulse not available in OnContactAdded

    std::lock_guard lock(m_mutex);
    m_contacts.push_back(event);
}

void JoltContactListener::OnContactPersisted(
    const JPH::Body& /*body1*/,
    const JPH::Body& /*body2*/,
    const JPH::ContactManifold& /*manifold*/,
    JPH::ContactSettings& /*settings*/)
{
    // Persisted contacts are not emitted as events by default.
    // Users who need per-frame contact info can extend this.
}

void JoltContactListener::OnContactRemoved(
    const JPH::SubShapeIDPair& /*sub_shape_pair*/)
{
    // Contact-removed events could be added here if needed.
}

std::vector<ContactEvent> JoltContactListener::drain() {
    std::lock_guard lock(m_mutex);
    std::vector<ContactEvent> result = std::move(m_contacts);
    m_contacts.clear();
    return result;
}

// ============================================================
// JoltActivationListener
// ============================================================

void JoltActivationListener::OnBodyActivated(
    const JPH::BodyID& /*body_id*/, JPH::uint64 /*user_data*/)
{
    // Logging hook. In production, route through helios logging:
    // HVE_CORE_TRACE_TAG("Physics", "Body activated");
}

void JoltActivationListener::OnBodyDeactivated(
    const JPH::BodyID& /*body_id*/, JPH::uint64 /*user_data*/)
{
    // HVE_CORE_TRACE_TAG("Physics", "Body went to sleep");
}

// ============================================================
// Jolt global init helpers (thread-safe via static local)
// ============================================================

namespace {

// Jolt requires a one-time global registration of types and factory.
// We ref-count it so multiple JoltPhysicsWorld instances (unlikely but
// possible in tests) don't double-init.
struct JoltGlobalInit {
    static void acquire() {
        std::lock_guard lock(s_mutex);
        if (s_ref_count++ == 0) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    static void release() {
        std::lock_guard lock(s_mutex);
        if (--s_ref_count == 0) {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

private:
    static inline std::mutex s_mutex;
    static inline int s_ref_count = 0;
};

} // anonymous namespace

// ============================================================
// JoltPhysicsWorld
// ============================================================

JoltPhysicsWorld::JoltPhysicsWorld(const PhysicsConfig& config,
                                   uint32_t temp_allocator_size_mb)
    : m_config(config)
{
    JoltGlobalInit::acquire();

    // Create temp allocator (per-frame scratch memory for Jolt internals)
    m_temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(
        temp_allocator_size_mb * 1024 * 1024
    );

    // Create job system with hardware_concurrency - 1 worker threads
    const auto num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
    m_job_system = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        static_cast<int>(num_threads)
    );

    // Create the physics system
    m_physics_system = std::make_unique<JPH::PhysicsSystem>();
    m_physics_system->Init(
        MAX_BODIES,
        NUM_BODY_MUTEXES,
        MAX_BODY_PAIRS,
        MAX_CONTACT_CONSTRAINTS,
        m_broad_phase_layer_mapping,
        m_object_vs_broad_phase_filter,
        m_object_layer_pair_filter
    );

    // Set listeners
    m_physics_system->SetContactListener(&m_contact_listener);
    m_physics_system->SetBodyActivationListener(&m_activation_listener);

    // Apply gravity from config
    m_physics_system->SetGravity(jolt::to_jph(config.gravity));
}

JoltPhysicsWorld::~JoltPhysicsWorld() {
    // Destroy all bodies we track
    auto& bi = body_interface();
    for (auto& [handle, body_id] : m_handle_to_body) {
        bi.RemoveBody(body_id);
        bi.DestroyBody(body_id);
    }
    m_handle_to_body.clear();

    // Destroy Jolt objects in reverse creation order.
    // unique_ptrs handle this automatically, but we clear explicitly
    // to control order before JoltGlobalInit::release().
    m_physics_system.reset();
    m_job_system.reset();
    m_temp_allocator.reset();

    JoltGlobalInit::release();
}

JPH::BodyInterface& JoltPhysicsWorld::body_interface() {
    return m_physics_system->GetBodyInterface();
}

const JPH::BodyInterface& JoltPhysicsWorld::body_interface() const {
    return m_physics_system->GetBodyInterface();
}

// --- Body lifecycle ---

BodyHandle JoltPhysicsWorld::create_body(const BodyDesc& desc, uint64_t entity_id) {
    auto& bi = body_interface();

    // Create the shape from the variant
    JPH::ShapeRefC shape;

    if (auto* box = std::get_if<BoxShape>(&desc.shape)) {
        auto settings = JPH::BoxShapeSettings(jolt::to_jph(box->half_extents));
        auto result = settings.Create();
        assert(!result.HasError());
        shape = result.Get();
    } else if (auto* sphere = std::get_if<SphereShape>(&desc.shape)) {
        auto settings = JPH::SphereShapeSettings(sphere->radius);
        auto result = settings.Create();
        assert(!result.HasError());
        shape = result.Get();
    } else if (auto* capsule = std::get_if<CapsuleShape>(&desc.shape)) {
        auto settings = JPH::CapsuleShapeSettings(capsule->half_height, capsule->radius);
        auto result = settings.Create();
        assert(!result.HasError());
        shape = result.Get();
    }

    assert(shape != nullptr);

    // Determine object layer based on body type
    JPH::ObjectLayer layer = jolt::object_layer_for(desc.type);
    JPH::EMotionType motion_type = jolt::to_jph_motion_type(desc.type);

    // Create body
    JPH::BodyCreationSettings body_settings(
        shape,
        jolt::to_jph_rvec(desc.position),
        jolt::to_jph(desc.rotation),
        motion_type,
        layer
    );

    // Mass properties
    JPH::MassProperties mass_props;
    mass_props.ScaleToMass(desc.mass);
    body_settings.mMassPropertiesOverride = mass_props;
    body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;

    JPH::Body* body = bi.CreateBody(body_settings);
    assert(body != nullptr);

    // Store entity ID as user data (same pattern as existing code)
    body->SetUserData(entity_id);
    body->SetFriction(desc.friction);
    body->SetRestitution(desc.restitution);

    // Add to world (activate dynamic/kinematic, don't activate static)
    JPH::EActivation activation = (desc.type == BodyType::Static)
        ? JPH::EActivation::DontActivate
        : JPH::EActivation::Activate;
    bi.AddBody(body->GetID(), activation);

    // Broad phase needs re-optimization after new bodies
    m_broad_phase_optimized = false;

    // Use entity_id as the BodyHandle. If entity_id is 0, use the
    // Jolt BodyID index as a fallback handle.
    BodyHandle handle = (entity_id != 0)
        ? entity_id
        : static_cast<BodyHandle>(body->GetID().GetIndexAndSequenceNumber());
    m_handle_to_body[handle] = body->GetID();

    return handle;
}

void JoltPhysicsWorld::destroy_body(BodyHandle handle) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    auto& bi = body_interface();
    bi.RemoveBody(it->second);
    bi.DestroyBody(it->second);
    m_handle_to_body.erase(it);
}

// --- Transform ---

void JoltPhysicsWorld::set_transform(BodyHandle handle,
                                     const glm::vec3& pos,
                                     const glm::quat& rot) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().SetPositionAndRotation(
        it->second,
        jolt::to_jph_rvec(pos),
        jolt::to_jph(rot),
        JPH::EActivation::Activate
    );
}

glm::vec3 JoltPhysicsWorld::get_position(BodyHandle handle) const {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return glm::vec3(0.0f);

    return jolt::to_glm(body_interface().GetPosition(it->second));
}

glm::quat JoltPhysicsWorld::get_rotation(BodyHandle handle) const {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return glm::quat(1, 0, 0, 0);

    return jolt::to_glm(body_interface().GetRotation(it->second));
}

// --- Velocity / forces ---

void JoltPhysicsWorld::set_velocity(BodyHandle handle, const glm::vec3& linear) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().SetLinearVelocity(it->second, jolt::to_jph(linear));
}

void JoltPhysicsWorld::apply_force(BodyHandle handle, const glm::vec3& force) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().AddForce(it->second, jolt::to_jph(force));
}

void JoltPhysicsWorld::apply_impulse(BodyHandle handle, const glm::vec3& impulse) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().AddImpulse(it->second, jolt::to_jph(impulse));
}

// --- Simulation ---

void JoltPhysicsWorld::step(float dt) {
    // Optimize broad phase before first step (matches existing behavior)
    if (!m_broad_phase_optimized) {
        m_physics_system->OptimizeBroadPhase();
        m_broad_phase_optimized = true;
    }

    m_physics_system->Update(
        dt,
        m_config.collision_steps,
        m_temp_allocator.get(),
        m_job_system.get()
    );
}

std::vector<ContactEvent> JoltPhysicsWorld::drain_contacts() {
    return m_contact_listener.drain();
}

// --- Raycasting ---

std::optional<RayHit> JoltPhysicsWorld::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float max_distance) const
{
    JPH::RRayCast ray(
        jolt::to_jph_rvec(origin),
        jolt::to_jph(glm::normalize(direction) * max_distance)
    );

    JPH::RayCastResult result;
    bool hit = m_physics_system->GetNarrowPhaseQuery().CastRay(
        ray, result
    );

    if (!hit) return std::nullopt;

    // Resolve the hit body to get entity user data
    JPH::BodyLockRead lock(m_physics_system->GetBodyLockInterface(), result.mBodyID);
    if (!lock.Succeeded()) return std::nullopt;

    const JPH::Body& body = lock.GetBody();

    RayHit ray_hit;
    ray_hit.entity   = body.GetUserData();
    ray_hit.point    = jolt::to_glm(ray.GetPointOnRay(result.mFraction));
    ray_hit.normal   = glm::vec3(0.0f); // Normal requires shape query, simplified here
    ray_hit.distance = result.mFraction * max_distance;
    return ray_hit;
}

std::vector<RayHit> JoltPhysicsWorld::raycast_all(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float max_distance) const
{
    JPH::RRayCast ray(
        jolt::to_jph_rvec(origin),
        jolt::to_jph(glm::normalize(direction) * max_distance)
    );

    // Collect all hits
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    m_physics_system->GetNarrowPhaseQuery().CastRay(
        ray, JPH::RayCastSettings(), collector
    );

    // Sort by fraction (distance)
    collector.Sort();

    std::vector<RayHit> hits;
    hits.reserve(collector.mHits.size());

    for (auto& hit : collector.mHits) {
        JPH::BodyLockRead lock(m_physics_system->GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded()) continue;

        const JPH::Body& body = lock.GetBody();
        RayHit ray_hit;
        ray_hit.entity   = body.GetUserData();
        ray_hit.point    = jolt::to_glm(ray.GetPointOnRay(hit.mFraction));
        ray_hit.normal   = glm::vec3(0.0f);
        ray_hit.distance = hit.mFraction * max_distance;
        hits.push_back(ray_hit);
    }

    return hits;
}

// --- Configuration ---

void JoltPhysicsWorld::set_gravity(const glm::vec3& gravity) {
    m_physics_system->SetGravity(jolt::to_jph(gravity));
}

} // namespace helios::physics
```

- [ ] **Step 3: Verify compilation**

```bash
cmake --build build --target helios-physics 2>&1 | grep "error:" | head -10
```

Fix any missing includes or Jolt API mismatches. The Jolt vendored version may require adjustments to `CastRay` signatures -- check `Engine/vendor/JoltPhysics/JoltPhysics/Jolt/Physics/Collision/RayCast.h` for the exact API.

- [ ] **Step 4: Commit**

```bash
git add helios-physics/src/jolt/
git commit -m "feat(physics): implement JoltPhysicsWorld with RAII lifecycle, body management, raycasting"
```

---

## Phase 3: Physics ECS Integration

### Task 7: Define physics ECS components in helios-core

**Files:**
- Create: `helios-core/src/components/physics_components.h`

These components are plain aggregates defined in `helios-core` (not `helios-physics`) because game code queries them. They have no dependency on Jolt.

- [ ] **Step 1: Create physics_components.h**

```cpp
// helios-core/src/components/physics_components.h
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace helios {

// Opaque handle types (same typedefs as physics/audio interfaces, but
// repeated here so helios-core doesn't depend on helios-physics).
using BodyHandle = uint64_t;
using SoundHandle = uint64_t;

// Must match helios::physics::BodyType
enum class BodyType : uint8_t {
    Static,
    Dynamic,
    Kinematic,
};

// ---- Physics components (plain aggregates, no inheritance) ----

struct RigidBody {
    BodyType   type        = BodyType::Dynamic;
    float      mass        = 1.0f;
    float      friction    = 0.5f;
    float      restitution = 0.3f;
    BodyHandle body_handle {};  // Opaque handle into physics backend. Set by physics systems.
};

struct BoxCollider {
    glm::vec3 half_extents {0.5f};
    glm::vec3 offset       {0.0f};
};

struct SphereCollider {
    float     radius = 0.5f;
    glm::vec3 offset {0.0f};
};

// ---- Collision event (ECS event, not a component) ----
// Emitted by physics_step each frame after simulation.

struct CollisionEvent {
    uint64_t  entity_a    = 0;
    uint64_t  entity_b    = 0;
    glm::vec3 point       {0.0f};
    glm::vec3 normal      {0.0f};
    float     impulse     = 0.0f;
};

} // namespace helios
```

- [ ] **Step 2: Commit**

```bash
git add helios-core/src/components/physics_components.h
git commit -m "feat(core): add RigidBody, BoxCollider, SphereCollider components and CollisionEvent"
```

---

### Task 8: Create PhysicsPlugin with system registration

**Files:**
- Create: `helios-physics/src/interface/physics_plugin.h`

The plugin is a template parameterized on the backend. The Jolt backend is `JoltPhysicsWorld`. For testing, users can supply `MockPhysicsWorld`.

- [ ] **Step 1: Create physics_plugin.h**

```cpp
// helios-physics/src/interface/physics_plugin.h
#pragma once

#include <memory>
#include <type_traits>

#include "interface/physics_world.h"
// ECS types from helios-core (assumed by Plans 1-2)
// App, World, Res, ResMut, Query, EventWriter, EventReader, Schedule, Commands, Entity
// Transform, RigidBody, BoxCollider, SphereCollider, CollisionEvent from helios-core

namespace helios::physics {

// ============================================================
// Physics systems (free functions, registered by the plugin)
// ============================================================

// Schedule: FixedUpdate
// Step the physics simulation and emit collision events.
//
// void physics_step(
//     ResMut<std::unique_ptr<PhysicsWorld>> world,
//     Res<PhysicsConfig> config,
//     EventWriter<CollisionEvent> collisions
// ) {
//     world->get()->step(config->fixed_timestep);
//     for (auto& contact : world->get()->drain_contacts()) {
//         collisions.send(CollisionEvent{
//             .entity_a = contact.entity_a,
//             .entity_b = contact.entity_b,
//             .point    = contact.world_point,
//             .normal   = contact.normal,
//             .impulse  = contact.impulse,
//         });
//     }
// }
inline void physics_step(/*ResMut<std::unique_ptr<PhysicsWorld>>*/ PhysicsWorld& world,
                         const PhysicsConfig& config,
                         /* EventWriter<CollisionEvent>& collisions */) {
    // This is the system body. The actual parameter extraction is handled
    // by the ECS scheduler. The skeleton below shows the logic:
    //
    // 1. Step the simulation
    world.step(config.fixed_timestep);
    //
    // 2. Drain contacts and emit ECS events
    // auto contacts = world.drain_contacts();
    // for (auto& c : contacts) {
    //     collisions.send(CollisionEvent{c.entity_a, c.entity_b, c.world_point, c.normal, c.impulse});
    // }
}

// Schedule: PostUpdate
// Sync ECS Transform from physics simulation results (dynamic bodies).
//
// void sync_physics_transforms(
//     Query<Transform, const RigidBody> bodies,
//     Res<std::unique_ptr<PhysicsWorld>> world
// ) {
//     for (auto [transform, rb] : bodies) {
//         transform.position = world->get()->get_position(rb.body_handle);
//         transform.rotation = world->get()->get_rotation(rb.body_handle);
//     }
// }

// Schedule: PostUpdate
// Push ECS Transform into physics for kinematic bodies (user-driven movement).
//
// void push_kinematic_transforms(
//     Query<const Transform, const RigidBody> bodies,
//     ResMut<std::unique_ptr<PhysicsWorld>> world
// ) {
//     for (auto [transform, rb] : bodies) {
//         if (rb.type == BodyType::Kinematic) {
//             world->get()->set_transform(rb.body_handle, transform.position, transform.rotation);
//         }
//     }
// }

// ============================================================
// PhysicsPlugin
// ============================================================

// Backend concept: must derive from PhysicsWorld, be constructible
// from PhysicsConfig.
//
// Example backends:
//   JoltPhysicsWorld  -- production (Jolt Physics)
//   MockPhysicsWorld  -- testing (no external dependency)

template<typename Backend>
struct PhysicsPlugin {
    static_assert(std::is_base_of_v<PhysicsWorld, Backend>,
                  "Physics backend must derive from PhysicsWorld");

    PhysicsConfig config;  // User can customize before adding plugin

    // void build(App& app) {
    //     // 1. Insert physics config as a resource
    //     app.insert_resource<PhysicsConfig>(config);
    //
    //     // 2. Create the backend and insert as a resource
    //     auto world = std::make_unique<Backend>(config);
    //     app.insert_resource<std::unique_ptr<PhysicsWorld>>(std::move(world));
    //
    //     // 3. Register the CollisionEvent type
    //     app.add_event<CollisionEvent>();
    //
    //     // 4. Register systems in the correct schedules
    //     app.add_system(Schedule::FixedUpdate, physics_step);
    //     app.add_system(Schedule::PostUpdate, sync_physics_transforms);
    //     app.add_system(Schedule::PostUpdate, push_kinematic_transforms
    //                                            .after(sync_physics_transforms));
    // }

    // Full implementation below. The commented version above shows the
    // intended ECS API usage. The actual build() depends on the exact
    // App/World/Scheduler API from Plans 1-2.

    void build(auto& app) {
        // Insert physics configuration resource
        app.insert_resource(PhysicsConfig{config});

        // Create and insert the physics world backend
        auto world = std::make_unique<Backend>(config);
        app.template insert_resource<std::unique_ptr<PhysicsWorld>>(std::move(world));

        // Register collision event type
        // app.template add_event<CollisionEvent>();

        // Register systems
        // app.add_system(Schedule::FixedUpdate, physics_step);
        // app.add_system(Schedule::PostUpdate,  sync_physics_transforms);
        // app.add_system(Schedule::PostUpdate,  push_kinematic_transforms);
    }
};

// Convenience alias for production use
using JoltPhysicsPlugin = PhysicsPlugin<JoltPhysicsWorld>;

} // namespace helios::physics
```

**Note:** The system function signatures shown in comments use the ECS parameter types (`Query`, `Res`, `ResMut`, `EventWriter`) that will be finalized in Plans 1--2. The actual wiring in `build()` will use `app.add_system()` calls matching the scheduler API. The commented pseudo-code above and in the spec section 5.2 defines the exact intended signatures.

When Plans 1--2 are implemented, uncomment the `app.add_event<>()` and `app.add_system()` calls and replace the placeholder free functions with the real system functions that receive properly extracted ECS parameters.

- [ ] **Step 2: Commit**

```bash
git add helios-physics/src/interface/physics_plugin.h
git commit -m "feat(physics): add PhysicsPlugin template with system registration"
```

---

## Phase 4: Audio Interface and Types

### Task 9: Create helios-audio CMake target with directory skeleton

**Files:**
- Create: `helios-audio/CMakeLists.txt`
- Create: `helios-audio/src/interface/audio_types.h`
- Create: `helios-audio/src/interface/audio_device.h`
- Create: `helios-audio/src/interface/audio_plugin.h`
- Create: `helios-audio/src/soloud/soloud_device.h`
- Create: `helios-audio/src/soloud/soloud_device.cpp`
- Modify: root `CMakeLists.txt` (add `add_subdirectory(helios-audio)`)

- [ ] **Step 1: Create directory skeleton**

```bash
mkdir -p helios-audio/src/interface
mkdir -p helios-audio/src/soloud
```

- [ ] **Step 2: Create helios-audio/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)

add_library(helios-audio STATIC
    src/soloud/soloud_device.cpp
)

target_include_directories(helios-audio
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/soloud
)

# SoLoud vendored dependency
target_include_directories(helios-audio PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/soloud
)

target_link_libraries(helios-audio
    PUBLIC  helios-core
    PRIVATE SoLoud
)

# SoLoud uses miniaudio as its backend on all platforms
target_compile_definitions(helios-audio PRIVATE
    WITH_MINIAUDIO
)

target_compile_features(helios-audio PUBLIC cxx_std_20)
```

- [ ] **Step 3: Add to root CMakeLists.txt**

Add after the `helios-physics` subdirectory:

```cmake
add_subdirectory(helios-audio)
```

- [ ] **Step 4: Create placeholder source**

```bash
touch helios-audio/src/soloud/soloud_device.cpp
```

- [ ] **Step 5: Verify CMake configures**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5
```

- [ ] **Step 6: Commit**

```bash
git add helios-audio/
git commit -m "build: add helios-audio CMake target with directory skeleton"
```

---

### Task 10: Define audio types -- SoundHandle and PlayParams

**Files:**
- Create: `helios-audio/src/interface/audio_types.h`

- [ ] **Step 1: Create audio_types.h**

```cpp
// helios-audio/src/interface/audio_types.h
#pragma once

#include <cstdint>

namespace helios::audio {

// Opaque handle to a playing sound instance. Zero means no sound / invalid.
using SoundHandle = uint64_t;

// Parameters for playing a sound.
struct PlayParams {
    float volume = 1.0f;    // 0.0 = silent, 1.0 = full volume
    bool  loop   = false;   // If true, the sound loops indefinitely
    float pitch  = 1.0f;    // Playback speed multiplier (1.0 = normal)
};

} // namespace helios::audio
```

- [ ] **Step 2: Commit**

```bash
git add helios-audio/src/interface/audio_types.h
git commit -m "feat(audio): add SoundHandle and PlayParams types"
```

---

### Task 11: Define AudioDevice abstract interface

**Files:**
- Create: `helios-audio/src/interface/audio_device.h`

- [ ] **Step 1: Create audio_device.h**

```cpp
// helios-audio/src/interface/audio_device.h
#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "interface/audio_types.h"

namespace helios::audio {

// Abstract audio device interface.
//
// Implementations:
//   SoLoudDevice    -- production backend using SoLoud + miniaudio
//   MockAudioDevice -- stub for headless/test use
//
// Ownership: created by the plugin, stored as a World resource via
//   std::unique_ptr<AudioDevice>.
class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    // --- Playback ---

    // Play a sound from raw PCM data. Returns a handle to control the instance.
    virtual SoundHandle play(const void* pcm_data,
                             size_t size,
                             const PlayParams& params = {}) = 0;

    // Play a sound at a 3D world position (spatial audio).
    virtual SoundHandle play_at(const void* pcm_data,
                                size_t size,
                                const glm::vec3& position,
                                const PlayParams& params = {}) = 0;

    // Stop a playing sound immediately.
    virtual void stop(SoundHandle handle) = 0;

    // Pause a playing sound (can be resumed).
    virtual void pause(SoundHandle handle) = 0;

    // Resume a paused sound.
    virtual void resume(SoundHandle handle) = 0;

    // Check if a sound handle is currently playing.
    virtual bool is_playing(SoundHandle handle) const = 0;

    // --- Control ---

    // Set the volume of a playing sound (0.0 = silent, 1.0 = full).
    virtual void set_volume(SoundHandle handle, float volume) = 0;

    // Update the 3D position of a playing spatial sound.
    virtual void set_position(SoundHandle handle, const glm::vec3& pos) = 0;

    // Set the listener position and orientation for 3D audio.
    virtual void set_listener(const glm::vec3& position,
                              const glm::vec3& forward,
                              const glm::vec3& up) = 0;

    // --- Per-frame ---

    // Per-frame update. Processes stream buffers, updates 3D calculations.
    // Called once per frame from the audio update system.
    virtual void update() = 0;
};

} // namespace helios::audio
```

- [ ] **Step 2: Commit**

```bash
git add helios-audio/src/interface/audio_device.h
git commit -m "feat(audio): add AudioDevice abstract interface"
```

---

## Phase 5: SoLoud Backend Implementation

### Task 12: Implement SoLoudDevice

**Files:**
- Create: `helios-audio/src/soloud/soloud_device.h`
- Create: `helios-audio/src/soloud/soloud_device.cpp`

RAII: constructor calls `soloud.init()`, destructor calls `soloud.deinit()`. SoLoud uses `SoLoud::Soloud` as the main engine object and `SoLoud::Wav` for loaded audio. The device owns the SoLoud engine instance.

- [ ] **Step 1: Create soloud_device.h**

```cpp
// helios-audio/src/soloud/soloud_device.h
#pragma once

#include <memory>
#include <unordered_map>

#include "soloud.h"
#include "soloud_wav.h"

#include "interface/audio_device.h"

namespace helios::audio {

// Production AudioDevice implementation backed by SoLoud.
//
// RAII: constructor calls soloud.init(), destructor calls soloud.deinit().
// All SoLoud state is owned by this object. No global state.
class SoLoudDevice final : public AudioDevice {
public:
    SoLoudDevice();
    ~SoLoudDevice() override;

    // Non-copyable, non-movable (SoLoud engine is not relocatable)
    SoLoudDevice(const SoLoudDevice&) = delete;
    SoLoudDevice& operator=(const SoLoudDevice&) = delete;
    SoLoudDevice(SoLoudDevice&&) = delete;
    SoLoudDevice& operator=(SoLoudDevice&&) = delete;

    // --- AudioDevice interface ---

    SoundHandle play(const void* pcm_data, size_t size,
                     const PlayParams& params = {}) override;
    SoundHandle play_at(const void* pcm_data, size_t size,
                        const glm::vec3& position,
                        const PlayParams& params = {}) override;

    void stop(SoundHandle handle) override;
    void pause(SoundHandle handle) override;
    void resume(SoundHandle handle) override;
    bool is_playing(SoundHandle handle) const override;

    void set_volume(SoundHandle handle, float volume) override;
    void set_position(SoundHandle handle, const glm::vec3& pos) override;
    void set_listener(const glm::vec3& position,
                      const glm::vec3& forward,
                      const glm::vec3& up) override;

    void update() override;

private:
    SoLoud::Soloud m_soloud;

    // SoLoud::Wav objects need to stay alive while playing.
    // We keep them in a map keyed by SoLoud voice handle.
    // When a sound finishes, we clean up on the next update().
    struct ActiveSound {
        std::unique_ptr<SoLoud::Wav> wav;
        SoLoud::handle               voice_handle;
    };
    std::unordered_map<SoundHandle, ActiveSound> m_active_sounds;

    // Monotonically increasing handle counter
    SoundHandle m_next_handle = 1;
};

} // namespace helios::audio
```

- [ ] **Step 2: Create soloud_device.cpp**

```cpp
// helios-audio/src/soloud/soloud_device.cpp

#include "soloud/soloud_device.h"

namespace helios::audio {

SoLoudDevice::SoLoudDevice() {
    m_soloud.init();
}

SoLoudDevice::~SoLoudDevice() {
    m_soloud.stopAll();
    m_active_sounds.clear();
    m_soloud.deinit();
}

SoundHandle SoLoudDevice::play(const void* pcm_data, size_t size,
                                const PlayParams& params) {
    auto wav = std::make_unique<SoLoud::Wav>();
    wav->loadMem(
        static_cast<const unsigned char*>(pcm_data),
        static_cast<unsigned int>(size),
        /*copy=*/true,
        /*take_ownership=*/false
    );
    wav->setLooping(params.loop);

    SoLoud::handle voice = m_soloud.play(*wav, params.volume);
    m_soloud.setRelativePlaySpeed(voice, params.pitch);

    SoundHandle handle = m_next_handle++;
    m_active_sounds[handle] = ActiveSound{std::move(wav), voice};
    return handle;
}

SoundHandle SoLoudDevice::play_at(const void* pcm_data, size_t size,
                                   const glm::vec3& position,
                                   const PlayParams& params) {
    auto wav = std::make_unique<SoLoud::Wav>();
    wav->loadMem(
        static_cast<const unsigned char*>(pcm_data),
        static_cast<unsigned int>(size),
        /*copy=*/true,
        /*take_ownership=*/false
    );
    wav->setLooping(params.loop);

    SoLoud::handle voice = m_soloud.play3d(
        *wav,
        position.x, position.y, position.z,
        0.0f, 0.0f, 0.0f,  // velocity
        params.volume
    );
    m_soloud.setRelativePlaySpeed(voice, params.pitch);

    SoundHandle handle = m_next_handle++;
    m_active_sounds[handle] = ActiveSound{std::move(wav), voice};
    return handle;
}

void SoLoudDevice::stop(SoundHandle handle) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.stop(it->second.voice_handle);
    m_active_sounds.erase(it);
}

void SoLoudDevice::pause(SoundHandle handle) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.setPause(it->second.voice_handle, true);
}

void SoLoudDevice::resume(SoundHandle handle) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.setPause(it->second.voice_handle, false);
}

bool SoLoudDevice::is_playing(SoundHandle handle) const {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return false;

    return m_soloud.isValidVoiceHandle(it->second.voice_handle);
}

void SoLoudDevice::set_volume(SoundHandle handle, float volume) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.setVolume(it->second.voice_handle, volume);
}

void SoLoudDevice::set_position(SoundHandle handle, const glm::vec3& pos) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.set3dSourcePosition(
        it->second.voice_handle,
        pos.x, pos.y, pos.z
    );
}

void SoLoudDevice::set_listener(const glm::vec3& position,
                                 const glm::vec3& forward,
                                 const glm::vec3& up) {
    m_soloud.set3dListenerPosition(position.x, position.y, position.z);
    m_soloud.set3dListenerAt(forward.x, forward.y, forward.z);
    m_soloud.set3dListenerUp(up.x, up.y, up.z);
}

void SoLoudDevice::update() {
    // Update 3D audio calculations
    m_soloud.update3dAudio();

    // Clean up finished sounds
    std::vector<SoundHandle> finished;
    for (auto& [handle, sound] : m_active_sounds) {
        if (!m_soloud.isValidVoiceHandle(sound.voice_handle)) {
            finished.push_back(handle);
        }
    }
    for (auto handle : finished) {
        m_active_sounds.erase(handle);
    }
}

} // namespace helios::audio
```

- [ ] **Step 3: Verify compilation**

```bash
cmake --build build --target helios-audio 2>&1 | grep "error:" | head -10
```

SoLoud header paths may need adjustment. Check that `soloud.h` and `soloud_wav.h` are reachable from the include directory configured in CMake. The vendored SoLoud in this repo lives at `Engine/vendor/SoLoud/`. Adjust the CMake include path if needed:

```cmake
target_include_directories(helios-audio PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/vendor/SoLoud
)
```

- [ ] **Step 4: Commit**

```bash
git add helios-audio/src/soloud/
git commit -m "feat(audio): implement SoLoudDevice with RAII lifecycle and 3D audio"
```

---

## Phase 6: Audio ECS Integration

### Task 13: Define AudioSource component in helios-core and create AudioPlugin

**Files:**
- Create: `helios-core/src/components/audio_components.h`
- Create: `helios-audio/src/interface/audio_plugin.h`

- [ ] **Step 1: Create audio_components.h in helios-core**

```cpp
// helios-core/src/components/audio_components.h
#pragma once

#include <cstdint>

namespace helios {

// Forward-declared alias (same as audio::SoundHandle)
using SoundHandle = uint64_t;

// Forward-declared alias (same as AssetHandle from helios-core assets)
// struct AssetHandle { uint64_t id = 0; };

struct AudioSource {
    uint64_t    clip {};             // AssetHandle to the audio clip asset
    float       volume   = 1.0f;
    bool        loop     = false;
    bool        spatial  = false;    // If true, uses 3D positional audio
    SoundHandle playing_handle {};   // Opaque handle into audio backend (set by audio systems)
};

} // namespace helios
```

- [ ] **Step 2: Create audio_plugin.h**

```cpp
// helios-audio/src/interface/audio_plugin.h
#pragma once

#include <memory>
#include <type_traits>

#include "interface/audio_device.h"

namespace helios::audio {

// ============================================================
// Audio systems (free functions, registered by the plugin)
// ============================================================

// Schedule: PostUpdate
// Update the 3D listener position from the active camera's transform.
//
// void update_audio_listener(
//     Query<const Transform, With<ActiveCamera>> camera,
//     ResMut<std::unique_ptr<AudioDevice>> audio
// ) {
//     for (auto [transform] : camera) {
//         auto forward = transform.rotation * glm::vec3{0, 0, -1};
//         auto up = transform.rotation * glm::vec3{0, 1, 0};
//         audio->get()->set_listener(transform.position, forward, up);
//     }
// }

// Schedule: PostUpdate
// Update 3D positions of all spatial audio sources.
//
// void update_spatial_sources(
//     Query<const Transform, AudioSource> sources,
//     ResMut<std::unique_ptr<AudioDevice>> audio
// ) {
//     for (auto [transform, source] : sources) {
//         if (source.spatial && source.playing_handle) {
//             audio->get()->set_position(source.playing_handle, transform.position);
//         }
//     }
// }

// Schedule: PostUpdate
// Call AudioDevice::update() for per-frame processing (stream buffers, 3D recalculation).
//
// void audio_update(
//     ResMut<std::unique_ptr<AudioDevice>> audio
// ) {
//     audio->get()->update();
// }

// ============================================================
// AudioPlugin
// ============================================================

template<typename Backend>
struct AudioPlugin {
    static_assert(std::is_base_of_v<AudioDevice, Backend>,
                  "Audio backend must derive from AudioDevice");

    // void build(App& app) {
    //     auto device = std::make_unique<Backend>();
    //     app.insert_resource<std::unique_ptr<AudioDevice>>(std::move(device));
    //
    //     app.add_system(Schedule::PostUpdate, update_audio_listener);
    //     app.add_system(Schedule::PostUpdate, update_spatial_sources);
    //     app.add_system(Schedule::PostUpdate, audio_update
    //                                             .after(update_audio_listener)
    //                                             .after(update_spatial_sources));
    // }

    void build(auto& app) {
        auto device = std::make_unique<Backend>();
        app.template insert_resource<std::unique_ptr<AudioDevice>>(std::move(device));

        // Register audio systems (uncomment when ECS API from Plans 1-2 is ready):
        // app.add_system(Schedule::PostUpdate, update_audio_listener);
        // app.add_system(Schedule::PostUpdate, update_spatial_sources);
        // app.add_system(Schedule::PostUpdate, audio_update);
    }
};

// Convenience alias for production use
using SoLoudAudioPlugin = AudioPlugin<SoLoudDevice>;

} // namespace helios::audio
```

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/components/audio_components.h helios-audio/src/interface/audio_plugin.h
git commit -m "feat(audio): add AudioSource component and AudioPlugin template"
```

---

## Phase 7: Tests

### Task 14: Physics unit tests

**Files:**
- Create: `helios-physics/tests/test_jolt_physics_world.cpp`
- Modify: `helios-physics/CMakeLists.txt` (add test target)

These tests verify the JoltPhysicsWorld directly -- no ECS needed. They exercise body creation/destruction, simulation stepping, contact detection, and raycasting.

- [ ] **Step 1: Add test target to CMakeLists.txt**

Append to `helios-physics/CMakeLists.txt`:

```cmake
# --- Tests ---
if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(helios-physics-tests
        tests/test_jolt_physics_world.cpp
    )

    target_link_libraries(helios-physics-tests
        PRIVATE helios-physics
        PRIVATE GTest::gtest_main
    )

    target_include_directories(helios-physics-tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    include(GoogleTest)
    gtest_discover_tests(helios-physics-tests)
endif()
```

- [ ] **Step 2: Create test_jolt_physics_world.cpp**

```cpp
// helios-physics/tests/test_jolt_physics_world.cpp

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "interface/physics_world.h"
#include "jolt/jolt_physics_world.h"

using namespace helios::physics;

class JoltPhysicsWorldTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsConfig config;
        config.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        config.fixed_timestep = 1.0f / 60.0f;
        world = std::make_unique<JoltPhysicsWorld>(config);
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<JoltPhysicsWorld> world;
};

// --- Body creation / destruction ---

TEST_F(JoltPhysicsWorldTest, CreateBodyReturnsNonZeroHandle) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = BoxShape{glm::vec3(0.5f)};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, /*entity_id=*/42);
    EXPECT_NE(handle, 0u);
}

TEST_F(JoltPhysicsWorldTest, DestroyBodyDoesNotCrash) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = SphereShape{0.5f};

    BodyHandle handle = world->create_body(desc, 100);
    EXPECT_NO_THROW(world->destroy_body(handle));
}

TEST_F(JoltPhysicsWorldTest, DestroyInvalidHandleDoesNotCrash) {
    EXPECT_NO_THROW(world->destroy_body(999999));
}

TEST_F(JoltPhysicsWorldTest, GetPositionReturnsCreationPosition) {
    BodyDesc desc;
    desc.type = BodyType::Static;
    desc.position = glm::vec3(1.0f, 2.0f, 3.0f);
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle handle = world->create_body(desc, 10);

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.x, 1.0f, 0.01f);
    EXPECT_NEAR(pos.y, 2.0f, 0.01f);
    EXPECT_NEAR(pos.z, 3.0f, 0.01f);
}

// --- Simulation step ---

TEST_F(JoltPhysicsWorldTest, DynamicBodyFallsUnderGravity) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = SphereShape{0.5f};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, 1);

    // Step 60 times (1 second of simulation)
    for (int i = 0; i < 60; ++i) {
        world->step(1.0f / 60.0f);
    }

    glm::vec3 pos = world->get_position(handle);
    // After 1 second of gravity (-9.81), the body should have fallen
    // significantly below its starting height of 10.0
    EXPECT_LT(pos.y, 10.0f);
    EXPECT_LT(pos.y, 6.0f); // Rough check: ~0.5*g*t^2 = 4.9m drop
}

TEST_F(JoltPhysicsWorldTest, StaticBodyDoesNotMove) {
    BodyDesc desc;
    desc.type = BodyType::Static;
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle handle = world->create_body(desc, 2);

    for (int i = 0; i < 60; ++i) {
        world->step(1.0f / 60.0f);
    }

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.y, 5.0f, 0.01f);
}

// --- Contact detection ---

TEST_F(JoltPhysicsWorldTest, ContactDetectedBetweenCollidingBodies) {
    // Create a static floor
    BodyDesc floor_desc;
    floor_desc.type = BodyType::Static;
    floor_desc.position = glm::vec3(0.0f, 0.0f, 0.0f);
    floor_desc.shape = BoxShape{glm::vec3(50.0f, 0.5f, 50.0f)};
    world->create_body(floor_desc, 100);

    // Create a dynamic sphere above the floor
    BodyDesc sphere_desc;
    sphere_desc.type = BodyType::Dynamic;
    sphere_desc.position = glm::vec3(0.0f, 2.0f, 0.0f);
    sphere_desc.shape = SphereShape{0.5f};
    sphere_desc.mass = 1.0f;
    world->create_body(sphere_desc, 200);

    // Step until the sphere should have hit the floor
    bool contact_detected = false;
    for (int i = 0; i < 120; ++i) {
        world->step(1.0f / 60.0f);
        auto contacts = world->drain_contacts();
        if (!contacts.empty()) {
            contact_detected = true;
            // Verify the contact involves our two entities
            auto& c = contacts[0];
            bool involves_floor  = (c.entity_a == 100 || c.entity_b == 100);
            bool involves_sphere = (c.entity_a == 200 || c.entity_b == 200);
            EXPECT_TRUE(involves_floor);
            EXPECT_TRUE(involves_sphere);
            break;
        }
    }
    EXPECT_TRUE(contact_detected);
}

// --- Set transform / velocity ---

TEST_F(JoltPhysicsWorldTest, SetTransformUpdatesPosition) {
    BodyDesc desc;
    desc.type = BodyType::Kinematic;
    desc.position = glm::vec3(0.0f);
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle handle = world->create_body(desc, 50);
    world->set_transform(handle, glm::vec3(5.0f, 10.0f, 15.0f),
                         glm::quat(1, 0, 0, 0));

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.x, 5.0f, 0.01f);
    EXPECT_NEAR(pos.y, 10.0f, 0.01f);
    EXPECT_NEAR(pos.z, 15.0f, 0.01f);
}

TEST_F(JoltPhysicsWorldTest, ApplyImpulseChangesPosition) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = SphereShape{0.5f};
    desc.mass = 1.0f;

    // Zero gravity for this test
    world->set_gravity(glm::vec3(0.0f));

    BodyHandle handle = world->create_body(desc, 3);
    world->apply_impulse(handle, glm::vec3(10.0f, 0.0f, 0.0f));

    for (int i = 0; i < 60; ++i) {
        world->step(1.0f / 60.0f);
    }

    glm::vec3 pos = world->get_position(handle);
    EXPECT_GT(pos.x, 0.5f); // Should have moved in +X
}

// --- Multiple shape types ---

TEST_F(JoltPhysicsWorldTest, CapsuleShapeCreatesSuccessfully) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);
    desc.shape = CapsuleShape{0.5f, 0.25f};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, 77);
    EXPECT_NE(handle, 0u);

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.y, 5.0f, 0.01f);
}

// --- Raycast ---

TEST_F(JoltPhysicsWorldTest, RaycastHitsBody) {
    BodyDesc desc;
    desc.type = BodyType::Static;
    desc.position = glm::vec3(0.0f, 0.0f, -5.0f);
    desc.shape = BoxShape{glm::vec3(2.0f)};

    world->create_body(desc, 300);

    // Need at least one step for broad phase optimization
    world->step(1.0f / 60.0f);

    auto hit = world->raycast(
        glm::vec3(0.0f, 0.0f, 0.0f),    // origin
        glm::vec3(0.0f, 0.0f, -1.0f),    // direction
        100.0f                             // max distance
    );

    EXPECT_TRUE(hit.has_value());
    if (hit) {
        EXPECT_EQ(hit->entity, 300u);
        EXPECT_GT(hit->distance, 0.0f);
        EXPECT_LT(hit->distance, 10.0f);
    }
}

TEST_F(JoltPhysicsWorldTest, RaycastMissesReturnNullopt) {
    // No bodies in the world
    world->step(1.0f / 60.0f);

    auto hit = world->raycast(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        100.0f
    );

    EXPECT_FALSE(hit.has_value());
}
```

- [ ] **Step 3: Build and run tests**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --target helios-physics-tests
./build/helios-physics/helios-physics-tests
```

- [ ] **Step 4: Commit**

```bash
git add helios-physics/tests/ helios-physics/CMakeLists.txt
git commit -m "test(physics): add JoltPhysicsWorld unit tests (body lifecycle, simulation, contacts, raycast)"
```

---

### Task 15: Audio unit tests and plugin registration tests

**Files:**
- Create: `helios-audio/tests/test_soloud_device.cpp`
- Create: `helios-physics/tests/test_physics_plugin.cpp`
- Modify: `helios-audio/CMakeLists.txt` (add test target)

- [ ] **Step 1: Add test target to helios-audio/CMakeLists.txt**

Append to `helios-audio/CMakeLists.txt`:

```cmake
# --- Tests ---
if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(helios-audio-tests
        tests/test_soloud_device.cpp
    )

    target_link_libraries(helios-audio-tests
        PRIVATE helios-audio
        PRIVATE GTest::gtest_main
    )

    target_include_directories(helios-audio-tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    include(GoogleTest)
    gtest_discover_tests(helios-audio-tests)
endif()
```

- [ ] **Step 2: Create test_soloud_device.cpp**

```cpp
// helios-audio/tests/test_soloud_device.cpp

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

#include "interface/audio_device.h"
#include "soloud/soloud_device.h"

using namespace helios::audio;

class SoLoudDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<SoLoudDevice>();
    }

    void TearDown() override {
        device.reset();
    }

    // Generate a simple sine wave PCM buffer (16-bit, mono, 44100 Hz)
    static std::vector<uint8_t> generate_test_wav() {
        // Minimal WAV file header + 0.1 seconds of 440 Hz sine wave
        const int sample_rate = 44100;
        const int num_samples = sample_rate / 10; // 0.1 seconds
        const int bits_per_sample = 16;
        const int num_channels = 1;
        const int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
        const int block_align = num_channels * bits_per_sample / 8;
        const int data_size = num_samples * block_align;
        const int file_size = 44 + data_size; // WAV header is 44 bytes

        std::vector<uint8_t> buffer(file_size);

        // RIFF header
        buffer[0] = 'R'; buffer[1] = 'I'; buffer[2] = 'F'; buffer[3] = 'F';
        auto write_u32 = [&](int offset, uint32_t val) {
            buffer[offset]     = val & 0xFF;
            buffer[offset + 1] = (val >> 8) & 0xFF;
            buffer[offset + 2] = (val >> 16) & 0xFF;
            buffer[offset + 3] = (val >> 24) & 0xFF;
        };
        auto write_u16 = [&](int offset, uint16_t val) {
            buffer[offset]     = val & 0xFF;
            buffer[offset + 1] = (val >> 8) & 0xFF;
        };

        write_u32(4, file_size - 8);
        buffer[8] = 'W'; buffer[9] = 'A'; buffer[10] = 'V'; buffer[11] = 'E';

        // fmt chunk
        buffer[12] = 'f'; buffer[13] = 'm'; buffer[14] = 't'; buffer[15] = ' ';
        write_u32(16, 16); // chunk size
        write_u16(20, 1);  // PCM format
        write_u16(22, num_channels);
        write_u32(24, sample_rate);
        write_u32(28, byte_rate);
        write_u16(32, block_align);
        write_u16(34, bits_per_sample);

        // data chunk
        buffer[36] = 'd'; buffer[37] = 'a'; buffer[38] = 't'; buffer[39] = 'a';
        write_u32(40, data_size);

        // Generate sine wave samples
        for (int i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            double sample = std::sin(2.0 * M_PI * 440.0 * t) * 0.5;
            int16_t pcm_sample = static_cast<int16_t>(sample * 32767.0);
            int offset = 44 + i * 2;
            buffer[offset]     = pcm_sample & 0xFF;
            buffer[offset + 1] = (pcm_sample >> 8) & 0xFF;
        }

        return buffer;
    }

    std::unique_ptr<SoLoudDevice> device;
};

// --- Construction / destruction ---

TEST_F(SoLoudDeviceTest, ConstructionAndDestructionDoNotCrash) {
    // SetUp() and TearDown() exercise this
    EXPECT_NE(device, nullptr);
}

// --- Play / stop ---

TEST_F(SoLoudDeviceTest, PlayReturnsNonZeroHandle) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    EXPECT_NE(handle, 0u);
    device->stop(handle);
}

TEST_F(SoLoudDeviceTest, StopInvalidHandleDoesNotCrash) {
    EXPECT_NO_THROW(device->stop(999));
}

TEST_F(SoLoudDeviceTest, IsPlayingReturnsTrueForActiveSound) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    EXPECT_TRUE(device->is_playing(handle));
    device->stop(handle);
}

TEST_F(SoLoudDeviceTest, IsPlayingReturnsFalseAfterStop) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    device->stop(handle);
    EXPECT_FALSE(device->is_playing(handle));
}

// --- Pause / resume ---

TEST_F(SoLoudDeviceTest, PauseAndResumeDoNotCrash) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    EXPECT_NO_THROW(device->pause(handle));
    EXPECT_NO_THROW(device->resume(handle));
    device->stop(handle);
}

// --- 3D audio ---

TEST_F(SoLoudDeviceTest, PlayAtCreates3DSound) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play_at(
        wav_data.data(), wav_data.size(),
        glm::vec3(1.0f, 2.0f, 3.0f)
    );
    EXPECT_NE(handle, 0u);
    EXPECT_TRUE(device->is_playing(handle));
    device->stop(handle);
}

TEST_F(SoLoudDeviceTest, SetListenerDoesNotCrash) {
    EXPECT_NO_THROW(device->set_listener(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    ));
}

TEST_F(SoLoudDeviceTest, SetPositionOnActiveSound) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play_at(
        wav_data.data(), wav_data.size(),
        glm::vec3(0.0f)
    );
    EXPECT_NO_THROW(device->set_position(handle, glm::vec3(10.0f, 0.0f, 0.0f)));
    device->stop(handle);
}

// --- Update ---

TEST_F(SoLoudDeviceTest, UpdateDoesNotCrash) {
    EXPECT_NO_THROW(device->update());
}

TEST_F(SoLoudDeviceTest, UpdateCleansUpFinishedSounds) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    device->stop(handle);

    // After stop + update, the internal map should clean up
    device->update();
    EXPECT_FALSE(device->is_playing(handle));
}
```

- [ ] **Step 3: Create test_physics_plugin.cpp**

Tests plugin construction and resource insertion (does not require a full ECS -- uses a minimal mock App).

```cpp
// helios-physics/tests/test_physics_plugin.cpp

#include <gtest/gtest.h>
#include <memory>

#include "interface/physics_world.h"
#include "jolt/jolt_physics_world.h"

using namespace helios::physics;

// Minimal mock App for testing plugin build().
// Just records what resources and systems were registered.
struct MockApp {
    struct ResourceEntry {
        const void* ptr;
    };

    template<typename T>
    void insert_resource(T resource) {
        // Store the resource
        resources.push_back({});
        resource_count++;
        // If it's a unique_ptr to PhysicsWorld, verify it's not null
        if constexpr (std::is_same_v<T, std::unique_ptr<PhysicsWorld>>) {
            physics_world_inserted = (resource != nullptr);
            stored_world = std::move(resource);
        }
        if constexpr (std::is_same_v<T, PhysicsConfig>) {
            config_inserted = true;
            stored_config = resource;
        }
    }

    std::vector<ResourceEntry> resources;
    int resource_count = 0;
    bool physics_world_inserted = false;
    bool config_inserted = false;
    std::unique_ptr<PhysicsWorld> stored_world;
    PhysicsConfig stored_config;
};

TEST(PhysicsPlugin, BuildInsertsResources) {
    MockApp app;

    PhysicsPlugin<JoltPhysicsWorld> plugin;
    plugin.config.gravity = glm::vec3(0.0f, -10.0f, 0.0f);
    plugin.config.fixed_timestep = 1.0f / 120.0f;

    plugin.build(app);

    // Plugin should have inserted PhysicsConfig and PhysicsWorld
    EXPECT_TRUE(app.config_inserted);
    EXPECT_TRUE(app.physics_world_inserted);
    EXPECT_EQ(app.resource_count, 2);

    // Verify the config values were passed through
    EXPECT_NEAR(app.stored_config.fixed_timestep, 1.0f / 120.0f, 0.0001f);
    EXPECT_NEAR(app.stored_config.gravity.y, -10.0f, 0.01f);

    // Verify the world is functional
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = SphereShape{0.5f};
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);

    BodyHandle handle = app.stored_world->create_body(desc, 1);
    EXPECT_NE(handle, 0u);
}

TEST(PhysicsPlugin, MultipleWorldInstancesWork) {
    // Verify that two JoltPhysicsWorlds can coexist (ref-counted global init)
    PhysicsConfig config;
    auto world1 = std::make_unique<JoltPhysicsWorld>(config);
    auto world2 = std::make_unique<JoltPhysicsWorld>(config);

    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle h1 = world1->create_body(desc, 1);
    BodyHandle h2 = world2->create_body(desc, 2);

    EXPECT_NE(h1, 0u);
    EXPECT_NE(h2, 0u);

    // Destruction order should not matter
    world1.reset();
    world2.reset();
}
```

- [ ] **Step 4: Add plugin test to helios-physics CMakeLists.txt**

Update the test target sources list:

```cmake
add_executable(helios-physics-tests
    tests/test_jolt_physics_world.cpp
    tests/test_physics_plugin.cpp
)
```

- [ ] **Step 5: Build and run all tests**

```bash
cmake --build build --target helios-physics-tests helios-audio-tests
./build/helios-physics/helios-physics-tests
./build/helios-audio/helios-audio-tests
```

All tests should pass. If audio tests fail due to missing audio device (CI/headless), SoLoud with miniaudio should fall back to a null driver. If it doesn't, add a `SoLoud::Soloud::NULLDRIVER` backend flag in the `SoLoudDevice` constructor for headless environments.

- [ ] **Step 6: Commit**

```bash
git add helios-physics/tests/ helios-physics/CMakeLists.txt \
        helios-audio/tests/ helios-audio/CMakeLists.txt
git commit -m "test: add physics plugin and audio device unit tests"
```

---

## Summary of files created/modified

### New files (helios-physics/)
| File | Purpose |
|------|---------|
| `helios-physics/CMakeLists.txt` | CMake target for static library + tests |
| `helios-physics/src/interface/body_types.h` | `BodyHandle`, `BodyType`, `ColliderShape`, `BodyDesc` |
| `helios-physics/src/interface/contact_event.h` | `ContactEvent`, `RayHit` |
| `helios-physics/src/interface/physics_world.h` | `PhysicsWorld` abstract interface, `PhysicsConfig` |
| `helios-physics/src/interface/physics_plugin.h` | `PhysicsPlugin<Backend>` template, system declarations |
| `helios-physics/src/jolt/jolt_utils.h` | glm-Jolt conversions, layer filters |
| `helios-physics/src/jolt/jolt_physics_world.h` | `JoltPhysicsWorld` header |
| `helios-physics/src/jolt/jolt_physics_world.cpp` | `JoltPhysicsWorld` implementation |
| `helios-physics/tests/test_jolt_physics_world.cpp` | Body lifecycle, simulation, contact, raycast tests |
| `helios-physics/tests/test_physics_plugin.cpp` | Plugin resource insertion tests |

### New files (helios-audio/)
| File | Purpose |
|------|---------|
| `helios-audio/CMakeLists.txt` | CMake target for static library + tests |
| `helios-audio/src/interface/audio_types.h` | `SoundHandle`, `PlayParams` |
| `helios-audio/src/interface/audio_device.h` | `AudioDevice` abstract interface |
| `helios-audio/src/interface/audio_plugin.h` | `AudioPlugin<Backend>` template, system declarations |
| `helios-audio/src/soloud/soloud_device.h` | `SoLoudDevice` header |
| `helios-audio/src/soloud/soloud_device.cpp` | `SoLoudDevice` implementation |
| `helios-audio/tests/test_soloud_device.cpp` | Play/stop, 3D audio, lifecycle tests |

### New files (helios-core/)
| File | Purpose |
|------|---------|
| `helios-core/src/components/physics_components.h` | `RigidBody`, `BoxCollider`, `SphereCollider`, `CollisionEvent` |
| `helios-core/src/components/audio_components.h` | `AudioSource` |

### Modified files
| File | Change |
|------|--------|
| Root `CMakeLists.txt` | Add `add_subdirectory(helios-physics)` and `add_subdirectory(helios-audio)` |

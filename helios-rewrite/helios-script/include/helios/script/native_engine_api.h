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
    // -- World context --------------------------------------------------------
    /// Opaque pointer to the World. Stored by C# and passed back
    /// on every native call. Set by ScriptGlue::fill().
    void* world_context = nullptr;

    // -- Logging --------------------------------------------------------------
    void (*Log)(void* world_ctx, int level, const char* message)                          = nullptr;

    // -- Entity operations ----------------------------------------------------
    /// Spawn a new entity. Returns its raw ID (generation + index packed).
    uint64_t (*Spawn)(void* world_ctx)                                                    = nullptr;
    /// Despawn an entity by ID (deferred via Commands).
    void (*Despawn)(void* world_ctx, uint64_t entity_id)                                  = nullptr;
    /// Returns true if the entity is still alive.
    bool (*IsAlive)(void* world_ctx, uint64_t entity_id)                                  = nullptr;

    // -- Component access (generic, string-based) -----------------------------
    /// Returns true if entity has the named component.
    bool (*HasComponent)(void* world_ctx, uint64_t entity_id, const char* component_name) = nullptr;

    // -- Transform (convenience shortcuts) ------------------------------------
    void (*TransformGetTranslation)(void* world_ctx, uint64_t entity_id, float* out_xyz)  = nullptr;
    void (*TransformSetTranslation)(void* world_ctx, uint64_t entity_id, float* in_xyz)   = nullptr;
    void (*TransformGetRotation)(void* world_ctx, uint64_t entity_id, float* out_xyz)     = nullptr;
    void (*TransformSetRotation)(void* world_ctx, uint64_t entity_id, float* in_xyz)      = nullptr;
    void (*TransformGetScale)(void* world_ctx, uint64_t entity_id, float* out_xyz)        = nullptr;
    void (*TransformSetScale)(void* world_ctx, uint64_t entity_id, float* in_xyz)         = nullptr;

    // -- Input ----------------------------------------------------------------
    bool (*IsKeyPressed)(void* world_ctx, int keycode)                                    = nullptr;
    bool (*IsMouseButtonPressed)(void* world_ctx, int button)                             = nullptr;
    void (*GetMousePosition)(void* world_ctx, float* out_x, float* out_y)                = nullptr;

    // -- Physics --------------------------------------------------------------
    void (*PhysicsApplyForce)(void* world_ctx, uint64_t entity_id, float* force_xyz)      = nullptr;
    void (*PhysicsApplyImpulse)(void* world_ctx, uint64_t entity_id, float* impulse_xyz)  = nullptr;
    void (*PhysicsApplyTorque)(void* world_ctx, uint64_t entity_id, float* torque_xyz)    = nullptr;
    void (*PhysicsSetLinearVelocity)(void* world_ctx, uint64_t entity_id, float* vel_xyz) = nullptr;
    void (*PhysicsGetLinearVelocity)(void* world_ctx, uint64_t entity_id, float* out_xyz) = nullptr;
    void (*PhysicsSetAngularVelocity)(void* world_ctx, uint64_t entity_id, float* vel_xyz)= nullptr;
    void (*PhysicsGetAngularVelocity)(void* world_ctx, uint64_t entity_id, float* out_xyz)= nullptr;
};

} // namespace helios

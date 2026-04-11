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

    // -- Mouse ----------------------------------------------------------------
    void (*GetMouseDelta)(void* world_ctx, float* out_dx, float* out_dy)                 = nullptr;
    float (*GetScrollDelta)(void* world_ctx)                                              = nullptr;

    // -- Cursor mode (0 = normal, 1 = captured/hidden) ------------------------
    void (*SetCursorMode)(void* world_ctx, int mode)                                     = nullptr;
    int  (*GetCursorMode)(void* world_ctx)                                                = nullptr;

    // -- Physics: teleport (set_transform + zero velocity in one call) --------
    void (*PhysicsTeleport)(void* world_ctx, uint64_t entity_id,
                            float* pos_xyz, float* rot_xyzw)                              = nullptr;

    // -- Tag name lookup ------------------------------------------------------
    void (*GetTagName)(void* world_ctx, uint64_t entity_id,
                       char* out_buffer, int buffer_size)                                  = nullptr;

    // -- Time -----------------------------------------------------------------
    float (*GetDeltaTime)(void* world_ctx)                                                = nullptr;

    // -- Input: single-press detection ----------------------------------------
    bool (*IsKeyJustPressed)(void* world_ctx, int keycode)                                = nullptr;

    // -- Transform: look-at (computes rotation to face a target) -------------
    void (*TransformLookAt)(void* world_ctx, uint64_t entity_id,
                            float* target_xyz, float* up_xyz)                             = nullptr;

    // -- Assets ---------------------------------------------------------------
    /// Async load by path. Returns packed AssetHandle (0 on failure).
    uint64_t (*AssetLoad)(void* world_ctx, const char* path)                              = nullptr;
    /// Check if a loaded asset is ready.
    bool (*AssetIsLoaded)(void* world_ctx, uint64_t handle)                               = nullptr;

    // -- Audio ----------------------------------------------------------------
    uint32_t (*AudioGetSoundId)(void* world_ctx, const char* name)                        = nullptr;
    void (*AudioPlaySoundAt)(void* world_ctx, uint32_t sound_id,
                             float x, float y, float z)                                   = nullptr;
    void (*AudioPlayFile)(void* world_ctx, const char* path,
                          float x, float y, float z, float volume, bool loop)             = nullptr;
    /// Play from a pre-loaded asset handle.
    void (*AudioPlayHandle)(void* world_ctx, uint64_t handle,
                            float x, float y, float z, float volume, bool loop)           = nullptr;

    // -- Scene ----------------------------------------------------------------
    void (*SceneLoad)(void* world_ctx, const char* scene_path)                            = nullptr;
    void (*SceneInstantiate)(void* world_ctx, const char* scene_path)                     = nullptr;

    /// Preload a scene's assets without switching. Returns true if all ready.
    bool (*SceneIsReady)(void* world_ctx, const char* scene_path)                         = nullptr;
};

} // namespace helios

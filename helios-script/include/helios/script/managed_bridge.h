#pragma once

#include <cstdint>

namespace helios {

/// Function pointers received FROM the C# managed bridge.
/// C++ calls these to control script instances.
/// All pointer types are blittable -- no managed types, no C++ objects.
/// Layout must exactly match the C# ManagedBridgeNative struct.
struct ManagedBridge {
    // -- Assembly management --------------------------------------------------
    void (*LoadAppAssembly)(const char* path)                              = nullptr;
    void (*UnloadAppAssembly)()                                            = nullptr;

    // -- Class discovery ------------------------------------------------------
    int  (*GetEntityClassCount)()                                          = nullptr;
    void (*GetEntityClassName)(int index, char* buffer, int buffer_size)   = nullptr;
    bool (*EntityClassExists)(const char* full_name)                       = nullptr;
    /// Returns a stable hash for the class name (used as script_type_id).
    uint32_t (*GetScriptTypeId)(const char* full_name)                     = nullptr;

    // -- Instance lifecycle ---------------------------------------------------
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

    // -- Method invocation (generic) ------------------------------------------
    /// Invoke a named method on a specific entity's script instance.
    /// Used for collision callbacks, custom events, etc.
    void (*InvokeMethod)(uint64_t entity_id,
                         const char* method_name,
                         const void* args,
                         int args_size_bytes)                              = nullptr;

    // -- Collision callbacks ---------------------------------------------------
    /// Invoke OnCollisionEnter on the script instance for entity_id.
    /// px/py/pz: contact point, nx/ny/nz: normal, impulse: magnitude.
    void (*InvokeOnCollision)(uint64_t entity_id,
                              uint64_t other_entity_id,
                              float px, float py, float pz,
                              float nx, float ny, float nz,
                              float impulse)                               = nullptr;
};

} // namespace helios

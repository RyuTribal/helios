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

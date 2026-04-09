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

    // -- Assembly management --------------------------------------------------

    /// Load a game assembly (.dll) from disk.
    /// Returns true on success.
    virtual bool load_assembly(const std::filesystem::path& assembly_path) = 0;

    /// Unload the currently loaded game assembly.
    /// All script instances must be destroyed first.
    virtual void unload_assembly() = 0;

    /// Reload the game assembly. Internally: unload -> load.
    /// Caller is responsible for serializing/deserializing script state.
    virtual bool reload_assembly(const std::filesystem::path& assembly_path) = 0;

    // -- Class discovery ------------------------------------------------------

    /// Returns true if the given fully-qualified class name exists
    /// as a Script subclass in the loaded assembly.
    virtual bool class_exists(const std::string& full_class_name) const = 0;

    /// Returns all Script subclass names found in the loaded assembly.
    virtual std::vector<std::string> get_script_class_names() const = 0;

    /// Returns a numeric type ID for a script class name.
    /// The same name always produces the same ID within a single
    /// assembly load (but IDs may change across reloads).
    virtual uint32_t get_script_type_id(const std::string& full_class_name) const = 0;

    // -- Instance lifecycle ---------------------------------------------------

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

    // -- Hot reload support ---------------------------------------------------

    /// Request a reload on the next check_script_reload tick.
    virtual void request_reload() = 0;

    /// Returns true if a reload has been requested.
    virtual bool reload_requested() const = 0;

    /// Clear the reload request flag (called after reload completes).
    virtual void clear_reload_request() = 0;
};

} // namespace helios

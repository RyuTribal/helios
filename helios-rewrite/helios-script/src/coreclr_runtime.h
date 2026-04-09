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

    // -- ScriptRuntime interface ----------------------------------------------

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
    void invoke_on_collision(uint64_t entity_id,
                             uint64_t other_entity_id,
                             float px, float py, float pz,
                             float nx, float ny, float nz,
                             float impulse) override;

    void request_reload() override;
    bool reload_requested() const override;
    void clear_reload_request() override;

private:
    // -- HostFXR handles ------------------------------------------------------
    void* m_hostfxr_handle = nullptr;           // dlopen handle to libhostfxr.so
    hostfxr_handle m_host_context = nullptr;     // runtime context handle

    // -- HostFXR function pointers --------------------------------------------
    void* m_close_fn = nullptr;
    load_assembly_and_get_function_pointer_fn m_load_assembly_fn = nullptr;

    // -- Bridge structs -------------------------------------------------------
    ManagedBridge m_bridge{};
    NativeEngineAPI m_native_api{};

    // -- State ----------------------------------------------------------------
    std::filesystem::path m_app_assembly_path;
    std::atomic<bool> m_reload_requested{false};

    // -- Internal helpers -----------------------------------------------------
    bool load_hostfxr();
    void* get_managed_fn(const std::filesystem::path& assembly_path,
                         const char* type_name,
                         const char* method_name);
};

} // namespace helios

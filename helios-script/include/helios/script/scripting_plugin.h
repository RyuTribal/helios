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

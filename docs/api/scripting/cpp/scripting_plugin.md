# ScriptingPlugin

The `ScriptingPlugin` is the entry point for integrating C# scripting into a Helios application. It initializes the CoreCLR runtime, loads assemblies, and registers the ECS systems for script execution.

## Configuration

The plugin uses `ScriptingPluginConfig` for setup:

```cpp
struct ScriptingPluginConfig {
    std::filesystem::path runtime_config_path;   // Path to ScriptCore.runtimeconfig.json
    std::filesystem::path script_core_dll_path;  // Path to ScriptCore.dll
    std::filesystem::path app_assembly_path;     // Path to the game's script assembly (.dll)
    std::filesystem::path watch_directory;       // Directory for hot reload (optional)
};
```

## Usage

Add the plugin to your `App` during initialization:

```cpp
ScriptingPluginConfig config;
config.runtime_config_path = "bin/ScriptCore.runtimeconfig.json";
config.script_core_dll_path = "bin/ScriptCore.dll";
config.app_assembly_path = "bin/GameScripts.dll";
config.watch_directory = "scripts/";

app.add_plugin(ScriptingPlugin{config});
```

## Systems Registered

The plugin registers the following systems to the world:

- `check_script_reload` (`PreUpdate`): Monitors file changes and triggers hot reload.
- `script_create_system` (`Update`): Instantiates managed script objects for entities with `ScriptInstance`.
- `script_execution_system` (`Update`): Calls `OnUpdate` on all active script instances.
- `script_collision_dispatch` (`Update`): Passes physics collision events to scripts.
- `script_destroy_system` (`PostUpdate`): Cleans up managed handles when entities are destroyed.

## Related Pages

- [ScriptRuntime](script_runtime.md)
- [Components & Resources](components.md)
- [C# Scripting Overview](../csharp/index.md)

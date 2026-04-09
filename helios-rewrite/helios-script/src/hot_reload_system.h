// helios-script/src/hot_reload_system.h
#pragma once

#include <filesystem>

namespace helios {

class World;

/// ECS system: checks if a script reload has been requested.
/// If so, serializes script instance state, reloads the assembly,
/// and re-creates instances with the saved state.
/// Runs in Schedule::PreUpdate.
void check_script_reload(World& world);

/// Simple resource to store the path to the game assembly.
/// Inserted by ScriptingPlugin.
struct ScriptAssemblyPath {
    std::filesystem::path path;
};

} // namespace helios

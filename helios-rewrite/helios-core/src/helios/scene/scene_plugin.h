#pragma once

#include "helios/scene/scene_manager.h"
#include "helios/scene/scene_data.h"
#include "helios/assets/asset_server.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace helios {

// Forward declarations
class App;
class World;

/// A component factory parses a YAML node at scene-load time and returns a
/// ComponentAdder that applies the component to an entity at spawn time.
///
/// Two-phase design:
///   1. Parse: reads YAML, captures data (runs once when scene file is loaded)
///   2. Apply: adds component to entity (runs each time the scene is spawned)
///
/// This lets MeshRenderer capture a mesh *path* from YAML, then resolve it
/// to a Handle<MeshAsset> at spawn time via AssetServer.
using ComponentFactory = std::function<
    ComponentAdder(const YAML::Node&)
>;

/// Plugin that registers SceneManager as a world resource and provides
/// component factories for all engine components.
///
/// Usage:
///   app.add_plugin(SceneManagerPlugin{});
struct SceneManagerPlugin {
    void build(App& app);

    /// Register a custom component factory. Must be called before build().
    void register_factory(const std::string& name, ComponentFactory factory);

    /// Initialize built-in factories without requiring an App.
    /// Useful for tests that call load_scene_from_yaml directly.
    void build_factories_for_test() { register_builtin_factories(); }

    /// Access the factory map (for use with load_scene_from_yaml).
    const std::unordered_map<std::string, ComponentFactory>& factories_for_test() const {
        return m_factories;
    }

private:
    /// Register all built-in engine component factories.
    void register_builtin_factories();

    std::unordered_map<std::string, ComponentFactory> m_factories;
};

/// Parse a YAML scene file and create blueprints in the SceneManager.
/// Called by SceneManager::load() after the plugin has registered factories.
///
/// Scene YAML format:
///   scene:
///     name: "village"
///     assets:
///       - "meshes/house.gltf"
///     entities:
///       - name: "house_1"
///         components:
///           Transform: { position: [10, 0, 5], ... }
///           MeshRenderer: { mesh: "meshes/house.gltf" }
void load_scene_from_yaml(
    SceneManager& scenes,
    SceneHandle handle,
    const std::filesystem::path& path,
    const std::unordered_map<std::string, ComponentFactory>& factories);

} // namespace helios

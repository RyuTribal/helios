#pragma once

#include "helios/scene/scene_handle.h"
#include "helios/scene/scene_data.h"
#include "helios/ecs/entity.h"
#include "helios/ecs/world.h"
#include "helios/ecs/asset_handle.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

// Forward declarations
class AssetServer;

/// Manages runtime scenes: groups of entities that can be spawned/despawned
/// as a unit. Supports both programmatic scene building and (Phase 3) YAML
/// file loading.
///
/// Usage:
///   SceneManager scenes;
///   auto h = scenes.create("village");
///   scenes.add_entity(h, "house", Transform{...}, MeshRenderer{mesh});
///   scenes.spawn(h, world, server);
///   // later...
///   scenes.despawn(h, world);
///   scenes.unload(h);
class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;

    // Non-copyable (holds scene state)
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // Movable
    SceneManager(SceneManager&&) = default;
    SceneManager& operator=(SceneManager&&) = default;

    // --- Scene lifecycle ---

    /// Create a new empty scene. Returns a handle for further operations.
    SceneHandle create(const std::string& name);

    /// Add an entity blueprint with the given components.
    /// Components are captured by value and replayed at spawn time.
    template<typename... Ts>
    void add_entity(SceneHandle handle, const std::string& name, Ts&&... components);

    /// Add an entity blueprint with a custom component adder function.
    /// Used for components that need AssetServer access at spawn time.
    void add_entity_fn(SceneHandle handle, const std::string& name,
                       ComponentAdder adder);

    /// Register an asset path to be preloaded when preload() is called.
    void add_asset(SceneHandle handle, const std::string& path);

    /// Preload all registered asset paths. Assets are loaded via
    /// AssetServer (async by default). Call is_preloaded() to check.
    void preload(SceneHandle handle, AssetServer& server);

    /// Check if all preloaded assets are ready.
    bool is_preloaded(SceneHandle handle, const AssetServer& server) const;

    /// Spawn all entity blueprints into the world. Each entity gets a
    /// SceneTag component for identification.
    void spawn(SceneHandle handle, World& world, AssetServer& server);

    /// Despawn all entities belonging to this scene (reverse order).
    /// Releases acquired asset handles.
    void despawn(SceneHandle handle, World& world);

    /// Free all scene data (blueprints, asset handles, etc.).
    /// The handle becomes invalid.
    void unload(SceneHandle handle);

    // --- Queries ---

    /// Get the current state of a scene.
    SceneState state(SceneHandle handle) const;

    /// Get the name of a scene.
    const std::string& name(SceneHandle handle) const;

    /// Get the list of spawned entities for a scene.
    const std::vector<Entity>& spawned_entities(SceneHandle handle) const;

    /// Check if a scene handle is valid.
    bool is_valid(SceneHandle handle) const;

private:
    SceneData* find(SceneHandle handle);
    const SceneData* find(SceneHandle handle) const;

    std::unordered_map<uint64_t, SceneData> m_scenes;
    uint32_t m_next_index = 1;
};

// --- Template implementations ---

template<typename... Ts>
void SceneManager::add_entity(SceneHandle handle, const std::string& name,
                               Ts&&... components) {
    auto* scene = find(handle);
    if (!scene) return;

    EntityBlueprint bp;
    bp.name = name;

    // Capture each component by value in its own adder lambda.
    // The fold expression creates one adder per component.
    (bp.adders.push_back(
        [comp = std::forward<Ts>(components)](
            World& world, Entity entity, AssetServer& /*server*/) mutable {
            world.add(entity, std::move(comp));
        }
    ), ...);

    scene->blueprints.push_back(std::move(bp));
}

} // namespace helios

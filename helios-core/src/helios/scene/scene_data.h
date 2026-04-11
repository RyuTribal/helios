#pragma once

#include "helios/scene/scene_handle.h"
#include "helios/ecs/entity.h"
#include "helios/ecs/asset_handle.h"

#include <functional>
#include <string>
#include <vector>

namespace helios {

// Forward declarations
class World;
class AssetServer;

/// State of a scene in its lifecycle.
enum class SceneState : uint8_t {
    Created,    // Just created, blueprints may be added
    Preloading, // Assets being loaded
    Ready,      // Assets loaded, ready to spawn
    Spawned,    // Entities have been instantiated in the world
    Unloaded    // Scene data freed (handle can be recycled)
};

/// Type-erased function that adds components to an entity.
/// Captures all component data at add_entity() time, replays at spawn().
using ComponentAdder = std::function<void(World&, Entity, AssetServer&)>;

/// Blueprint for a single entity within a scene.
struct EntityBlueprint {
    std::string name;
    std::vector<ComponentAdder> adders;
};

/// Internal data for a managed scene.
struct SceneData {
    std::string name;
    SceneHandle handle;
    SceneState state = SceneState::Created;

    // Entity blueprints (templates for spawning)
    std::vector<EntityBlueprint> blueprints;

    // Asset paths to preload
    std::vector<std::string> asset_paths;

    // Spawned entities (tracked for despawn)
    std::vector<Entity> spawned_entities;

    // Raw asset handles acquired during preload/spawn (for release on despawn)
    std::vector<AssetHandle> acquired_assets;
};

} // namespace helios

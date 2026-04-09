#include "helios/scene/scene_manager.h"
#include "helios/ecs/world.h"
#include "helios/assets/asset_server.h"
#include "helios/core/engine_log_channels.h"

#include <algorithm>
#include <stdexcept>

namespace helios {

SceneHandle SceneManager::create(const std::string& name) {
    SceneHandle handle{m_next_index++, 1};

    SceneData data;
    data.name = name;
    data.handle = handle;
    data.state = SceneState::Created;

    m_scenes.emplace(handle.packed(), std::move(data));

    HELIOS_LOG(Assets, Info, "SceneManager: created scene '{}' (handle {}/{})",
               name, handle.index, handle.generation);
    return handle;
}

void SceneManager::add_entity_fn(SceneHandle handle, const std::string& name,
                                  ComponentAdder adder) {
    auto* scene = find(handle);
    if (!scene) return;

    EntityBlueprint bp;
    bp.name = name;
    bp.adders.push_back(std::move(adder));
    scene->blueprints.push_back(std::move(bp));
}

void SceneManager::add_asset(SceneHandle handle, const std::string& path) {
    auto* scene = find(handle);
    if (!scene) return;
    scene->asset_paths.push_back(path);
}

void SceneManager::preload(SceneHandle handle, AssetServer& server) {
    auto* scene = find(handle);
    if (!scene) return;

    scene->state = SceneState::Preloading;

    for (const auto& path : scene->asset_paths) {
        auto asset_handle = server.load_by_extension(path);
        if (asset_handle) {
            scene->acquired_assets.push_back(asset_handle);
        }
    }

    HELIOS_LOG(Assets, Info, "SceneManager: preloading {} assets for scene '{}'",
               scene->asset_paths.size(), scene->name);
}

bool SceneManager::is_preloaded(SceneHandle handle, const AssetServer& server) const {
    const auto* scene = find(handle);
    if (!scene) return false;

    for (const auto& asset : scene->acquired_assets) {
        auto s = server.status(asset);
        if (s == AssetStatus::Loading) return false;
    }

    return true;
}

void SceneManager::spawn(SceneHandle handle, World& world, AssetServer& server) {
    auto* scene = find(handle);
    if (!scene) return;

    HELIOS_LOG(Assets, Info, "SceneManager: spawning {} entities for scene '{}'",
               scene->blueprints.size(), scene->name);

    for (const auto& blueprint : scene->blueprints) {
        Entity entity = world.spawn();
        world.add(entity, SceneTag{.scene = handle});

        for (const auto& adder : blueprint.adders) {
            adder(world, entity, server);
        }

        scene->spawned_entities.push_back(entity);
    }

    scene->state = SceneState::Spawned;

    HELIOS_LOG(Assets, Info, "SceneManager: scene '{}' spawned ({} entities)",
               scene->name, scene->spawned_entities.size());
}

void SceneManager::despawn(SceneHandle handle, World& world) {
    auto* scene = find(handle);
    if (!scene) return;

    HELIOS_LOG(Assets, Info, "SceneManager: despawning scene '{}' ({} entities)",
               scene->name, scene->spawned_entities.size());

    // Despawn in reverse order to minimize swap-remove overhead
    for (auto it = scene->spawned_entities.rbegin();
         it != scene->spawned_entities.rend(); ++it) {
        if (world.is_alive(*it)) {
            world.despawn(*it);
        }
    }
    scene->spawned_entities.clear();

    scene->state = SceneState::Created;
}

void SceneManager::unload(SceneHandle handle) {
    auto it = m_scenes.find(handle.packed());
    if (it == m_scenes.end()) return;

    HELIOS_LOG(Assets, Info, "SceneManager: unloading scene '{}'",
               it->second.name);

    it->second.state = SceneState::Unloaded;
    m_scenes.erase(it);
}

SceneState SceneManager::state(SceneHandle handle) const {
    const auto* scene = find(handle);
    if (!scene) return SceneState::Unloaded;
    return scene->state;
}

const std::string& SceneManager::name(SceneHandle handle) const {
    const auto* scene = find(handle);
    if (!scene) {
        static const std::string empty;
        return empty;
    }
    return scene->name;
}

const std::vector<Entity>& SceneManager::spawned_entities(SceneHandle handle) const {
    const auto* scene = find(handle);
    if (!scene) {
        static const std::vector<Entity> empty;
        return empty;
    }
    return scene->spawned_entities;
}

bool SceneManager::is_valid(SceneHandle handle) const {
    return find(handle) != nullptr;
}

SceneData* SceneManager::find(SceneHandle handle) {
    auto it = m_scenes.find(handle.packed());
    if (it == m_scenes.end()) return nullptr;
    return &it->second;
}

const SceneData* SceneManager::find(SceneHandle handle) const {
    auto it = m_scenes.find(handle.packed());
    if (it == m_scenes.end()) return nullptr;
    return &it->second;
}

} // namespace helios

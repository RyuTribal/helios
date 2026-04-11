#pragma once

#include "helios/ecs/world.h"
#include "helios/ecs/entity.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

/// Serializes and deserializes a World's entities and components to/from YAML.
///
/// Component types must be registered before save/load:
///   SceneSerializer serializer;
///   serializer.register_component<Transform>("Transform", ...);
///   serializer.save(world, "scene.yaml");
class SceneSerializer {
public:
    /// A type-erased serializer for a single component on one entity.
    /// Returns true if the entity has this component and it was written.
    using SerializeFn   = std::function<bool(const World& world, Entity entity,
                                             YAML::Emitter& out)>;

    /// A type-erased deserializer that adds a component to an entity from YAML.
    using DeserializeFn = std::function<void(World& world, Entity entity,
                                             const YAML::Node& node)>;

    /// Register a component type with explicit serialize/deserialize callbacks.
    void register_component(const std::string& name,
                            SerializeFn serialize_fn,
                            DeserializeFn deserialize_fn);

    /// Convenience: register a component type with lambdas that call
    /// the yaml_serializer.h free functions.
    template <typename T>
    void register_component(
        const std::string& name,
        std::function<void(YAML::Emitter&, const T&)> ser,
        std::function<T(const YAML::Node&)> deser);

    /// Save all entities and their registered components to a YAML file.
    void save(const World& world, const std::filesystem::path& path) const;

    /// Save a specific set of entities (and their children recursively) to a YAML file.
    void save_entities(const World& world, const std::vector<Entity>& roots,
                       const std::filesystem::path& path) const;

    /// Load entities from a YAML file into the world.
    void load(World& world, const std::filesystem::path& path) const;

    /// Full scene load: deserialize entities, find/create SceneRoot,
    /// parent orphans under it. Returns the SceneRoot entity.
    Entity load_scene(World& world, const std::filesystem::path& path) const;

    /// Serialize all entities to a YAML string (for in-memory snapshot).
    std::string save_to_string(const World& world) const;

    /// Load entities from a YAML string into the world.
    void load_from_string(World& world, const std::string& yaml_str) const;

    /// Full scene restore from string: deserialize, find/create SceneRoot,
    /// parent orphans. Returns the SceneRoot entity.
    Entity load_scene_from_string(World& world, const std::string& yaml_str) const;

private:
    struct ComponentEntry {
        std::string     name;
        SerializeFn     serialize;
        DeserializeFn   deserialize;
    };

    std::vector<ComponentEntry> m_components;
    std::unordered_map<std::string, size_t> m_name_index;

    void serialize_entity(const World& world, Entity entity, YAML::Emitter& out) const;
};

// ============================================================================
// Template implementation
// ============================================================================

template <typename T>
void SceneSerializer::register_component(
    const std::string& name,
    std::function<void(YAML::Emitter&, const T&)> ser,
    std::function<T(const YAML::Node&)> deser)
{
    register_component(
        name,
        // SerializeFn
        [ser](const World& world, Entity entity, YAML::Emitter& out) -> bool {
            auto* comp = world.try_get<T>(entity);
            if (!comp) return false;
            ser(out, *comp);
            return true;
        },
        // DeserializeFn
        [deser](World& world, Entity entity, const YAML::Node& node) {
            T comp = deser(node);
            world.add<T>(entity, std::move(comp));
        }
    );
}

} // namespace helios

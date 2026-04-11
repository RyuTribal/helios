#include "helios/serialization/scene_serializer.h"
#include "helios/components/components.h"
#include "helios/core/engine_log_channels.h"

#include <fstream>

namespace helios {

void SceneSerializer::register_component(const std::string& name,
                                         SerializeFn serialize_fn,
                                         DeserializeFn deserialize_fn) {
    size_t idx = m_components.size();
    m_components.push_back(ComponentEntry{name, std::move(serialize_fn),
                                          std::move(deserialize_fn)});
    m_name_index[name] = idx;
}

void SceneSerializer::save(const World& world,
                           const std::filesystem::path& path) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "scene" << YAML::Value;
    out << YAML::BeginMap;
    out << YAML::Key << "entities" << YAML::Value;
    out << YAML::BeginSeq;

    world.archetypes().for_each_archetype(
        [&](const Archetype& arch) {
            for (size_t row = 0; row < arch.size(); ++row) {
                serialize_entity(world, arch.entities[row], out);
            }
        });

    out << YAML::EndSeq;
    out << YAML::EndMap;
    out << YAML::EndMap;

    std::ofstream file(path);
    if (!file.is_open()) {
        HELIOS_LOG(Assets, Error,
            "SceneSerializer::save: cannot open file {}", path.string());
        return;
    }
    file << out.c_str();
}

// Helper: serialize a single entity to the YAML emitter.
void SceneSerializer::serialize_entity(const World& world, Entity entity,
                                        YAML::Emitter& out) const {
    const Tag* tag = world.try_get<Tag>(entity);
    if (world.has<EditorOnly>(entity)) return;

    out << YAML::BeginMap;

    if (tag && !tag->name.empty()) {
        out << YAML::Key << "name" << YAML::Value << tag->name;
    } else {
        out << YAML::Key << "name" << YAML::Value
            << ("entity_" + std::to_string(entity.index));
    }

    out << YAML::Key << "components" << YAML::Value;
    out << YAML::BeginMap;
    for (const auto& entry : m_components) {
        if (entry.name == "Tag") continue;
        YAML::Emitter temp;
        bool has = entry.serialize(world, entity, temp);
        if (has) {
            out << YAML::Key << entry.name << YAML::Value
                << YAML::Load(temp.c_str());
        }
    }
    out << YAML::EndMap;
    out << YAML::EndMap;
}

// Collect entity + all children recursively.
// Stops at child SceneRoot entities — they save to their own files.
// The SceneRoot entity itself IS included (as a reference), but not its children.
static void collect_hierarchy(const World& world, Entity root, std::vector<Entity>& out) {
    out.push_back(root);
    const Children* ch = world.try_get<Children>(root);
    if (ch) {
        for (auto child : ch->entities) {
            if (!world.is_alive(child)) continue;
            if (world.has<SceneRoot>(child)) {
                out.push_back(child); // include as reference only
                continue;
            }
            collect_hierarchy(world, child, out);
        }
    }
}

void SceneSerializer::save_entities(const World& world, const std::vector<Entity>& roots,
                                     const std::filesystem::path& path) const {
    // Collect all entities (roots + their children recursively)
    std::vector<Entity> all;
    for (auto root : roots) {
        collect_hierarchy(world, root, all);
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "scene" << YAML::Value;
    out << YAML::BeginMap;
    out << YAML::Key << "entities" << YAML::Value;
    out << YAML::BeginSeq;

    for (auto entity : all) {
        serialize_entity(world, entity, out);
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;
    out << YAML::EndMap;

    std::ofstream file(path);
    if (!file.is_open()) {
        HELIOS_LOG(Assets, Error,
            "SceneSerializer::save_entities: cannot open file {}", path.string());
        return;
    }
    file << out.c_str();
}

void SceneSerializer::load(World& world,
                           const std::filesystem::path& path) const {
    YAML::Node root = YAML::LoadFile(path.string());
    if (!root["scene"]) {
        HELIOS_LOG(Assets, Error,
            "SceneSerializer::load: missing 'scene' root in {}", path.string());
        return;
    }

    auto scene_node = root["scene"];
    if (!scene_node["entities"]) return;

    for (const auto& entity_node : scene_node["entities"]) {
        // Spawn a bare entity.
        Entity entity = world.spawn();

        // If a name is provided, add a Tag component.
        if (entity_node["name"]) {
            std::string name = entity_node["name"].as<std::string>();
            world.add<Tag>(entity, Tag{std::move(name)});
        }

        if (!entity_node["components"]) continue;

        auto components_node = entity_node["components"];
        for (auto it = components_node.begin(); it != components_node.end(); ++it) {
            std::string comp_name = it->first.as<std::string>();
            auto comp_it = m_name_index.find(comp_name);
            if (comp_it == m_name_index.end()) continue; // unknown component, skip

            const auto& entry = m_components[comp_it->second];
            entry.deserialize(world, entity, it->second);
        }
    }
}

std::string SceneSerializer::save_to_string(const World& world) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "scene" << YAML::Value;
    out << YAML::BeginMap;
    out << YAML::Key << "entities" << YAML::Value;
    out << YAML::BeginSeq;

    world.archetypes().for_each_archetype(
        [&](const Archetype& arch) {
            for (size_t row = 0; row < arch.size(); ++row) {
                serialize_entity(world, arch.entities[row], out);
            }
        });

    out << YAML::EndSeq;
    out << YAML::EndMap;
    out << YAML::EndMap;

    return out.c_str();
}

void SceneSerializer::load_from_string(World& world, const std::string& yaml_str) const {
    YAML::Node root = YAML::Load(yaml_str);
    if (!root["scene"]) return;

    auto scene_node = root["scene"];
    if (!scene_node["entities"]) return;

    for (const auto& entity_node : scene_node["entities"]) {
        Entity entity = world.spawn();

        if (entity_node["name"]) {
            std::string name = entity_node["name"].as<std::string>();
            world.add<Tag>(entity, Tag{std::move(name)});
        }

        if (!entity_node["components"]) continue;

        auto components_node = entity_node["components"];
        for (auto it = components_node.begin(); it != components_node.end(); ++it) {
            std::string comp_name = it->first.as<std::string>();
            auto comp_it = m_name_index.find(comp_name);
            if (comp_it == m_name_index.end()) continue;

            const auto& entry = m_components[comp_it->second];
            entry.deserialize(world, entity, it->second);
        }
    }
}

// Common post-load: find SceneRoot, parent orphans under it.
static Entity finalize_scene_load(World& world) {
    Entity scene_root{};
    auto sq = world.query<const SceneRoot>();
    for (auto [e, sr] : sq.with_entity()) {
        scene_root = e;
        break;
    }

    if (!world.is_alive(scene_root)) return scene_root;

    auto orphans = world.query<const Tag, Without<Parent>>();
    for (auto [e, tag] : orphans.with_entity()) {
        if (e == scene_root) continue;
        if (world.has<EditorOnly>(e)) continue;
        if (world.has<SceneRoot>(e)) continue;
        world.add(e, Parent{.entity = scene_root});
        if (auto* ch = world.try_get<Children>(scene_root)) {
            ch->entities.push_back(e);
        } else {
            world.add(scene_root, Children{.entities = {e}});
        }
    }

    return scene_root;
}

Entity SceneSerializer::load_scene(World& world,
                                    const std::filesystem::path& path) const {
    load(world, path);
    return finalize_scene_load(world);
}

Entity SceneSerializer::load_scene_from_string(World& world,
                                                const std::string& yaml_str) const {
    load_from_string(world, yaml_str);
    return finalize_scene_load(world);
}

} // namespace helios

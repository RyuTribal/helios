#include "helios/serialization/scene_serializer.h"
#include "helios/components/components.h"

#include <fstream>
#include <stdexcept>

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

    // Iterate all archetypes and their entities.
    const_cast<World&>(world).archetypes().for_each_archetype(
        [&](Archetype& arch) {
            for (size_t row = 0; row < arch.size(); ++row) {
                Entity entity = arch.entities[row];

                out << YAML::BeginMap;

                // Use Tag component name if present; otherwise generate one.
                const Tag* tag = world.try_get<Tag>(entity);
                if (tag && !tag->name.empty()) {
                    out << YAML::Key << "name" << YAML::Value << tag->name;
                } else {
                    out << YAML::Key << "name" << YAML::Value
                        << ("entity_" + std::to_string(entity.index));
                }

                out << YAML::Key << "components" << YAML::Value;
                out << YAML::BeginMap;

                for (const auto& entry : m_components) {
                    // Skip Tag -- it is already written as the entity name.
                    if (entry.name == "Tag") continue;

                    // Try to serialize. The function returns false if the
                    // entity doesn't have this component.
                    std::string component_name = entry.name;
                    // We need to write key before trying, so buffer it.
                    YAML::Emitter temp;
                    bool has = entry.serialize(world, entity, temp);
                    if (has) {
                        out << YAML::Key << component_name << YAML::Value
                            << YAML::Load(temp.c_str());
                    }
                }

                out << YAML::EndMap; // components
                out << YAML::EndMap; // entity
            }
        });

    out << YAML::EndSeq;  // entities
    out << YAML::EndMap;  // scene
    out << YAML::EndMap;  // root

    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(
            "SceneSerializer::save: cannot open file " + path.string());
    }
    file << out.c_str();
}

void SceneSerializer::load(World& world,
                           const std::filesystem::path& path) const {
    YAML::Node root = YAML::LoadFile(path.string());
    if (!root["scene"]) {
        throw std::runtime_error(
            "SceneSerializer::load: missing 'scene' root in " + path.string());
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

} // namespace helios

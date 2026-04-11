#include "helios/scene/scene_plugin.h"

#include "helios/ecs/app.h"
#include "helios/ecs/world.h"
#include "helios/assets/asset_server.h"
#include "helios/assets/mesh_asset.h"
#include "helios/components/components.h"
#include "helios/serialization/yaml_serializer.h"
#include "helios/core/engine_log_channels.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace helios {

// ============================================================================
// Built-in component factories
// ============================================================================

void SceneManagerPlugin::register_builtin_factories() {

    // --- Transform ---
    m_factories["Transform"] = [](const YAML::Node& node) -> ComponentAdder {
        Transform t = deserialize_yaml_transform(node);
        return [t](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, t);
        };
    };

    // --- Camera ---
    m_factories["Camera"] = [](const YAML::Node& node) -> ComponentAdder {
        Camera c = deserialize_yaml_camera(node);
        return [c](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, c);
        };
    };

    // --- ActiveCamera ---
    m_factories["ActiveCamera"] = [](const YAML::Node& node) -> ComponentAdder {
        ActiveCamera ac = deserialize_yaml_active_camera(node);
        return [ac](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, ac);
        };
    };

    // --- PointLight ---
    m_factories["PointLight"] = [](const YAML::Node& node) -> ComponentAdder {
        PointLight pl = deserialize_yaml_point_light(node);
        return [pl](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, pl);
        };
    };

    // --- DirectionalLight ---
    m_factories["DirectionalLight"] = [](const YAML::Node& node) -> ComponentAdder {
        DirectionalLight dl = deserialize_yaml_directional_light(node);
        return [dl](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, dl);
        };
    };

    // --- MeshRenderer ---
    // Special: captures mesh path from YAML, resolves to Handle<MeshAsset> at
    // spawn time via AssetServer.
    m_factories["MeshRenderer"] = [](const YAML::Node& node) -> ComponentAdder {
        std::string mesh_path;
        if (node["mesh"] && node["mesh"].IsScalar()) {
            mesh_path = node["mesh"].as<std::string>();
        }

        uint32_t flags = MeshFlags::CastShadows | MeshFlags::ReceiveShadows;
        if (node["flags"]) {
            flags = node["flags"].as<uint32_t>();
        }

        return [mesh_path, flags](World& w, Entity e, AssetServer& s) {
            MeshRenderer mr;
            if (!mesh_path.empty()) {
                mr.mesh = s.load<MeshAsset>(mesh_path);
            }
            mr.flags = flags;
            w.add(e, std::move(mr));
        };
    };

    // --- RigidBody ---
    m_factories["RigidBody"] = [](const YAML::Node& node) -> ComponentAdder {
        RigidBody rb = deserialize_yaml_rigid_body(node);
        return [rb](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, rb);
        };
    };

    // --- BoxCollider ---
    m_factories["BoxCollider"] = [](const YAML::Node& node) -> ComponentAdder {
        BoxCollider bc = deserialize_yaml_box_collider(node);
        return [bc](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, bc);
        };
    };

    // --- SphereCollider ---
    m_factories["SphereCollider"] = [](const YAML::Node& node) -> ComponentAdder {
        SphereCollider sc = deserialize_yaml_sphere_collider(node);
        return [sc](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, sc);
        };
    };

    // --- AudioSource ---
    m_factories["AudioSource"] = [](const YAML::Node& node) -> ComponentAdder {
        AudioSource as = deserialize_yaml_audio_source(node);
        return [as](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, as);
        };
    };

    // --- Tag ---
    m_factories["Tag"] = [](const YAML::Node& node) -> ComponentAdder {
        Tag tag = deserialize_yaml_tag(node);
        return [tag](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, tag);
        };
    };

    // --- Disabled ---
    m_factories["Disabled"] = [](const YAML::Node& node) -> ComponentAdder {
        Disabled d = deserialize_yaml_disabled(node);
        return [d](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, d);
        };
    };
}

// ============================================================================
// Plugin build
// ============================================================================

void SceneManagerPlugin::build(App& app) {
    register_builtin_factories();

    // Insert SceneManager as a world resource.
    app.world().insert_resource<SceneManager>(SceneManager{});

    HELIOS_LOG(Assets, Info, "SceneManagerPlugin: registered {} component factories",
               m_factories.size());
}

void SceneManagerPlugin::register_factory(const std::string& name,
                                           ComponentFactory factory) {
    m_factories[name] = std::move(factory);
}

// ============================================================================
// YAML scene loader
// ============================================================================

void load_scene_from_yaml(
    SceneManager& scenes,
    SceneHandle handle,
    const std::filesystem::path& path,
    const std::unordered_map<std::string, ComponentFactory>& factories)
{
    YAML::Node root = YAML::LoadFile(path.string());
    if (!root["scene"]) {
        HELIOS_LOG(Assets, Error,
            "load_scene_from_yaml: missing 'scene' root in {}", path.string());
        return;
    }

    auto scene_node = root["scene"];

    // Parse asset paths for preloading
    if (scene_node["assets"]) {
        for (const auto& asset_node : scene_node["assets"]) {
            scenes.add_asset(handle, asset_node.as<std::string>());
        }
    }

    // Parse entity blueprints
    if (scene_node["entities"]) {
        for (const auto& entity_node : scene_node["entities"]) {
            std::string entity_name = "unnamed";
            if (entity_node["name"]) {
                entity_name = entity_node["name"].as<std::string>();
            }

            // Collect all component adders for this entity
            std::vector<ComponentAdder> adders;

            // If entity has a name, add a Tag component
            if (entity_node["name"]) {
                std::string tag_name = entity_name;
                adders.push_back(
                    [tag_name](World& w, Entity e, AssetServer& /*s*/) {
                        w.add(e, Tag{.name = tag_name});
                    });
            }

            if (entity_node["components"]) {
                auto components_node = entity_node["components"];
                for (auto it = components_node.begin();
                     it != components_node.end(); ++it)
                {
                    std::string comp_name = it->first.as<std::string>();
                    auto factory_it = factories.find(comp_name);
                    if (factory_it == factories.end()) {
                        HELIOS_LOG(Assets, Warn,
                            "load_scene_from_yaml: unknown component '{}' "
                            "on entity '{}', skipping",
                            comp_name, entity_name);
                        continue;
                    }

                    // Parse phase: create the adder from YAML
                    auto adder = factory_it->second(it->second);
                    adders.push_back(std::move(adder));
                }
            }

            // Bundle all adders into a single entity blueprint
            scenes.add_entity_fn(handle, entity_name,
                [captured_adders = std::move(adders)](
                    World& w, Entity e, AssetServer& s) {
                    for (const auto& adder : captured_adders) {
                        adder(w, e, s);
                    }
                });
        }
    }

    HELIOS_LOG(Assets, Info, "load_scene_from_yaml: loaded scene from '{}'",
               path.string());
}

} // namespace helios

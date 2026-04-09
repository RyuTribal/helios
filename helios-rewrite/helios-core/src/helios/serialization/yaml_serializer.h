#pragma once

#include "helios/ecs/asset_handle.h"
#include "helios/components/components.h"
#include "helios/components/component_enums.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <string>
#include <type_traits>

namespace helios {

// ============================================================================
// YAML serialize helpers  (write to YAML::Emitter)
// ============================================================================

// --- Scalars ---

inline void serialize_yaml(YAML::Emitter& out, float v) {
    out << v;
}

inline void serialize_yaml(YAML::Emitter& out, double v) {
    out << v;
}

inline void serialize_yaml(YAML::Emitter& out, int v) {
    out << v;
}

inline void serialize_yaml(YAML::Emitter& out, uint32_t v) {
    out << v;
}

inline void serialize_yaml(YAML::Emitter& out, uint64_t v) {
    out << v;
}

inline void serialize_yaml(YAML::Emitter& out, bool v) {
    out << v;
}

inline void serialize_yaml(YAML::Emitter& out, const std::string& v) {
    out << v;
}

// --- Enums ---

inline void serialize_yaml(YAML::Emitter& out, AssetStatus v) {
    switch (v) {
        case AssetStatus::Loading: out << "Loading"; break;
        case AssetStatus::Loaded:  out << "Loaded";  break;
        case AssetStatus::Failed:  out << "Failed";  break;
    }
}

inline void serialize_yaml(YAML::Emitter& out, ProjectionType v) {
    switch (v) {
        case ProjectionType::Perspective:  out << "Perspective";  break;
        case ProjectionType::Orthographic: out << "Orthographic"; break;
    }
}

inline void serialize_yaml(YAML::Emitter& out, BodyType v) {
    switch (v) {
        case BodyType::Static:    out << "Static";    break;
        case BodyType::Kinematic: out << "Kinematic"; break;
        case BodyType::Dynamic:   out << "Dynamic";   break;
    }
}

// --- AssetHandle ---

inline void serialize_yaml(YAML::Emitter& out, const AssetHandle& h) {
    out << YAML::BeginMap;
    out << YAML::Key << "index"      << YAML::Value << h.index;
    out << YAML::Key << "generation" << YAML::Value << h.generation;
    out << YAML::EndMap;
}

/// Serialize a typed Handle<T> (same wire format as AssetHandle).
template<typename T>
inline void serialize_yaml(YAML::Emitter& out, const Handle<T>& h) {
    serialize_yaml(out, h.untyped());
}

// --- glm types ---

inline void serialize_yaml(YAML::Emitter& out, const glm::vec2& v) {
    out << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
}

inline void serialize_yaml(YAML::Emitter& out, const glm::vec3& v) {
    out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
}

inline void serialize_yaml(YAML::Emitter& out, const glm::vec4& v) {
    out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
}

inline void serialize_yaml(YAML::Emitter& out, const glm::quat& q) {
    out << YAML::Flow << YAML::BeginSeq << q.w << q.x << q.y << q.z << YAML::EndSeq;
}

inline void serialize_yaml(YAML::Emitter& out, const glm::mat4& m) {
    out << YAML::BeginSeq;
    for (int col = 0; col < 4; ++col) {
        out << YAML::Flow << YAML::BeginSeq;
        for (int row = 0; row < 4; ++row) {
            out << m[col][row];
        }
        out << YAML::EndSeq;
    }
    out << YAML::EndSeq;
}

// --- Component structs ---

inline void serialize_yaml(YAML::Emitter& out, const Transform& t) {
    out << YAML::BeginMap;
    out << YAML::Key << "position" << YAML::Value; serialize_yaml(out, t.position);
    out << YAML::Key << "rotation" << YAML::Value; serialize_yaml(out, t.rotation);
    out << YAML::Key << "scale"    << YAML::Value; serialize_yaml(out, t.scale);
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const GlobalTransform& g) {
    out << YAML::BeginMap;
    out << YAML::Key << "matrix" << YAML::Value; serialize_yaml(out, g.matrix);
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const MeshRenderer& mr) {
    out << YAML::BeginMap;
    out << YAML::Key << "mesh"     << YAML::Value; serialize_yaml(out, mr.mesh);
    out << YAML::Key << "material" << YAML::Value; serialize_yaml(out, mr.material);
    out << YAML::Key << "flags"    << YAML::Value << mr.flags;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const PointLight& pl) {
    out << YAML::BeginMap;
    out << YAML::Key << "color"     << YAML::Value; serialize_yaml(out, pl.color);
    out << YAML::Key << "intensity" << YAML::Value << pl.intensity;
    out << YAML::Key << "radius"    << YAML::Value << pl.radius;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const DirectionalLight& dl) {
    out << YAML::BeginMap;
    out << YAML::Key << "color"     << YAML::Value; serialize_yaml(out, dl.color);
    out << YAML::Key << "intensity" << YAML::Value << dl.intensity;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const Camera& c) {
    out << YAML::BeginMap;
    out << YAML::Key << "projection" << YAML::Value; serialize_yaml(out, c.projection);
    out << YAML::Key << "fov_degrees" << YAML::Value << c.fov_degrees;
    out << YAML::Key << "near_plane"  << YAML::Value << c.near_plane;
    out << YAML::Key << "far_plane"   << YAML::Value << c.far_plane;
    out << YAML::Key << "ortho_size"  << YAML::Value << c.ortho_size;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const ActiveCamera&) {
    out << YAML::BeginMap << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const RigidBody& rb) {
    out << YAML::BeginMap;
    out << YAML::Key << "body_type"    << YAML::Value; serialize_yaml(out, rb.body_type);
    out << YAML::Key << "mass"         << YAML::Value << rb.mass;
    out << YAML::Key << "friction"     << YAML::Value << rb.friction;
    out << YAML::Key << "restitution"  << YAML::Value << rb.restitution;
    out << YAML::Key << "flags"        << YAML::Value << rb.flags;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const BoxCollider& bc) {
    out << YAML::BeginMap;
    out << YAML::Key << "half_extents" << YAML::Value; serialize_yaml(out, bc.half_extents);
    out << YAML::Key << "offset"       << YAML::Value; serialize_yaml(out, bc.offset);
    out << YAML::Key << "flags"        << YAML::Value << bc.flags;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const SphereCollider& sc) {
    out << YAML::BeginMap;
    out << YAML::Key << "radius" << YAML::Value << sc.radius;
    out << YAML::Key << "offset" << YAML::Value; serialize_yaml(out, sc.offset);
    out << YAML::Key << "flags"  << YAML::Value << sc.flags;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const AudioSource& as) {
    out << YAML::BeginMap;
    out << YAML::Key << "volume" << YAML::Value << as.volume;
    out << YAML::Key << "pitch"  << YAML::Value << as.pitch;
    out << YAML::Key << "flags"  << YAML::Value << as.flags;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const Tag& tag) {
    out << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << tag.name;
    out << YAML::EndMap;
}

inline void serialize_yaml(YAML::Emitter& out, const Disabled&) {
    out << YAML::BeginMap << YAML::EndMap;
}

// ============================================================================
// YAML deserialize helpers  (read from YAML::Node)
// ============================================================================

// --- Scalars ---

inline float deserialize_yaml_float(const YAML::Node& node) {
    return node.as<float>();
}

inline double deserialize_yaml_double(const YAML::Node& node) {
    return node.as<double>();
}

inline int deserialize_yaml_int(const YAML::Node& node) {
    return node.as<int>();
}

inline uint32_t deserialize_yaml_uint32(const YAML::Node& node) {
    return node.as<uint32_t>();
}

inline uint64_t deserialize_yaml_uint64(const YAML::Node& node) {
    return node.as<uint64_t>();
}

inline bool deserialize_yaml_bool(const YAML::Node& node) {
    return node.as<bool>();
}

inline std::string deserialize_yaml_string(const YAML::Node& node) {
    return node.as<std::string>();
}

// --- Enums ---

inline AssetStatus deserialize_yaml_asset_status(const YAML::Node& node) {
    auto s = node.as<std::string>();
    if (s == "Loaded")  return AssetStatus::Loaded;
    if (s == "Failed")  return AssetStatus::Failed;
    return AssetStatus::Loading;
}

inline ProjectionType deserialize_yaml_projection_type(const YAML::Node& node) {
    auto s = node.as<std::string>();
    if (s == "Orthographic") return ProjectionType::Orthographic;
    return ProjectionType::Perspective;
}

inline BodyType deserialize_yaml_body_type(const YAML::Node& node) {
    auto s = node.as<std::string>();
    if (s == "Static")    return BodyType::Static;
    if (s == "Kinematic") return BodyType::Kinematic;
    return BodyType::Dynamic;
}

// --- AssetHandle ---

inline AssetHandle deserialize_yaml_asset_handle(const YAML::Node& node) {
    AssetHandle h;
    if (node["index"])      h.index      = node["index"].as<uint32_t>();
    if (node["generation"]) h.generation = node["generation"].as<uint32_t>();
    return h;
}

// --- glm types ---

inline glm::vec2 deserialize_yaml_vec2(const YAML::Node& node) {
    return glm::vec2(node[0].as<float>(), node[1].as<float>());
}

inline glm::vec3 deserialize_yaml_vec3(const YAML::Node& node) {
    return glm::vec3(node[0].as<float>(), node[1].as<float>(), node[2].as<float>());
}

inline glm::vec4 deserialize_yaml_vec4(const YAML::Node& node) {
    return glm::vec4(node[0].as<float>(), node[1].as<float>(),
                     node[2].as<float>(), node[3].as<float>());
}

inline glm::quat deserialize_yaml_quat(const YAML::Node& node) {
    // Stored as [w, x, y, z]
    return glm::quat(node[0].as<float>(), node[1].as<float>(),
                     node[2].as<float>(), node[3].as<float>());
}

inline glm::mat4 deserialize_yaml_mat4(const YAML::Node& node) {
    glm::mat4 m(0.0f);
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            m[col][row] = node[col][row].as<float>();
        }
    }
    return m;
}

// --- Component structs ---

inline Transform deserialize_yaml_transform(const YAML::Node& node) {
    Transform t;
    if (node["position"]) t.position = deserialize_yaml_vec3(node["position"]);
    if (node["rotation"]) t.rotation = deserialize_yaml_quat(node["rotation"]);
    if (node["scale"])    t.scale    = deserialize_yaml_vec3(node["scale"]);
    return t;
}

inline GlobalTransform deserialize_yaml_global_transform(const YAML::Node& node) {
    GlobalTransform g;
    if (node["matrix"]) g.matrix = deserialize_yaml_mat4(node["matrix"]);
    return g;
}

inline MeshRenderer deserialize_yaml_mesh_renderer(const YAML::Node& node) {
    MeshRenderer mr;
    if (node["mesh"])     mr.mesh     = Handle<MeshAsset>::from(deserialize_yaml_asset_handle(node["mesh"]));
    if (node["material"]) mr.material = Handle<MaterialAsset>::from(deserialize_yaml_asset_handle(node["material"]));
    if (node["flags"])    mr.flags    = node["flags"].as<uint32_t>();
    return mr;
}

inline PointLight deserialize_yaml_point_light(const YAML::Node& node) {
    PointLight pl;
    if (node["color"])     pl.color     = deserialize_yaml_vec3(node["color"]);
    if (node["intensity"]) pl.intensity = node["intensity"].as<float>();
    if (node["radius"])    pl.radius    = node["radius"].as<float>();
    return pl;
}

inline DirectionalLight deserialize_yaml_directional_light(const YAML::Node& node) {
    DirectionalLight dl;
    if (node["color"])     dl.color     = deserialize_yaml_vec3(node["color"]);
    if (node["intensity"]) dl.intensity = node["intensity"].as<float>();
    return dl;
}

inline Camera deserialize_yaml_camera(const YAML::Node& node) {
    Camera c;
    if (node["projection"]) c.projection = deserialize_yaml_projection_type(node["projection"]);
    if (node["fov_degrees"]) c.fov_degrees = node["fov_degrees"].as<float>();
    if (node["near_plane"])  c.near_plane  = node["near_plane"].as<float>();
    if (node["far_plane"])   c.far_plane   = node["far_plane"].as<float>();
    if (node["ortho_size"])  c.ortho_size  = node["ortho_size"].as<float>();
    return c;
}

inline ActiveCamera deserialize_yaml_active_camera(const YAML::Node& /*node*/) {
    return ActiveCamera{};
}

inline RigidBody deserialize_yaml_rigid_body(const YAML::Node& node) {
    RigidBody rb;
    if (node["body_type"])    rb.body_type    = deserialize_yaml_body_type(node["body_type"]);
    if (node["mass"])         rb.mass         = node["mass"].as<float>();
    if (node["friction"])     rb.friction     = node["friction"].as<float>();
    if (node["restitution"])  rb.restitution  = node["restitution"].as<float>();
    if (node["flags"])        rb.flags        = node["flags"].as<uint32_t>();
    return rb;
}

inline BoxCollider deserialize_yaml_box_collider(const YAML::Node& node) {
    BoxCollider bc;
    if (node["half_extents"]) bc.half_extents = deserialize_yaml_vec3(node["half_extents"]);
    if (node["offset"])       bc.offset       = deserialize_yaml_vec3(node["offset"]);
    if (node["flags"])        bc.flags        = node["flags"].as<uint32_t>();
    return bc;
}

inline SphereCollider deserialize_yaml_sphere_collider(const YAML::Node& node) {
    SphereCollider sc;
    if (node["radius"]) sc.radius = node["radius"].as<float>();
    if (node["offset"]) sc.offset = deserialize_yaml_vec3(node["offset"]);
    if (node["flags"])  sc.flags  = node["flags"].as<uint32_t>();
    return sc;
}

inline AudioSource deserialize_yaml_audio_source(const YAML::Node& node) {
    AudioSource as;
    if (node["volume"]) as.volume = node["volume"].as<float>();
    if (node["pitch"])  as.pitch  = node["pitch"].as<float>();
    if (node["flags"])  as.flags  = node["flags"].as<uint32_t>();
    return as;
}

inline Tag deserialize_yaml_tag(const YAML::Node& node) {
    Tag tag;
    if (node["name"]) tag.name = node["name"].as<std::string>();
    return tag;
}

inline Disabled deserialize_yaml_disabled(const YAML::Node& /*node*/) {
    return Disabled{};
}

} // namespace helios

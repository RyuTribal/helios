#pragma once

#include "helios/components/component_enums.h"
#include "helios/assets/handle.h"
#include "helios/assets/mesh_asset.h"
#include "helios/assets/material_asset.h"
#include "helios/ecs/asset_handle.h"
#include "helios/ecs/entity.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace helios {

namespace Flags {
    inline bool has(uint32_t flags, uint32_t bit) { return (flags & bit) != 0; }
    inline void set(uint32_t& flags, uint32_t bit) { flags |= bit; }
    inline void clear(uint32_t& flags, uint32_t bit) { flags &= ~bit; }
    inline void toggle(uint32_t& flags, uint32_t bit) { flags ^= bit; }
} // namespace Flags

namespace MeshFlags {
    constexpr uint32_t CastShadows    = 1u << 0;
    constexpr uint32_t ReceiveShadows = 1u << 1;
} // namespace MeshFlags

namespace RigidBodyFlags {
    constexpr uint32_t UseGravity = 1u << 0;
} // namespace RigidBodyFlags

namespace ColliderFlags {
    constexpr uint32_t IsTrigger = 1u << 0;
} // namespace ColliderFlags

namespace AudioFlags {
    constexpr uint32_t Looping     = 1u << 0;
    constexpr uint32_t PlayOnStart = 1u << 1;
} // namespace AudioFlags

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity
    glm::vec3 scale    = glm::vec3(1.0f);

    glm::mat4 to_mat4() const {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }
};

struct GlobalTransform {
    glm::mat4 matrix = glm::mat4(1.0f);
};

struct MeshRenderer {
    Handle<MeshAsset> mesh;
    Handle<MaterialAsset> material;  // override material (if null, uses mesh's default)
    uint32_t flags = MeshFlags::CastShadows | MeshFlags::ReceiveShadows;
    int32_t sort_order = 0;

    // Asset paths for persistence — handles are session-local, paths survive save/load
    std::string mesh_path;      // relative to asset root, e.g. "Meshes/Sphere.hvemesh"
    std::string material_path;
};

struct PointLight {
    glm::vec3 color     = glm::vec3(1.0f);
    float     intensity = 1.0f;
    float     radius    = 10.0f;
};

struct DirectionalLight {
    glm::vec3 color     = glm::vec3(1.0f);
    float     intensity = 1.0f;
};

struct Camera {
    ProjectionType projection = ProjectionType::Perspective;
    float fov_degrees         = 60.0f;
    float near_plane          = 0.1f;
    float far_plane           = 1000.0f;
    float ortho_size          = 10.0f;

    // Multi-camera support
    int32_t order = 0;           // render order: lower first, higher on top

    // Viewport: normalized [0,1] rect within the render target
    // Default covers the full window
    float viewport_x = 0.0f;
    float viewport_y = 0.0f;
    float viewport_w = 1.0f;
    float viewport_h = 1.0f;

    // Clear behavior
    enum class ClearMode { SolidColor, None };  // None = render on top of previous camera
    ClearMode clear_mode = ClearMode::SolidColor;

    // Render schedule name (e.g. "forward_plus", "ui", "debug_wireframe").
    // Empty = default pipeline. Resolved to a label at extract time.
    std::string render_schedule;
};

/// Tag component: marks one camera as the active/primary camera.
/// In the editor, only EditorCamera has this during Edit mode.
/// Game cameras get it when entering Play mode.
struct ActiveCamera {};

/// Render layer membership. Attach to cameras, meshes, and lights.
/// A camera renders an entity only when their layers intersect (bitwise AND != 0).
/// Entities/cameras WITHOUT this component are treated as layer 0.
struct RenderLayers {
    uint32_t mask = 1;  // layer 0 by default

    static RenderLayers layer(uint32_t n) { return {1u << n}; }
    static RenderLayers all() { return {0xFFFFFFFF}; }
    static RenderLayers none() { return {0}; }

    RenderLayers with(uint32_t n) const { return {mask | (1u << n)}; }
    RenderLayers without(uint32_t n) const { return {mask & ~(1u << n)}; }
    bool intersects(RenderLayers other) const { return (mask & other.mask) != 0; }
};

struct RigidBody {
    BodyType body_type = BodyType::Dynamic;
    BodyHandle handle;
    float    mass        = 1.0f;
    float    friction    = 0.5f;
    float    restitution = 0.3f;
    uint32_t flags       = RigidBodyFlags::UseGravity;
};

struct BoxCollider {
    glm::vec3 half_extents = glm::vec3(0.5f);
    glm::vec3 offset       = glm::vec3(0.0f);
    uint32_t  flags        = 0;
};

struct SphereCollider {
    float     radius = 0.5f;
    glm::vec3 offset = glm::vec3(0.0f);
    uint32_t  flags  = 0;
};

struct AudioSource {
    SoundHandle clip;
    float    volume = 1.0f;
    float    pitch  = 1.0f;
    uint32_t flags  = 0;
};

struct Parent {
    Entity entity;
};

struct Children {
    std::vector<Entity> entities;
};

struct Tag {
    std::string name;
};

/// Marks an entity as the root of a loaded scene.
/// All entities belonging to this scene are children (direct or nested) of this entity.
/// The scene_path is relative to the asset root (e.g. "Scenes/Level_1.hvescn").
struct SceneRoot {
    std::string scene_name;   // human-readable name
    std::string scene_path;   // relative to asset root, empty = unsaved
};

/// Tag component: marks an entity as disabled (excluded from most queries).
struct Disabled {};

/// Marker: entity is editor-internal (e.g. EditorCamera). Core systems
/// (serializer, physics, scripts) skip entities with this component.
struct EditorOnly {};

} // namespace helios

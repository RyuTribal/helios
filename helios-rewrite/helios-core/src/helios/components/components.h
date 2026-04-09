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

// ---------------------------------------------------------------------------
// Flags helpers
// ---------------------------------------------------------------------------

namespace Flags {
    inline bool has(uint32_t flags, uint32_t bit) { return (flags & bit) != 0; }
    inline void set(uint32_t& flags, uint32_t bit) { flags |= bit; }
    inline void clear(uint32_t& flags, uint32_t bit) { flags &= ~bit; }
    inline void toggle(uint32_t& flags, uint32_t bit) { flags ^= bit; }
} // namespace Flags

// ---------------------------------------------------------------------------
// Per-component flag namespaces
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// GlobalTransform
// ---------------------------------------------------------------------------

struct GlobalTransform {
    glm::mat4 matrix = glm::mat4(1.0f);
};

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

struct MeshRenderer {
    Handle<MeshAsset> mesh;
    Handle<MaterialAsset> material;  // override material (if null, uses mesh's default)
    uint32_t flags = MeshFlags::CastShadows | MeshFlags::ReceiveShadows;

    MeshRenderer() = default;
    explicit MeshRenderer(Handle<MeshAsset> m)
        : mesh(std::move(m)) {}
    MeshRenderer(Handle<MeshAsset> m, Handle<MaterialAsset> mat)
        : mesh(std::move(m)), material(std::move(mat)) {}
    MeshRenderer(Handle<MeshAsset> m, Handle<MaterialAsset> mat, uint32_t f)
        : mesh(std::move(m)), material(std::move(mat)), flags(f) {}
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
};

/// Tag component: marks one camera as the active/primary camera.
struct ActiveCamera {};

// ---------------------------------------------------------------------------
// Physics
// ---------------------------------------------------------------------------

struct RigidBody {
    BodyType body_type = BodyType::Dynamic;
    BodyHandle handle;
    float    mass  = 1.0f;
    uint32_t flags = RigidBodyFlags::UseGravity;
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

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

struct AudioSource {
    SoundHandle clip;
    float    volume = 1.0f;
    float    pitch  = 1.0f;
    uint32_t flags  = 0;
};

// ---------------------------------------------------------------------------
// Scripting
// ---------------------------------------------------------------------------

struct ScriptInstance {
    AssetHandle script;
};

// ---------------------------------------------------------------------------
// Hierarchy
// ---------------------------------------------------------------------------

struct Parent {
    Entity entity;
};

struct Children {
    std::vector<Entity> entities;
};

// ---------------------------------------------------------------------------
// Misc / Tags
// ---------------------------------------------------------------------------

struct Tag {
    std::string name;
};

/// Tag component: marks an entity as disabled (excluded from most queries).
struct Disabled {};

} // namespace helios

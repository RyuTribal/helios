#pragma once

#include "helios/components/component_enums.h"
#include "helios/ecs/asset_handle.h"
#include "helios/ecs/entity.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace helios {

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
    AssetHandle mesh;
    AssetHandle material;
    bool cast_shadows   = true;
    bool receive_shadows = true;
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
};

/// Tag component: marks one camera as the active/primary camera.
struct ActiveCamera {};

// ---------------------------------------------------------------------------
// Physics
// ---------------------------------------------------------------------------

struct RigidBody {
    BodyType body_type = BodyType::Dynamic;
    BodyHandle handle;
    float mass        = 1.0f;
    bool  use_gravity = true;
};

struct BoxCollider {
    glm::vec3 half_extents = glm::vec3(0.5f);
    glm::vec3 offset       = glm::vec3(0.0f);
    bool      is_trigger   = false;
};

struct SphereCollider {
    float     radius    = 0.5f;
    glm::vec3 offset    = glm::vec3(0.0f);
    bool      is_trigger = false;
};

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

struct AudioSource {
    SoundHandle clip;
    float       volume    = 1.0f;
    float       pitch     = 1.0f;
    bool        looping   = false;
    bool        play_on_start = false;
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

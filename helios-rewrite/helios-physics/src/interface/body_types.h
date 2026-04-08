// helios-physics/src/interface/body_types.h
#pragma once

#include <cstdint>
#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace helios::physics {

// Opaque handle into the physics backend. Zero means invalid.
using BodyHandle = uint64_t;

enum class BodyType : uint8_t {
    Static,       // Non-movable. Infinite mass. Placed once.
    Dynamic,      // Responds to forces, gravity, impulses.
    Kinematic,    // Movable by code, does not respond to forces. Pushes dynamic bodies.
};

// --- Collider shape descriptors (plain data, no Jolt types) ---

struct BoxShape {
    glm::vec3 half_extents{0.5f};
};

struct SphereShape {
    float radius = 0.5f;
};

struct CapsuleShape {
    float half_height = 0.5f;
    float radius      = 0.25f;
};

// Variant of all supported collider shapes.
// Extend this variant when adding new shape types (MeshShape, ConvexHullShape, etc.).
using ColliderShape = std::variant<BoxShape, SphereShape, CapsuleShape>;

// Complete description needed to create a physics body in the backend.
struct BodyDesc {
    BodyType       type        = BodyType::Static;
    glm::vec3      position    {0.0f};
    glm::quat      rotation    {1.0f, 0.0f, 0.0f, 0.0f};  // w, x, y, z
    float          mass        = 1.0f;
    float          friction    = 0.5f;
    float          restitution = 0.3f;
    ColliderShape  shape       = BoxShape{};
};

} // namespace helios::physics

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace helios::physics {

// Contact reported by the physics backend after a simulation step.
// entity_a and entity_b are the ECS entities whose bodies collided.
// The backend maps BodyHandle -> Entity via user data stored on each body.
struct ContactEvent {
    uint64_t   entity_a    = 0;    // Entity ID of first body
    uint64_t   entity_b    = 0;    // Entity ID of second body
    glm::vec3  world_point {0.0f}; // Contact point in world space
    glm::vec3  normal      {0.0f}; // Contact normal (from A toward B)
    float      impulse     = 0.0f; // Normal impulse magnitude
};

// Result of a single ray-vs-world intersection.
struct RayHit {
    uint64_t   entity   = 0;      // Entity ID of the hit body
    glm::vec3  point    {0.0f};   // Hit point in world space
    glm::vec3  normal   {0.0f};   // Surface normal at hit point
    float      distance = 0.0f;   // Distance from ray origin to hit point
};

} // namespace helios::physics

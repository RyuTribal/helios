// helios-physics/src/interface/physics_world.h
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "interface/body_types.h"
#include "interface/contact_event.h"

namespace helios::physics {

// PhysicsConfig resource inserted by the plugin. Systems read this
// for fixed timestep, gravity, etc.
struct PhysicsConfig {
    float     fixed_timestep = 1.0f / 60.0f;
    glm::vec3 gravity        {0.0f, -9.81f, 0.0f};
    int       collision_steps = 1;
};

// Abstract physics world interface.
//
// Implementation: JoltPhysicsWorld (Jolt Physics backend).
//
// Ownership: created by the plugin, stored as a World resource via
//   std::unique_ptr<PhysicsWorld>.
class PhysicsWorld {
public:
    virtual ~PhysicsWorld() = default;

    // --- Body lifecycle ---

    // Create a body from a descriptor. Returns an opaque handle.
    // The backend stores the entity_id as user data on the body so that
    // contacts can be mapped back to ECS entities.
    virtual BodyHandle create_body(const BodyDesc& desc, uint64_t entity_id = 0) = 0;

    // Remove and destroy a body. The handle becomes invalid.
    virtual void destroy_body(BodyHandle handle) = 0;

    // --- Transform ---

    virtual void      set_transform(BodyHandle handle,
                                    const glm::vec3& pos,
                                    const glm::quat& rot) = 0;
    virtual glm::vec3 get_position(BodyHandle handle) const = 0;
    virtual glm::quat get_rotation(BodyHandle handle) const = 0;

    // --- Velocity / forces ---

    virtual void set_velocity(BodyHandle handle, const glm::vec3& linear) = 0;
    virtual glm::vec3 get_velocity(BodyHandle handle) const = 0;
    virtual void set_angular_velocity(BodyHandle handle, const glm::vec3& angular) = 0;
    virtual glm::vec3 get_angular_velocity(BodyHandle handle) const = 0;
    virtual void apply_force(BodyHandle handle, const glm::vec3& force) = 0;
    virtual void apply_impulse(BodyHandle handle, const glm::vec3& impulse) = 0;
    virtual void apply_torque(BodyHandle handle, const glm::vec3& torque) = 0;

    // --- Simulation ---

    // Advance the simulation by dt seconds. Internally may sub-step.
    virtual void step(float dt) = 0;

    // Drain all contact events accumulated during the last step().
    // The returned vector is moved out; the internal buffer is cleared.
    virtual std::vector<ContactEvent> drain_contacts() = 0;

    // --- Raycasting ---

    // Cast a ray and return the closest hit, or std::nullopt.
    virtual std::optional<RayHit> raycast(const glm::vec3& origin,
                                          const glm::vec3& direction,
                                          float max_distance) const = 0;

    // Cast a ray and return all hits sorted by distance.
    virtual std::vector<RayHit> raycast_all(const glm::vec3& origin,
                                            const glm::vec3& direction,
                                            float max_distance) const = 0;

    // --- Configuration ---

    virtual void set_gravity(const glm::vec3& gravity) = 0;
};

} // namespace helios::physics

// helios-physics/src/jolt/jolt_physics_world.h
#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include "interface/physics_world.h"
#include "jolt/jolt_utils.h"

namespace helios::physics {

// Thread-safe contact listener that accumulates ContactEvents during
// simulation. Contacts are drained after each step() call.
class JoltContactListener final : public JPH::ContactListener {
public:
    JPH::ValidateResult OnContactValidate(
        const JPH::Body& body1,
        const JPH::Body& body2,
        JPH::RVec3Arg base_offset,
        const JPH::CollideShapeResult& collision_result) override;

    void OnContactAdded(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override;

    void OnContactPersisted(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override;

    void OnContactRemoved(
        const JPH::SubShapeIDPair& sub_shape_pair) override;

    // Move-drain all accumulated contacts. Called from the main thread
    // after step(). Clears the internal buffer.
    std::vector<ContactEvent> drain();

private:
    std::mutex              m_mutex;
    std::vector<ContactEvent> m_contacts;
};

// Jolt body activation listener (logging only, no gameplay logic).
class JoltActivationListener final : public JPH::BodyActivationListener {
public:
    void OnBodyActivated(const JPH::BodyID& body_id, JPH::uint64 user_data) override;
    void OnBodyDeactivated(const JPH::BodyID& body_id, JPH::uint64 user_data) override;
};

// Production PhysicsWorld implementation backed by Jolt Physics.
//
// RAII: constructor initializes the entire Jolt subsystem. Destructor
// cleans up all bodies and shuts down Jolt. No Init()/Shutdown().
class JoltPhysicsWorld final : public PhysicsWorld {
public:
    explicit JoltPhysicsWorld(const PhysicsConfig& config,
                              uint32_t temp_allocator_size_mb = 10);
    ~JoltPhysicsWorld() override;

    // Non-copyable, non-movable (Jolt state is not trivially relocatable)
    JoltPhysicsWorld(const JoltPhysicsWorld&) = delete;
    JoltPhysicsWorld& operator=(const JoltPhysicsWorld&) = delete;
    JoltPhysicsWorld(JoltPhysicsWorld&&) = delete;
    JoltPhysicsWorld& operator=(JoltPhysicsWorld&&) = delete;

    // --- PhysicsWorld interface ---

    BodyHandle create_body(const BodyDesc& desc, uint64_t entity_id = 0) override;
    void       destroy_body(BodyHandle handle) override;

    void       set_transform(BodyHandle handle,
                             const glm::vec3& pos,
                             const glm::quat& rot) override;
    glm::vec3  get_position(BodyHandle handle) const override;
    glm::quat  get_rotation(BodyHandle handle) const override;

    void       set_velocity(BodyHandle handle, const glm::vec3& linear) override;
    glm::vec3  get_velocity(BodyHandle handle) const override;
    void       set_angular_velocity(BodyHandle handle, const glm::vec3& angular) override;
    glm::vec3  get_angular_velocity(BodyHandle handle) const override;
    void       apply_force(BodyHandle handle, const glm::vec3& force) override;
    void       apply_impulse(BodyHandle handle, const glm::vec3& impulse) override;
    void       apply_torque(BodyHandle handle, const glm::vec3& torque) override;

    void       step(float dt) override;
    std::vector<ContactEvent> drain_contacts() override;

    std::optional<RayHit> raycast(const glm::vec3& origin,
                                  const glm::vec3& direction,
                                  float max_distance) const override;
    std::vector<RayHit>   raycast_all(const glm::vec3& origin,
                                      const glm::vec3& direction,
                                      float max_distance) const override;

    void set_gravity(const glm::vec3& gravity) override;

private:
    // Jolt subsystem objects -- order matters for destruction
    std::unique_ptr<JPH::TempAllocator>       m_temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool>  m_job_system;
    std::unique_ptr<JPH::PhysicsSystem>        m_physics_system;

    // Layer filters (owned, non-polymorphic)
    jolt::BroadPhaseLayerMapping    m_broad_phase_layer_mapping;
    jolt::ObjectVsBroadPhaseFilter  m_object_vs_broad_phase_filter;
    jolt::ObjectLayerPairFilter     m_object_layer_pair_filter;

    // Listeners
    JoltContactListener     m_contact_listener;
    JoltActivationListener  m_activation_listener;

    // BodyHandle -> JPH::BodyID mapping
    std::unordered_map<BodyHandle, JPH::BodyID> m_handle_to_body;

    // Configuration
    PhysicsConfig m_config;
    bool          m_broad_phase_optimized = false;

    // Jolt system limits
    static constexpr uint32_t MAX_BODIES             = 65536;
    static constexpr uint32_t NUM_BODY_MUTEXES       = 0;     // auto
    static constexpr uint32_t MAX_BODY_PAIRS          = 65536;
    static constexpr uint32_t MAX_CONTACT_CONSTRAINTS = 10240;

    // Internal: get the body interface (non-locking for single-threaded access)
    JPH::BodyInterface& body_interface();
    const JPH::BodyInterface& body_interface() const;
};

} // namespace helios::physics

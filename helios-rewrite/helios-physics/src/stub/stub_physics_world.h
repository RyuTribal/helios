// helios-physics/src/stub/stub_physics_world.h
#pragma once

#include <unordered_map>
#include <vector>

#include "interface/physics_world.h"

namespace helios::physics {

// No-op stub backend that implements PhysicsWorld without any real simulation.
// Used when the Jolt backend is unavailable or for lightweight testing.
// All methods either log warnings or return sensible defaults.
class StubPhysicsWorld final : public PhysicsWorld {
public:
    explicit StubPhysicsWorld(const PhysicsConfig& config = {});
    ~StubPhysicsWorld() override;

    // Non-copyable, non-movable
    StubPhysicsWorld(const StubPhysicsWorld&) = delete;
    StubPhysicsWorld& operator=(const StubPhysicsWorld&) = delete;
    StubPhysicsWorld(StubPhysicsWorld&&) = delete;
    StubPhysicsWorld& operator=(StubPhysicsWorld&&) = delete;

    // --- PhysicsWorld interface ---

    BodyHandle create_body(const BodyDesc& desc, uint64_t entity_id = 0) override;
    void       destroy_body(BodyHandle handle) override;

    void       set_transform(BodyHandle handle,
                             const glm::vec3& pos,
                             const glm::quat& rot) override;
    glm::vec3  get_position(BodyHandle handle) const override;
    glm::quat  get_rotation(BodyHandle handle) const override;

    void       set_velocity(BodyHandle handle, const glm::vec3& linear) override;
    void       apply_force(BodyHandle handle, const glm::vec3& force) override;
    void       apply_impulse(BodyHandle handle, const glm::vec3& impulse) override;

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
    struct BodyData {
        BodyDesc  desc;
        uint64_t  entity_id = 0;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 velocity{0.0f};
    };

    std::unordered_map<BodyHandle, BodyData> m_bodies;
    PhysicsConfig m_config;
    BodyHandle    m_next_handle = 1;
};

} // namespace helios::physics

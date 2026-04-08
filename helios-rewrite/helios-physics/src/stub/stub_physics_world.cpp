// helios-physics/src/stub/stub_physics_world.cpp

#include "stub/stub_physics_world.h"

#include <cstdio>

namespace helios::physics {

namespace {
    bool s_warned_once = false;
    void warn_stub() {
        if (!s_warned_once) {
            std::fprintf(stderr,
                "[helios-physics] WARNING: Using stub physics backend. "
                "No real simulation is occurring.\n");
            s_warned_once = true;
        }
    }
} // anonymous namespace

StubPhysicsWorld::StubPhysicsWorld(const PhysicsConfig& config)
    : m_config(config)
{
    warn_stub();
}

StubPhysicsWorld::~StubPhysicsWorld() = default;

BodyHandle StubPhysicsWorld::create_body(const BodyDesc& desc, uint64_t entity_id) {
    BodyHandle handle = (entity_id != 0)
        ? entity_id
        : m_next_handle++;

    BodyData data;
    data.desc      = desc;
    data.entity_id = entity_id;
    data.position  = desc.position;
    data.rotation  = desc.rotation;
    m_bodies[handle] = data;

    return handle;
}

void StubPhysicsWorld::destroy_body(BodyHandle handle) {
    m_bodies.erase(handle);
}

void StubPhysicsWorld::set_transform(BodyHandle handle,
                                     const glm::vec3& pos,
                                     const glm::quat& rot) {
    auto it = m_bodies.find(handle);
    if (it == m_bodies.end()) return;
    it->second.position = pos;
    it->second.rotation = rot;
}

glm::vec3 StubPhysicsWorld::get_position(BodyHandle handle) const {
    auto it = m_bodies.find(handle);
    if (it == m_bodies.end()) return glm::vec3(0.0f);
    return it->second.position;
}

glm::quat StubPhysicsWorld::get_rotation(BodyHandle handle) const {
    auto it = m_bodies.find(handle);
    if (it == m_bodies.end()) return glm::quat(1, 0, 0, 0);
    return it->second.rotation;
}

void StubPhysicsWorld::set_velocity(BodyHandle handle, const glm::vec3& linear) {
    auto it = m_bodies.find(handle);
    if (it == m_bodies.end()) return;
    it->second.velocity = linear;
}

void StubPhysicsWorld::apply_force(BodyHandle /*handle*/, const glm::vec3& /*force*/) {
    // No-op in stub
}

void StubPhysicsWorld::apply_impulse(BodyHandle handle, const glm::vec3& impulse) {
    auto it = m_bodies.find(handle);
    if (it == m_bodies.end()) return;
    // Simple velocity change (mass = 1 approximation)
    if (it->second.desc.mass > 0.0f) {
        it->second.velocity += impulse / it->second.desc.mass;
    }
}

void StubPhysicsWorld::step(float dt) {
    // Simple Euler integration for dynamic bodies
    for (auto& [handle, body] : m_bodies) {
        if (body.desc.type != BodyType::Dynamic) continue;

        // Apply gravity
        body.velocity += m_config.gravity * dt;

        // Integrate position
        body.position += body.velocity * dt;
    }
}

std::vector<ContactEvent> StubPhysicsWorld::drain_contacts() {
    // Stub does not detect contacts
    return {};
}

std::optional<RayHit> StubPhysicsWorld::raycast(
    const glm::vec3& /*origin*/,
    const glm::vec3& /*direction*/,
    float /*max_distance*/) const
{
    return std::nullopt;
}

std::vector<RayHit> StubPhysicsWorld::raycast_all(
    const glm::vec3& /*origin*/,
    const glm::vec3& /*direction*/,
    float /*max_distance*/) const
{
    return {};
}

void StubPhysicsWorld::set_gravity(const glm::vec3& gravity) {
    m_config.gravity = gravity;
}

} // namespace helios::physics

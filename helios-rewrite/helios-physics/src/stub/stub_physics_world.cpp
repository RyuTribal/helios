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

void StubPhysicsWorld::apply_force(BodyHandle handle, const glm::vec3& force) {
    auto it = m_bodies.find(handle);
    if (it == m_bodies.end()) return;
    it->second.accumulated_force += force;
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
    // Simple Euler integration for dynamic bodies with basic collision
    for (auto& [handle, body] : m_bodies) {
        if (body.desc.type != BodyType::Dynamic) continue;

        // Apply accumulated forces (F = ma → a = F/m)
        if (body.desc.mass > 0.0f) {
            body.velocity += (body.accumulated_force / body.desc.mass) * dt;
        }
        body.accumulated_force = glm::vec3{0.0f};

        // Apply gravity
        body.velocity += m_config.gravity * dt;

        // Integrate position
        glm::vec3 new_pos = body.position + body.velocity * dt;

        // Check against all static bodies for simple floor collision
        float body_radius = 0.0f;
        if (auto* sphere = std::get_if<SphereShape>(&body.desc.shape)) {
            body_radius = sphere->radius;
        } else if (auto* box = std::get_if<BoxShape>(&body.desc.shape)) {
            body_radius = box->half_extents.y;
        }

        for (auto& [other_handle, other] : m_bodies) {
            if (other.desc.type != BodyType::Static) continue;

            // Simple floor plane: static body top surface
            float floor_y = other.position.y;
            if (auto* box = std::get_if<BoxShape>(&other.desc.shape)) {
                floor_y += box->half_extents.y;
            }

            float body_bottom = new_pos.y - body_radius;
            if (body_bottom < floor_y && body.velocity.y < 0.0f) {
                // Bounce
                new_pos.y = floor_y + body_radius;
                float impulse_mag = std::abs(body.velocity.y) * body.desc.mass;
                body.velocity.y = -body.velocity.y * body.desc.restitution;

                // Generate contact event
                ContactEvent event;
                event.entity_a = body.entity_id;
                event.entity_b = other.entity_id;
                event.world_point = glm::vec3(new_pos.x, floor_y, new_pos.z);
                event.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                event.impulse = impulse_mag;
                m_pending_contacts.push_back(event);
            }
        }

        body.position = new_pos;
    }
}

std::vector<ContactEvent> StubPhysicsWorld::drain_contacts() {
    std::vector<ContactEvent> result = std::move(m_pending_contacts);
    m_pending_contacts.clear();
    return result;
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

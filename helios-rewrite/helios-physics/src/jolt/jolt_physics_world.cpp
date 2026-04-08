// helios-physics/src/jolt/jolt_physics_world.cpp

#include "jolt/jolt_physics_world.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <thread>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

namespace helios::physics {

// ============================================================
// JoltContactListener
// ============================================================

JPH::ValidateResult JoltContactListener::OnContactValidate(
    const JPH::Body& /*body1*/,
    const JPH::Body& /*body2*/,
    JPH::RVec3Arg /*base_offset*/,
    const JPH::CollideShapeResult& /*collision_result*/)
{
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void JoltContactListener::OnContactAdded(
    const JPH::Body& body1,
    const JPH::Body& body2,
    const JPH::ContactManifold& manifold,
    JPH::ContactSettings& /*settings*/)
{
    ContactEvent event;
    event.entity_a    = body1.GetUserData();
    event.entity_b    = body2.GetUserData();
    event.world_point = jolt::to_glm(manifold.mBaseOffset +
                        manifold.mRelativeContactPointsOn1[0]);
    event.normal      = jolt::to_glm(manifold.mWorldSpaceNormal);
    event.impulse     = 0.0f; // Impulse not available in OnContactAdded

    std::lock_guard lock(m_mutex);
    m_contacts.push_back(event);
}

void JoltContactListener::OnContactPersisted(
    const JPH::Body& /*body1*/,
    const JPH::Body& /*body2*/,
    const JPH::ContactManifold& /*manifold*/,
    JPH::ContactSettings& /*settings*/)
{
    // Persisted contacts are not emitted as events by default.
}

void JoltContactListener::OnContactRemoved(
    const JPH::SubShapeIDPair& /*sub_shape_pair*/)
{
    // Contact-removed events could be added here if needed.
}

std::vector<ContactEvent> JoltContactListener::drain() {
    std::lock_guard lock(m_mutex);
    std::vector<ContactEvent> result = std::move(m_contacts);
    m_contacts.clear();
    return result;
}

// ============================================================
// JoltActivationListener
// ============================================================

void JoltActivationListener::OnBodyActivated(
    const JPH::BodyID& /*body_id*/, JPH::uint64 /*user_data*/)
{
}

void JoltActivationListener::OnBodyDeactivated(
    const JPH::BodyID& /*body_id*/, JPH::uint64 /*user_data*/)
{
}

// ============================================================
// Jolt global init helpers (thread-safe via static local)
// ============================================================

namespace {

struct JoltGlobalInit {
    static void acquire() {
        std::lock_guard lock(s_mutex);
        if (s_ref_count++ == 0) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    static void release() {
        std::lock_guard lock(s_mutex);
        if (--s_ref_count == 0) {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

private:
    static inline std::mutex s_mutex;
    static inline int s_ref_count = 0;
};

} // anonymous namespace

// ============================================================
// JoltPhysicsWorld
// ============================================================

JoltPhysicsWorld::JoltPhysicsWorld(const PhysicsConfig& config,
                                   uint32_t temp_allocator_size_mb)
    : m_config(config)
{
    JoltGlobalInit::acquire();

    m_temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(
        temp_allocator_size_mb * 1024 * 1024
    );

    const auto num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
    m_job_system = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        static_cast<int>(num_threads)
    );

    m_physics_system = std::make_unique<JPH::PhysicsSystem>();
    m_physics_system->Init(
        MAX_BODIES,
        NUM_BODY_MUTEXES,
        MAX_BODY_PAIRS,
        MAX_CONTACT_CONSTRAINTS,
        m_broad_phase_layer_mapping,
        m_object_vs_broad_phase_filter,
        m_object_layer_pair_filter
    );

    m_physics_system->SetContactListener(&m_contact_listener);
    m_physics_system->SetBodyActivationListener(&m_activation_listener);
    m_physics_system->SetGravity(jolt::to_jph(config.gravity));
}

JoltPhysicsWorld::~JoltPhysicsWorld() {
    auto& bi = body_interface();
    for (auto& [handle, body_id] : m_handle_to_body) {
        bi.RemoveBody(body_id);
        bi.DestroyBody(body_id);
    }
    m_handle_to_body.clear();

    m_physics_system.reset();
    m_job_system.reset();
    m_temp_allocator.reset();

    JoltGlobalInit::release();
}

JPH::BodyInterface& JoltPhysicsWorld::body_interface() {
    return m_physics_system->GetBodyInterface();
}

const JPH::BodyInterface& JoltPhysicsWorld::body_interface() const {
    return m_physics_system->GetBodyInterface();
}

// --- Body lifecycle ---

BodyHandle JoltPhysicsWorld::create_body(const BodyDesc& desc, uint64_t entity_id) {
    auto& bi = body_interface();

    JPH::ShapeRefC shape;

    if (auto* box = std::get_if<BoxShape>(&desc.shape)) {
        auto settings = JPH::BoxShapeSettings(jolt::to_jph(box->half_extents));
        auto result = settings.Create();
        assert(!result.HasError());
        shape = result.Get();
    } else if (auto* sphere = std::get_if<SphereShape>(&desc.shape)) {
        auto settings = JPH::SphereShapeSettings(sphere->radius);
        auto result = settings.Create();
        assert(!result.HasError());
        shape = result.Get();
    } else if (auto* capsule = std::get_if<CapsuleShape>(&desc.shape)) {
        auto settings = JPH::CapsuleShapeSettings(capsule->half_height, capsule->radius);
        auto result = settings.Create();
        assert(!result.HasError());
        shape = result.Get();
    }

    assert(shape != nullptr);

    JPH::ObjectLayer layer = jolt::object_layer_for(desc.type);
    JPH::EMotionType motion_type = jolt::to_jph_motion_type(desc.type);

    JPH::BodyCreationSettings body_settings(
        shape,
        jolt::to_jph_rvec(desc.position),
        jolt::to_jph(desc.rotation),
        motion_type,
        layer
    );

    body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    body_settings.mMassPropertiesOverride.mMass = desc.mass;

    JPH::Body* body = bi.CreateBody(body_settings);
    assert(body != nullptr);

    body->SetUserData(entity_id);
    body->SetFriction(desc.friction);
    body->SetRestitution(desc.restitution);

    JPH::EActivation activation = (desc.type == BodyType::Static)
        ? JPH::EActivation::DontActivate
        : JPH::EActivation::Activate;
    bi.AddBody(body->GetID(), activation);

    m_broad_phase_optimized = false;

    BodyHandle handle = (entity_id != 0)
        ? entity_id
        : static_cast<BodyHandle>(body->GetID().GetIndexAndSequenceNumber());
    m_handle_to_body[handle] = body->GetID();

    return handle;
}

void JoltPhysicsWorld::destroy_body(BodyHandle handle) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    auto& bi = body_interface();
    bi.RemoveBody(it->second);
    bi.DestroyBody(it->second);
    m_handle_to_body.erase(it);
}

// --- Transform ---

void JoltPhysicsWorld::set_transform(BodyHandle handle,
                                     const glm::vec3& pos,
                                     const glm::quat& rot) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().SetPositionAndRotation(
        it->second,
        jolt::to_jph_rvec(pos),
        jolt::to_jph(rot),
        JPH::EActivation::Activate
    );
}

glm::vec3 JoltPhysicsWorld::get_position(BodyHandle handle) const {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return glm::vec3(0.0f);

    return jolt::to_glm(body_interface().GetPosition(it->second));
}

glm::quat JoltPhysicsWorld::get_rotation(BodyHandle handle) const {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return glm::quat(1, 0, 0, 0);

    return jolt::to_glm(body_interface().GetRotation(it->second));
}

// --- Velocity / forces ---

void JoltPhysicsWorld::set_velocity(BodyHandle handle, const glm::vec3& linear) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().SetLinearVelocity(it->second, jolt::to_jph(linear));
}

void JoltPhysicsWorld::apply_force(BodyHandle handle, const glm::vec3& force) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().AddForce(it->second, jolt::to_jph(force));
}

void JoltPhysicsWorld::apply_impulse(BodyHandle handle, const glm::vec3& impulse) {
    auto it = m_handle_to_body.find(handle);
    if (it == m_handle_to_body.end()) return;

    body_interface().AddImpulse(it->second, jolt::to_jph(impulse));
}

// --- Simulation ---

void JoltPhysicsWorld::step(float dt) {
    if (!m_broad_phase_optimized) {
        m_physics_system->OptimizeBroadPhase();
        m_broad_phase_optimized = true;
    }

    m_physics_system->Update(
        dt,
        m_config.collision_steps,
        m_temp_allocator.get(),
        m_job_system.get()
    );
}

std::vector<ContactEvent> JoltPhysicsWorld::drain_contacts() {
    return m_contact_listener.drain();
}

// --- Raycasting ---

std::optional<RayHit> JoltPhysicsWorld::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float max_distance) const
{
    JPH::RRayCast ray(
        jolt::to_jph_rvec(origin),
        jolt::to_jph(glm::normalize(direction) * max_distance)
    );

    JPH::RayCastResult result;
    bool hit = m_physics_system->GetNarrowPhaseQuery().CastRay(
        ray, result
    );

    if (!hit) return std::nullopt;

    JPH::BodyLockRead lock(m_physics_system->GetBodyLockInterface(), result.mBodyID);
    if (!lock.Succeeded()) return std::nullopt;

    const JPH::Body& body = lock.GetBody();

    RayHit ray_hit;
    ray_hit.entity   = body.GetUserData();
    ray_hit.point    = jolt::to_glm(ray.GetPointOnRay(result.mFraction));
    ray_hit.normal   = glm::vec3(0.0f); // Normal requires shape query, simplified here
    ray_hit.distance = result.mFraction * max_distance;
    return ray_hit;
}

std::vector<RayHit> JoltPhysicsWorld::raycast_all(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float max_distance) const
{
    JPH::RRayCast ray(
        jolt::to_jph_rvec(origin),
        jolt::to_jph(glm::normalize(direction) * max_distance)
    );

    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    m_physics_system->GetNarrowPhaseQuery().CastRay(
        ray, JPH::RayCastSettings(), collector
    );

    collector.Sort();

    std::vector<RayHit> hits;
    hits.reserve(collector.mHits.size());

    for (auto& hit : collector.mHits) {
        JPH::BodyLockRead lock(m_physics_system->GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded()) continue;

        const JPH::Body& body = lock.GetBody();
        RayHit ray_hit;
        ray_hit.entity   = body.GetUserData();
        ray_hit.point    = jolt::to_glm(ray.GetPointOnRay(hit.mFraction));
        ray_hit.normal   = glm::vec3(0.0f);
        ray_hit.distance = hit.mFraction * max_distance;
        hits.push_back(ray_hit);
    }

    return hits;
}

// --- Configuration ---

void JoltPhysicsWorld::set_gravity(const glm::vec3& gravity) {
    m_physics_system->SetGravity(jolt::to_jph(gravity));
}

} // namespace helios::physics

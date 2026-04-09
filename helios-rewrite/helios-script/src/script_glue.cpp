// helios-script/src/script_glue.cpp
#include <helios/script/script_glue.h>
#include <helios/ecs/world.h>
#include <helios/components/components.h>
#include <helios/input/raw_input.h>
#include "interface/physics_plugin.h"
#include "script_log.h"

namespace helios {
namespace {

// -- Helper: recover World& from opaque pointer -------------------------------
inline World& world_from(void* ctx) {
    return *static_cast<World*>(ctx);
}

// Pack/unpack Entity from uint64_t
inline uint64_t entity_to_raw(Entity e) {
    return (static_cast<uint64_t>(e.generation) << 32) | static_cast<uint64_t>(e.index);
}

inline Entity entity_from_raw(uint64_t raw) {
    return Entity{
        .index      = static_cast<uint32_t>(raw & 0xFFFFFFFF),
        .generation = static_cast<uint32_t>(raw >> 32)
    };
}

// -- Logging ------------------------------------------------------------------

void GlueLog(void* /*world_ctx*/, int level, const char* message) {
    switch (level) {
        case 0: HELIOS_LOG(Script, Trace, "{}", message); break;
        case 1: HELIOS_LOG(Script, Info,  "{}", message); break;
        case 2: HELIOS_LOG(Script, Warn,  "{}", message); break;
        case 3: HELIOS_LOG(Script, Error, "{}", message); break;
        default: HELIOS_LOG(Script, Info,  "{}", message); break;
    }
}

// -- Entity operations --------------------------------------------------------

uint64_t GlueSpawn(void* world_ctx) {
    auto& w = world_from(world_ctx);
    Entity e = w.spawn();
    return entity_to_raw(e);
}

void GlueDespawn(void* world_ctx, uint64_t entity_id) {
    auto& w = world_from(world_ctx);
    w.despawn(entity_from_raw(entity_id));
}

bool GlueIsAlive(void* world_ctx, uint64_t entity_id) {
    auto& w = world_from(world_ctx);
    return w.is_alive(entity_from_raw(entity_id));
}

// -- Component access (string-based dispatch) ---------------------------------

bool GlueHasComponent(void* world_ctx, uint64_t entity_id, const char* name) {
    auto& w = world_from(world_ctx);
    auto e = entity_from_raw(entity_id);
    std::string component_name(name);

    if (component_name == "Transform")
        return w.has<Transform>(e);
    // Add more component types as they are defined.
    return false;
}

// -- Transform shortcuts ------------------------------------------------------

void GlueTransformGetTranslation(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* t = w.try_get<Transform>(entity);
    if (!t) return;
    out[0] = t->position.x;
    out[1] = t->position.y;
    out[2] = t->position.z;
}

void GlueTransformSetTranslation(void* world_ctx, uint64_t id, float* in) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* t = w.try_get<Transform>(entity);
    if (!t) return;
    t->position = {in[0], in[1], in[2]};
}

void GlueTransformGetRotation(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* t = w.try_get<Transform>(entity);
    if (!t) return;
    // rotation stored as quat, expose as euler for C# convenience
    auto euler = glm::eulerAngles(t->rotation);
    out[0] = euler.x; out[1] = euler.y; out[2] = euler.z;
}

void GlueTransformSetRotation(void* world_ctx, uint64_t id, float* in) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* t = w.try_get<Transform>(entity);
    if (!t) return;
    t->rotation = glm::quat(glm::vec3(in[0], in[1], in[2]));
}

void GlueTransformGetScale(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* t = w.try_get<Transform>(entity);
    if (!t) return;
    out[0] = t->scale.x; out[1] = t->scale.y; out[2] = t->scale.z;
}

void GlueTransformSetScale(void* world_ctx, uint64_t id, float* in) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* t = w.try_get<Transform>(entity);
    if (!t) return;
    t->scale = {in[0], in[1], in[2]};
}

// -- Input --------------------------------------------------------------------

bool GlueIsKeyPressed(void* world_ctx, int keycode) {
    auto& w = world_from(world_ctx);
    auto* input = w.try_resource<RawInput>();
    if (!input) return false;
    return input->key_pressed(static_cast<KeyCode>(keycode));
}

bool GlueIsMouseButtonPressed(void* world_ctx, int button) {
    auto& w = world_from(world_ctx);
    auto* input = w.try_resource<RawInput>();
    if (!input) return false;
    return input->mouse_button_pressed(static_cast<MouseButton>(button));
}

void GlueGetMousePosition(void* world_ctx, float* out_x, float* out_y) {
    auto& w = world_from(world_ctx);
    auto* input = w.try_resource<RawInput>();
    if (!input) { *out_x = 0; *out_y = 0; return; }
    auto pos = input->mouse_position();
    *out_x = pos.x;
    *out_y = pos.y;
}

// -- Physics ------------------------------------------------------------------

void GluePhysicsApplyForce(void* world_ctx, uint64_t id, float* force) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) return;
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) return;
    (*physics)->apply_force(pb->handle, glm::vec3(force[0], force[1], force[2]));
}

void GluePhysicsApplyImpulse(void* world_ctx, uint64_t id, float* impulse) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) return;
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) return;
    (*physics)->apply_impulse(pb->handle, glm::vec3(impulse[0], impulse[1], impulse[2]));
}

void GluePhysicsApplyTorque(void* world_ctx, uint64_t id, float* torque) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) return;
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) return;
    (*physics)->apply_torque(pb->handle, glm::vec3(torque[0], torque[1], torque[2]));
}

void GluePhysicsSetLinearVelocity(void* world_ctx, uint64_t id, float* vel) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) return;
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) return;
    (*physics)->set_velocity(pb->handle, glm::vec3(vel[0], vel[1], vel[2]));
}

void GluePhysicsGetLinearVelocity(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) { out[0] = out[1] = out[2] = 0; return; }
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) { out[0] = out[1] = out[2] = 0; return; }
    auto v = (*physics)->get_velocity(pb->handle);
    out[0] = v.x; out[1] = v.y; out[2] = v.z;
}

void GluePhysicsSetAngularVelocity(void* world_ctx, uint64_t id, float* vel) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) return;
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) return;
    (*physics)->set_angular_velocity(pb->handle, glm::vec3(vel[0], vel[1], vel[2]));
}

void GluePhysicsGetAngularVelocity(void* world_ctx, uint64_t id, float* out) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) { out[0] = out[1] = out[2] = 0; return; }
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) { out[0] = out[1] = out[2] = 0; return; }
    auto v = (*physics)->get_angular_velocity(pb->handle);
    out[0] = v.x; out[1] = v.y; out[2] = v.z;
}

} // anonymous namespace

// -- Public entry point -------------------------------------------------------

void ScriptGlue::fill(NativeEngineAPI& api, World& world) {
    api.world_context = static_cast<void*>(&world);

    // Logging
    api.Log = GlueLog;

    // Entity
    api.Spawn     = GlueSpawn;
    api.Despawn   = GlueDespawn;
    api.IsAlive   = GlueIsAlive;

    // Component (generic)
    api.HasComponent = GlueHasComponent;

    // Transform shortcuts
    api.TransformGetTranslation = GlueTransformGetTranslation;
    api.TransformSetTranslation = GlueTransformSetTranslation;
    api.TransformGetRotation    = GlueTransformGetRotation;
    api.TransformSetRotation    = GlueTransformSetRotation;
    api.TransformGetScale       = GlueTransformGetScale;
    api.TransformSetScale       = GlueTransformSetScale;

    // Input
    api.IsKeyPressed         = GlueIsKeyPressed;
    api.IsMouseButtonPressed = GlueIsMouseButtonPressed;
    api.GetMousePosition     = GlueGetMousePosition;

    // Physics
    api.PhysicsApplyForce          = GluePhysicsApplyForce;
    api.PhysicsApplyImpulse        = GluePhysicsApplyImpulse;
    api.PhysicsApplyTorque         = GluePhysicsApplyTorque;
    api.PhysicsSetLinearVelocity   = GluePhysicsSetLinearVelocity;
    api.PhysicsGetLinearVelocity   = GluePhysicsGetLinearVelocity;
    api.PhysicsSetAngularVelocity  = GluePhysicsSetAngularVelocity;
    api.PhysicsGetAngularVelocity  = GluePhysicsGetAngularVelocity;
}

} // namespace helios

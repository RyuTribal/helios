#include <helios/script/script_glue.h>
#include <helios/script/script_sound_registry.h>
#include <helios/ecs/world.h>
#include <helios/ecs/time.h>
#include <helios/components/components.h>
#include <helios/input/raw_input.h>
#include <helios/window/windows.h>
#include <helios/serialization/scene_serializer.h>
#include <yaml-cpp/yaml.h>
#include <helios/assets/asset_utils.h>
#include <helios/ecs/hierarchy.h>
#include <helios/assets/importers/audio_importer.h>
#include "interface/physics_plugin.h"
#include "interface/audio_device.h"
#include "script_log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstring>

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
    auto e = entity_from_raw(entity_id);
    if (!w.is_alive(e)) {
        HELIOS_LOG(Script, Warn, "GlueDespawn called on dead entity ({}/{})",
                   e.index, e.generation);
        return;
    }
    w.despawn(e);
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

    if (component_name == "TransformComponent") return w.has<Transform>(e);
    if (component_name == "RigidBody")          return w.has<RigidBody>(e);
    if (component_name == "BoxCollider")        return w.has<BoxCollider>(e);
    if (component_name == "SphereCollider")     return w.has<SphereCollider>(e);
    if (component_name == "Camera")             return w.has<Camera>(e);
    if (component_name == "ActiveCamera")       return w.has<ActiveCamera>(e);
    if (component_name == "PointLight")         return w.has<PointLight>(e);
    if (component_name == "DirectionalLight")   return w.has<DirectionalLight>(e);
    if (component_name == "AudioSource")        return w.has<AudioSource>(e);
    if (component_name == "MeshRenderer")       return w.has<MeshRenderer>(e);
    if (component_name == "Tag")                return w.has<Tag>(e);
    if (component_name == "SceneRoot")          return w.has<SceneRoot>(e);
    if (component_name == "EditorOnly")         return w.has<EditorOnly>(e);

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

// -- Mouse delta / scroll -----------------------------------------------------

void GlueGetMouseDelta(void* world_ctx, float* out_dx, float* out_dy) {
    auto& w = world_from(world_ctx);
    auto* input = w.try_resource<RawInput>();
    if (!input) { *out_dx = 0; *out_dy = 0; return; }
    auto d = input->mouse_delta();
    *out_dx = d.x;
    *out_dy = d.y;
}

float GlueGetScrollDelta(void* world_ctx) {
    auto& w = world_from(world_ctx);
    auto* input = w.try_resource<RawInput>();
    if (!input) return 0.0f;
    return input->scroll_delta();
}

// -- Cursor mode --------------------------------------------------------------

void GlueSetCursorMode(void* world_ctx, int mode) {
    auto& w = world_from(world_ctx);
    auto* windows = w.try_resource<Windows>();
    if (!windows || !windows->has_primary()) return;
    windows->primary().set_cursor_mode(
        mode == 1 ? Window::CursorMode::Captured : Window::CursorMode::Normal);
}

int GlueGetCursorMode(void* world_ctx) {
    auto& w = world_from(world_ctx);
    auto* windows = w.try_resource<Windows>();
    if (!windows || !windows->has_primary()) return 0;
    return windows->primary().cursor_mode() == Window::CursorMode::Captured ? 1 : 0;
}

// -- Physics teleport ---------------------------------------------------------

void GluePhysicsTeleport(void* world_ctx, uint64_t id, float* pos, float* rot) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* pb = w.try_get<physics::PhysicsBody>(entity);
    if (!pb) return;
    auto* physics = w.try_resource<std::unique_ptr<physics::PhysicsWorld>>();
    if (!physics || !*physics) return;
    glm::quat q(rot[3], rot[0], rot[1], rot[2]); // xyzw -> wxyz
    (*physics)->set_transform(pb->handle, glm::vec3(pos[0], pos[1], pos[2]), q);
    (*physics)->set_velocity(pb->handle, glm::vec3{0.0f});
}

// -- Tag name -----------------------------------------------------------------

void GlueGetTagName(void* world_ctx, uint64_t id, char* out_buffer, int buffer_size) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* tag = w.try_get<Tag>(entity);
    if (!tag || buffer_size <= 0) {
        if (buffer_size > 0) out_buffer[0] = '\0';
        return;
    }
    int len = static_cast<int>(tag->name.size());
    if (len >= buffer_size) len = buffer_size - 1;
    std::memcpy(out_buffer, tag->name.c_str(), len);
    out_buffer[len] = '\0';
}

// -- Time ---------------------------------------------------------------------

float GlueGetDeltaTime(void* world_ctx) {
    auto& w = world_from(world_ctx);
    auto* time = w.try_resource<Time>();
    if (!time) return 0.0f;
    return time->delta();
}

// -- Input: single-press detection --------------------------------------------

bool GlueIsKeyJustPressed(void* world_ctx, int keycode) {
    auto& w = world_from(world_ctx);
    auto* input = w.try_resource<RawInput>();
    if (!input) return false;
    return input->key_just_pressed(static_cast<KeyCode>(keycode));
}

// -- Transform: look-at -------------------------------------------------------

void GlueTransformLookAt(void* world_ctx, uint64_t id, float* target, float* up) {
    auto& w = world_from(world_ctx);
    auto entity = entity_from_raw(id);
    auto* t = w.try_get<Transform>(entity);
    if (!t) return;
    glm::vec3 tgt(target[0], target[1], target[2]);
    glm::vec3 u(up[0], up[1], up[2]);
    glm::mat4 look = glm::lookAt(t->position, tgt, u);
    t->rotation = glm::conjugate(glm::quat_cast(look));
}

// -- Audio ----------------------------------------------------------------

uint32_t GlueAudioGetSoundId(void* world_ctx, const char* name) {
    auto& w = world_from(world_ctx);
    auto* registry = w.try_resource<ScriptSoundRegistry>();
    if (!registry) return 0;
    return registry->find(name);
}

void GlueAudioPlaySoundAt(void* world_ctx, uint32_t sound_id,
                           float x, float y, float z) {
    auto& w = world_from(world_ctx);
    auto* registry = w.try_resource<ScriptSoundRegistry>();
    if (!registry) return;
    auto it = registry->sounds.find(sound_id);
    if (it == registry->sounds.end()) return;

    auto* audio = w.try_resource<std::unique_ptr<audio::AudioDevice>>();
    if (!audio || !*audio) return;

    (*audio)->play_at(it->second.data.data(), it->second.data.size(),
                      glm::vec3(x, y, z));
}

// -- Assets ---------------------------------------------------------------

uint64_t GlueAssetLoad(void* world_ctx, const char* path) {
    auto& w = world_from(world_ctx);
    if (!w.has_resource<std::shared_ptr<AssetServer>>()) return 0;
    auto& server = w.resource<std::shared_ptr<AssetServer>>();
    auto handle = server->load_by_extension(path);
    return handle.packed();
}

bool GlueAssetIsLoaded(void* world_ctx, uint64_t handle) {
    auto& w = world_from(world_ctx);
    if (!w.has_resource<std::shared_ptr<AssetServer>>()) return false;
    auto& server = w.resource<std::shared_ptr<AssetServer>>();
    return server->is_loaded(AssetHandle::from_packed(handle));
}

void GlueAudioPlayHandle(void* world_ctx, uint64_t handle,
                          float x, float y, float z, float volume, bool loop) {
    auto& w = world_from(world_ctx);
    auto* audio = w.try_resource<std::unique_ptr<audio::AudioDevice>>();
    if (!audio || !*audio) return;
    if (!w.has_resource<std::shared_ptr<AssetServer>>()) return;

    auto& server = w.resource<std::shared_ptr<AssetServer>>();
    const auto* data = server->get<AudioData>(AssetHandle::from_packed(handle));
    if (!data || data->file_bytes.empty()) return;

    audio::PlayParams params;
    params.volume = volume;
    params.loop = loop;
    (*audio)->play_at(data->file_bytes.data(), data->file_bytes.size(),
                      glm::vec3(x, y, z), params);
}

void GlueAudioPlayFile(void* world_ctx, const char* path,
                       float x, float y, float z, float volume, bool loop) {
    auto& w = world_from(world_ctx);
    auto* audio = w.try_resource<std::unique_ptr<audio::AudioDevice>>();
    if (!audio || !*audio) return;
    if (!w.has_resource<std::shared_ptr<AssetServer>>()) return;

    auto& server = w.resource<std::shared_ptr<AssetServer>>();

    // Async load — cached after first load. If not ready yet, skip this play.
    auto handle = server->load<AudioData>(path);
    if (!handle) return;
    const auto* data = server->get<AudioData>(handle.untyped());
    if (!data || data->file_bytes.empty()) return;

    audio::PlayParams params;
    params.volume = volume;
    params.loop = loop;
    (*audio)->play_at(data->file_bytes.data(), data->file_bytes.size(),
                      glm::vec3(x, y, z), params);
}

void GlueSceneLoad(void* world_ctx, const char* scene_path) {
    auto& world = *static_cast<World*>(world_ctx);
    auto* serializer = world.try_resource<SceneSerializer>();
    if (!serializer) return;

    std::filesystem::path path(scene_path);
    if (path.is_relative() && world.has_resource<std::shared_ptr<AssetServer>>()) {
        path = world.resource<std::shared_ptr<AssetServer>>()->root() / path;
    }
    if (!std::filesystem::exists(path)) return;

    // Despawn existing scenes (screen goes black/skybox during load)
    {
        std::vector<Entity> roots;
        auto q = world.query<const SceneRoot>();
        for (auto [e, sr] : q.with_entity()) roots.push_back(e);
        for (auto e : roots) {
            if (world.is_alive(e)) world.despawn(e);
        }
    }

    // Load entities from YAML + parent under SceneRoot
    serializer->load_scene(world, path);

    // Kick off async mesh loads — meshes appear when ready
    resolve_mesh_paths(world);
}

void GlueSceneInstantiate(void* world_ctx, const char* scene_path) {
    auto& world = *static_cast<World*>(world_ctx);
    auto* serializer = world.try_resource<SceneSerializer>();
    if (!serializer) return;

    std::filesystem::path path(scene_path);
    if (path.is_relative() && world.has_resource<std::shared_ptr<AssetServer>>()) {
        path = world.resource<std::shared_ptr<AssetServer>>()->root() / path;
    }
    if (!std::filesystem::exists(path)) return;

    // Find current active scene root to parent under
    Entity active{};
    {
        auto sq = world.query<const SceneRoot>();
        for (auto [e, sr] : sq.with_entity()) { active = e; break; }
    }

    // Load as sub-scene (creates SceneRoot + children)
    Entity new_root = serializer->load_scene(world, path);

    if (world.is_alive(new_root) && world.is_alive(active))
        set_parent(world, new_root, active);

    resolve_mesh_paths(world);
}

// Preload scene assets. First call parses the YAML and kicks off async loads.
// Subsequent calls just check if everything is loaded. Returns true when ready.
static std::unordered_map<std::string, std::vector<AssetHandle>> s_preload_cache;

bool GlueSceneIsReady(void* world_ctx, const char* scene_path) {
    auto& world = *static_cast<World*>(world_ctx);
    if (!world.has_resource<std::shared_ptr<AssetServer>>()) return true;
    auto& server = world.resource<std::shared_ptr<AssetServer>>();

    std::string key(scene_path);
    auto it = s_preload_cache.find(key);

    if (it == s_preload_cache.end()) {
        // First call — parse scene YAML, extract asset paths, kick off loads
        std::filesystem::path path(scene_path);
        if (path.is_relative()) path = server->root() / path;
        if (!std::filesystem::exists(path)) return true;

        std::vector<AssetHandle> handles;
        YAML::Node root = YAML::LoadFile(path.string());
        if (root["scene"] && root["scene"]["entities"]) {
            for (const auto& entity_node : root["scene"]["entities"]) {
                if (!entity_node["components"]) continue;
                auto comps = entity_node["components"];
                if (comps["MeshRenderer"] && comps["MeshRenderer"]["mesh_path"]) {
                    auto mesh_path = comps["MeshRenderer"]["mesh_path"].as<std::string>();
                    if (!mesh_path.empty()) {
                        auto h = server->load_by_extension(mesh_path);
                        if (h) handles.push_back(h);
                    }
                }
            }
        }
        s_preload_cache[key] = std::move(handles);
        return false; // just kicked off loads
    }

    // Check if all handles are loaded
    for (const auto& h : it->second) {
        if (!server->is_loaded(h)) return false;
    }

    // All loaded — clean up cache entry
    s_preload_cache.erase(it);
    return true;
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

    // Mouse delta / scroll
    api.GetMouseDelta    = GlueGetMouseDelta;
    api.GetScrollDelta   = GlueGetScrollDelta;

    // Cursor mode
    api.SetCursorMode    = GlueSetCursorMode;
    api.GetCursorMode    = GlueGetCursorMode;

    // Physics teleport
    api.PhysicsTeleport  = GluePhysicsTeleport;

    // Tag name
    api.GetTagName       = GlueGetTagName;

    // Time
    api.GetDeltaTime     = GlueGetDeltaTime;

    // Input: single-press
    api.IsKeyJustPressed = GlueIsKeyJustPressed;

    // Transform: look-at
    api.TransformLookAt  = GlueTransformLookAt;

    // Assets
    api.AssetLoad        = GlueAssetLoad;
    api.AssetIsLoaded    = GlueAssetIsLoaded;

    // Audio
    api.AudioGetSoundId  = GlueAudioGetSoundId;
    api.AudioPlaySoundAt = GlueAudioPlaySoundAt;
    api.AudioPlayFile    = GlueAudioPlayFile;
    api.AudioPlayHandle  = GlueAudioPlayHandle;

    // Scene
    api.SceneLoad = GlueSceneLoad;
    api.SceneInstantiate = GlueSceneInstantiate;
    api.SceneIsReady = GlueSceneIsReady;
}

} // namespace helios

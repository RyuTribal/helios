// Helios Engine - PBR Sandbox Demo (3-state version)
//
// Demonstrates the asset-to-GPU pipeline. Users just load a mesh and
// spawn an entity -- the engine handles textures, materials, GPU upload.
//
// Three states:
//   Loading  -- loads assets for the next scene via AssetServer
//   Scene1   -- DamagedHelmet + skybox + physics
//   Scene2   -- Lion + skybox
//
// Press 1 to switch to Scene1, 2 to switch to Scene2.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>
#include <helios/window/window_plugin.h>
#include <helios/input/input_plugin.h>
#include <helios/input/input_map.h>
#include <helios/input/raw_input.h>
#include <helios/render_plugin.h>
#include <helios/forward_plus/forward_plus_plugin.h>
#include <helios/forward_plus/gpu_data.h>
#include <helios/app/game_flow_plugin.h>
#include <helios/app/state.h>
#include <helios/app/state_builder.h>
#include <helios/assets/asset_server.h>
#include <helios/assets/asset_plugin.h>
#include <helios/assets/mesh_asset.h>
#include <helios/assets/handle.h>

// Physics
#include "interface/physics_world.h"
#include "interface/body_types.h"
#include "interface/physics_factory.h"

// Audio
#include "interface/audio_device.h"
#include "interface/audio_types.h"
#include "interface/audio_factory.h"
#include "interface/audio_utils.h"

#include <cmath>
#include <vector>

using namespace helios;

// ============================================================
// Log channels
// ============================================================

HELIOS_DEFINE_LOG_CHANNEL(Game);
HELIOS_DEFINE_LOG_CHANNEL(Scene);
HELIOS_DEFINE_LOG_CHANNEL(Physics);
HELIOS_DEFINE_LOG_CHANNEL(Audio);

// ============================================================
// PhysicsDemo resource (bundles physics + audio state)
// ============================================================

struct PhysicsDemo {
    std::unique_ptr<physics::PhysicsWorld> physics;
    std::unique_ptr<audio::AudioDevice>    audio;
    physics::BodyHandle floor_body  = 0;
    physics::BodyHandle helmet_body = 0;
    std::vector<uint8_t> bounce_wav;
    bool active = false;
};

// ============================================================
// State enum
// ============================================================

enum class SceneState { Loading, Scene1, Scene2 };

/// Which scene we want to transition to next (set by input handling).
struct PendingScene {
    SceneState target = SceneState::Scene1;
    bool pending = false;
};

// ============================================================
// Orbit camera state (stored as a resource)
// ============================================================

struct OrbitCamera {
    float yaw   = 0.0f;       // radians
    float pitch = 0.0f;       // radians
    float distance = 3.0f;
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    float sensitivity = 0.003f;
    float zoom_speed  = 0.3f;
    bool panning = false;
};

// ============================================================
// Systems
// ============================================================

void orbit_camera_system(Res<RawInput> input,
                         ResMut<Windows> windows,
                         ResMut<OrbitCamera> orbit,
                         Query<Transform, With<ActiveCamera>> cameras)
{
    if (!windows->has_primary()) return;
    auto& win = windows->primary();

    bool rmb = input->mouse_button_pressed(MouseButton::Right);

    // Transition: start panning
    if (rmb && !orbit->panning) {
        orbit->panning = true;
        win.set_cursor_mode(Window::CursorMode::Captured);
    }
    // Transition: stop panning
    if (!rmb && orbit->panning) {
        orbit->panning = false;
        win.set_cursor_mode(Window::CursorMode::Normal);
    }

    // Apply mouse delta while panning
    if (orbit->panning) {
        glm::vec2 delta = input->mouse_delta();
        orbit->yaw   += delta.x * orbit->sensitivity;
        orbit->pitch -= delta.y * orbit->sensitivity;

        constexpr float max_pitch = glm::radians(89.0f);
        orbit->pitch = glm::clamp(orbit->pitch, -max_pitch, max_pitch);
    }

    // Scroll zoom — proportional, clamped per tick to avoid jumps
    float scroll = glm::clamp(input->scroll_delta(), -1.0f, 1.0f);
    if (scroll != 0.0f) {
        orbit->distance *= 1.0f - scroll * orbit->zoom_speed;
        orbit->distance = glm::clamp(orbit->distance, 0.2f, 30.0f);
    }

    // Compute camera position from spherical coordinates
    glm::vec3 offset;
    offset.x = orbit->distance * std::cos(orbit->pitch) * std::sin(orbit->yaw);
    offset.y = orbit->distance * std::sin(orbit->pitch);
    offset.z = orbit->distance * std::cos(orbit->pitch) * std::cos(orbit->yaw);

    glm::vec3 cam_pos = orbit->target + offset;

    for (auto [t] : cameras) {
        t.position = cam_pos;
        glm::mat4 look = glm::lookAt(cam_pos, orbit->target, glm::vec3(0, 1, 0));
        t.rotation = glm::conjugate(glm::quat_cast(look));
    }
}


// ============================================================
// Physics update system (runs during Update when demo is active)
// ============================================================

void physics_update_system(ResMut<PhysicsDemo> demo,
                           Res<Time> time,
                           Res<RawInput> input,
                           Query<Transform, const Tag> tagged) {
    if (!demo->active || !demo->physics) return;

    // R key: reset helmet to starting position
    if (input->key_just_pressed(KeyCode::R)) {
        HELIOS_LOG(Physics, Info, "Resetting helmet to starting position");
        demo->physics->destroy_body(demo->helmet_body);

        physics::BodyDesc helmet_desc;
        helmet_desc.type        = physics::BodyType::Dynamic;
        helmet_desc.position    = {0.0f, 3.0f, 0.0f};
        helmet_desc.shape       = physics::SphereShape{1.0f};
        helmet_desc.mass        = 2.0f;
        helmet_desc.restitution = 0.6f;
        demo->helmet_body = demo->physics->create_body(helmet_desc, 2);
    }

    // Step the simulation
    demo->physics->step(time->delta());

    // Read back helmet position and apply to the entity Transform
    auto pos = demo->physics->get_position(demo->helmet_body);
    for (auto [t, tag] : tagged) {
        if (tag.name == "damaged_helmet") {
            t.position = pos;
            // Keep the display rotation from the original spawn
        }
    }

    // Drain contact events and play bounce sounds
    auto contacts = demo->physics->drain_contacts();
    for (auto& contact : contacts) {
        HELIOS_LOG(Physics, Debug, "Contact at ({:.2f}, {:.2f}, {:.2f}) impulse={:.2f}",
                   contact.world_point.x, contact.world_point.y, contact.world_point.z,
                   contact.impulse);

        if (demo->audio && !demo->bounce_wav.empty()) {
            demo->audio->play_at(
                demo->bounce_wav.data(), demo->bounce_wav.size(),
                contact.world_point);
        }
    }

    if (demo->audio) {
        demo->audio->update();
    }
}

// ============================================================
// Forward declarations of states
// ============================================================

class LoadingState;
class Scene1State;
class Scene2State;

// ============================================================
// LoadingState -- loads assets via AssetServer, then transitions
// ============================================================

class LoadingState : public State<SceneState> {
public:
    explicit LoadingState(World& world) : m_world(&world) {
        auto& pending = world.resource<PendingScene>();
        m_target = pending.target;
        pending.pending = false;

        HELIOS_LOG(Scene, Info, "Loading assets for {}...",
                   m_target == SceneState::Scene1 ? "Scene1 (Helmet)" : "Scene2 (Lion)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();

        // Load mesh asset synchronously (importer auto-loads textures + materials)
        if (m_target == SceneState::Scene1) {
            m_mesh_handle = server.load_sync<MeshAsset>(
                "Meshes/damaged_helmet_source_glb/scene.gltf");
        } else {
            m_mesh_handle = server.load_sync<MeshAsset>(
                "Meshes/lion/scene.gltf");
        }

        if (m_mesh_handle) {
            HELIOS_LOG(Scene, Info, "Loaded mesh asset (handle {}/{})",
                       m_mesh_handle.index(), m_mesh_handle.generation());
        } else {
            HELIOS_LOG(Scene, Error, "Failed to load mesh asset");
        }

        m_loaded = true;
        HELIOS_LOG(Scene, Info, "Loading complete for {}",
                   m_target == SceneState::Scene1 ? "Scene1" : "Scene2");
    }

    ~LoadingState() = default;

    static void describe(StateBuilder<LoadingState>& s) {
        s.opaque();
        s.system(&LoadingState::check_complete);
    }

    void check_complete(ResMut<GameFlow<SceneState>> flow) {
        if (m_loaded) {
            if (m_target == SceneState::Scene1) {
                flow->switch_to<Scene1State>();
            } else {
                flow->switch_to<Scene2State>();
            }
        }
    }

private:
    World* m_world = nullptr;
    SceneState m_target = SceneState::Scene1;
    Handle<MeshAsset> m_mesh_handle;
    bool m_loaded = false;
};

// ============================================================
// Scene1State -- DamagedHelmet
// ============================================================

class Scene1State : public State<SceneState> {
public:
    explicit Scene1State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene1: Entering (DamagedHelmet + Physics)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();

        // Find the loaded helmet mesh handle
        m_mesh_handle = server.load_sync<MeshAsset>(
            "Meshes/damaged_helmet_source_glb/scene.gltf");

        // Spawn helmet entity at Y=3 (physics will move it)
        m_helmet = spawn_tracked(world);
        world.add(m_helmet, Transform{
            .position = glm::vec3{0.0f, 3.0f, 0.0f},
            .rotation = glm::quat(glm::vec3(
                glm::radians(90.0f), glm::radians(180.0f), 0.0f))
        });
        world.add(m_helmet, MeshRenderer{m_mesh_handle});
        world.add(m_helmet, Tag{.name = "damaged_helmet"});

        HELIOS_LOG(Scene, Info, "Scene1: Spawned helmet entity");

        // --- Set up physics ---
        auto& demo = world.resource<PhysicsDemo>();
        if (!demo.physics) {
            HELIOS_LOG(Physics, Info, "Creating physics world");
            demo.physics = physics::create_physics_world();
        }

        // Static floor (large box at Y=-2)
        physics::BodyDesc floor_desc;
        floor_desc.type        = physics::BodyType::Static;
        floor_desc.position    = {0.0f, -2.0f, 0.0f};
        floor_desc.shape       = physics::BoxShape{{50.0f, 0.5f, 50.0f}};
        demo.floor_body = demo.physics->create_body(floor_desc, 1);

        // Dynamic helmet sphere at Y=3
        physics::BodyDesc helmet_desc;
        helmet_desc.type        = physics::BodyType::Dynamic;
        helmet_desc.position    = {0.0f, 3.0f, 0.0f};
        helmet_desc.shape       = physics::SphereShape{1.0f};
        helmet_desc.mass        = 2.0f;
        helmet_desc.restitution = 0.6f;
        demo.helmet_body = demo.physics->create_body(helmet_desc, 2);

        HELIOS_LOG(Physics, Info, "Floor body={} helmet body={}", demo.floor_body, demo.helmet_body);

        // --- Set up audio ---
        if (!demo.audio) {
            HELIOS_LOG(Audio, Info, "Creating audio device");
            demo.audio = audio::create_audio_device();
        }
        if (demo.bounce_wav.empty()) {
            demo.bounce_wav = audio::generate_bounce_wav();
            HELIOS_LOG(Audio, Info, "Generated bounce WAV ({} bytes)", demo.bounce_wav.size());
        }

        demo.active = true;
        HELIOS_LOG(Scene, Info, "Scene1: Physics and audio ready. Press R to re-drop helmet.");
    }

    ~Scene1State() {
        HELIOS_LOG(Scene, Info, "Scene1: Exiting (DamagedHelmet)");
        // Clean up physics bodies (keep the world alive for re-entry)
        auto& demo = m_world->resource<PhysicsDemo>();
        if (demo.physics) {
            if (demo.helmet_body) demo.physics->destroy_body(demo.helmet_body);
            if (demo.floor_body)  demo.physics->destroy_body(demo.floor_body);
            demo.helmet_body = 0;
            demo.floor_body  = 0;
        }
        demo.active = false;
        // m_mesh_handle releases automatically via Handle RAII
    }

    static void describe(StateBuilder<Scene1State>& s) {
        s.opaque();
        s.system(&Scene1State::handle_input);
    }

    void handle_input(Res<RawInput> input,
                      ResMut<GameFlow<SceneState>> flow,
                      ResMut<PendingScene> pending) {
        if (input->key_just_pressed(KeyCode::Num2)) {
            HELIOS_LOG(Scene, Info, "Switching to Scene2 (Lion)...");
            pending->target = SceneState::Scene2;
            pending->pending = true;
            flow->switch_to<LoadingState>();
        }
    }

private:
    World* m_world = nullptr;
    Entity m_helmet{};
    Handle<MeshAsset> m_mesh_handle;
};

// ============================================================
// Scene2State -- Lion
// ============================================================

class Scene2State : public State<SceneState> {
public:
    explicit Scene2State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene2: Entering (Lion)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();

        // Find the loaded lion mesh handle
        m_mesh_handle = server.load_sync<MeshAsset>("Meshes/lion/scene.gltf");

        // Spawn lion entity -- rotated to face camera
        m_lion = spawn_tracked(world);
        world.add(m_lion, Transform{
            .position = glm::vec3{0.0f, -0.5f, 0.0f},
            .rotation = glm::quat(glm::vec3(
                glm::radians(0.0f), glm::radians(180.0f), 0.0f)),
            .scale = glm::vec3{0.01f}  // lion model is large, scale down
        });
        world.add(m_lion, MeshRenderer{m_mesh_handle});
        world.add(m_lion, Tag{.name = "lion"});

        HELIOS_LOG(Scene, Info, "Scene2: Spawned lion entity");
    }

    ~Scene2State() {
        HELIOS_LOG(Scene, Info, "Scene2: Exiting (Lion)");
        // m_mesh_handle releases automatically via Handle RAII
    }

    static void describe(StateBuilder<Scene2State>& s) {
        s.opaque();
        s.system(&Scene2State::handle_input);
    }

    void handle_input(Res<RawInput> input,
                      ResMut<GameFlow<SceneState>> flow,
                      ResMut<PendingScene> pending) {
        if (input->key_just_pressed(KeyCode::Num1)) {
            HELIOS_LOG(Scene, Info, "Switching to Scene1 (Helmet)...");
            pending->target = SceneState::Scene1;
            pending->pending = true;
            flow->switch_to<LoadingState>();
        }
    }

private:
    World* m_world = nullptr;
    Entity m_lion{};
    Handle<MeshAsset> m_mesh_handle;
};

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        app.insert_resource(OrbitCamera{});
        app.insert_resource(PhysicsDemo{});
        app.add_system(Schedule::Update, orbit_camera_system, "orbit_camera");
        app.add_system(Schedule::Update, physics_update_system, "physics_update");
        HELIOS_LOG(Game, Info, "GamePlugin initialized");
    }
};

struct ScenePlugin {
    void build(App& app) {
        auto& world = app.world();

        // Camera -- looking at the helmet from the front
        world.spawn(
            Transform{ .position = glm::vec3{0.0f, 0.0f, 3.0f} },
            Camera{ .fov_degrees = 60.0f, .near_plane = 0.1f, .far_plane = 100.0f },
            ActiveCamera{},
            Tag{ .name = "main_camera" });

        // Directional light (sun)
        world.spawn(
            Transform{ .position = glm::vec3{0, 50, 0},
                        .rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f)) },
            DirectionalLight{ .color = glm::vec3{1.0f, 0.95f, 0.8f}, .intensity = 3.0f },
            Tag{ .name = "sun" });

        HELIOS_LOG(Scene, Info, "Scene: camera + sun spawned (no mesh yet -- states handle that)");
    }
};

// ============================================================
// Main
// ============================================================

int main() {
    LogSystem log(LogConfig{
        .enable_file_sink = false,
        .default_level = LogLevel::Debug,
    });

    HELIOS_LOG(Core, Info, "=== Helios PBR Sandbox (Asset Pipeline Demo) ===");

    App app;

    // Engine plugins (order matters: Window -> Render -> ForwardPlus)
    app.add_plugin(WindowPlugin{ .primary_window = WindowDesc{
        .title = "Helios PBR Sandbox",
        .width = 1280,
        .height = 720,
    }});
    app.add_plugin(InputPlugin{});
    app.add_plugin(RenderPlugin{});

    // Asset pipeline -- registers AssetServer + importers
    app.add_plugin(AssetPlugin{AssetPluginConfig{
        .asset_root = HELIOS_DEMO_ASSET_DIR,
        .loader_threads = 0,  // sync-only for this demo
    }});

    // Forward+ rendering pipeline -- creates PBR pipeline, skybox, depth buffer
    app.add_plugin(ForwardPlusPlugin{.config = ForwardPlusConfig{
        .skybox_hdr_path = "Textures/default_skybox.hdr",
    }});

    app.insert_resource(PendingScene{.target = SceneState::Scene1, .pending = true});

    // Game plugins (camera, etc.)
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});

    // State management
    app.add_plugin(GameFlowPlugin<SceneState>{}
        .state<LoadingState>(SceneState::Loading)
        .state<Scene1State>(SceneState::Scene1)
        .state<Scene2State>(SceneState::Scene2)
        .initial<LoadingState>()
    );

    HELIOS_LOG(Core, Info, "All plugins loaded. Starting engine...");
    HELIOS_LOG(Game, Info, "Controls: 1 = Scene1 (Helmet+Physics), 2 = Scene2 (Lion), R = re-drop helmet, RMB = orbit camera");

    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}

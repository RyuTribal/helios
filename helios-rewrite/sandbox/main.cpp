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
#include <helios/scene/scene_manager.h>
#include <helios/scene/scene_plugin.h>

// Physics & Audio plugins
#include "interface/physics_plugin.h"
#include "interface/audio_plugin.h"
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
// Scene-specific physics state (body handles + audio clips)
// The PhysicsWorld and AudioDevice are now provided by plugins.
// ============================================================

struct ScenePhysics {
    physics::BodyHandle floor_body  = 0;
    physics::BodyHandle helmet_body = 0;
    std::vector<uint8_t> bounce_wav;
    bool active = false;
};

// ============================================================
// State enum
// ============================================================

enum class DemoScene { Loading, Scene1, Scene2 };

/// Which scene we want to transition to next (set by input handling).
/// Also carries the SceneHandle created during loading.
struct PendingScene {
    DemoScene target = DemoScene::Scene1;
    bool pending = false;
    SceneHandle scene_handle;  // set by LoadingState, consumed by Scene*State
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
                         Query<Transform, const Tag, With<ActiveCamera>> cameras)
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

    // Compute main camera position from spherical coordinates
    glm::vec3 offset;
    offset.x = orbit->distance * std::cos(orbit->pitch) * std::sin(orbit->yaw);
    offset.y = orbit->distance * std::sin(orbit->pitch);
    offset.z = orbit->distance * std::cos(orbit->pitch) * std::cos(orbit->yaw);

    glm::vec3 cam_pos = orbit->target + offset;

    for (auto [t, tag] : cameras) {
        if (tag.name == "main_camera") {
            t.position = cam_pos;
            glm::mat4 look = glm::lookAt(cam_pos, orbit->target, glm::vec3(0, 1, 0));
            t.rotation = glm::conjugate(glm::quat_cast(look));
        } else if (tag.name == "side_camera") {
            glm::vec3 side_offset;
            side_offset.x = orbit->distance * std::cos(orbit->pitch) * std::sin(orbit->yaw + glm::half_pi<float>());
            side_offset.y = orbit->distance * std::sin(orbit->pitch);
            side_offset.z = orbit->distance * std::cos(orbit->pitch) * std::cos(orbit->yaw + glm::half_pi<float>());
            glm::vec3 side_pos = orbit->target + side_offset;
            t.position = side_pos;
            t.rotation = glm::conjugate(glm::quat_cast(glm::lookAt(side_pos, orbit->target, glm::vec3(0, 1, 0))));
        }
    }
}


// ============================================================
// Physics update system (runs during Update when demo is active)
// Reads back helmet position and plays bounce sounds using
// plugin-provided PhysicsWorld and AudioDevice resources.
// ============================================================

void physics_update_system(ResMut<ScenePhysics> scene,
                           ResMut<std::unique_ptr<physics::PhysicsWorld>> physics,
                           Res<RawInput> input,
                           Query<Transform, const Tag> tagged) {
    if (!scene->active || !*physics) return;

    // R key: reset helmet to starting position
    if (input->key_just_pressed(KeyCode::R)) {
        HELIOS_LOG(Physics, Info, "Resetting helmet to starting position");
        (*physics)->destroy_body(scene->helmet_body);

        physics::BodyDesc helmet_desc;
        helmet_desc.type        = physics::BodyType::Dynamic;
        helmet_desc.position    = {0.0f, 3.0f, 0.0f};
        helmet_desc.shape       = physics::SphereShape{1.0f};
        helmet_desc.mass        = 2.0f;
        helmet_desc.restitution = 0.6f;
        scene->helmet_body = (*physics)->create_body(helmet_desc, 2);
    }

    // Read back helmet position and apply to the entity Transform
    auto pos = (*physics)->get_position(scene->helmet_body);
    for (auto [t, tag] : tagged) {
        if (tag.name == "damaged_helmet") {
            t.position = pos;
        }
    }
}

// Separate system: react to collision events from the physics plugin.
// Any system can read these — decoupled from the physics update.
void on_collision(EventReader<physics::ContactEvent> contacts,
                  Res<ScenePhysics> scene,
                  ResMut<std::unique_ptr<audio::AudioDevice>> audio) {
    for (const auto& c : contacts) {
        if (*audio && !scene->bounce_wav.empty()) {
            (*audio)->play_at(
                scene->bounce_wav.data(), scene->bounce_wav.size(),
                c.world_point);
        }
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

class LoadingState : public State<DemoScene> {
public:
    explicit LoadingState(World& world) : m_world(&world) {
        auto& pending = world.resource<PendingScene>();
        m_target = pending.target;
        pending.pending = false;

        HELIOS_LOG(Scene, Info, "Loading assets for {}...",
                   m_target == DemoScene::Scene1 ? "Scene1 (Helmet)" : "Scene2 (Lion)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();
        auto& scenes = world.resource<SceneManager>();

        // Create scene and add entity blueprints
        m_scene = scenes.create(
            m_target == DemoScene::Scene1 ? "helmet_scene" : "lion_scene");

        if (m_target == DemoScene::Scene1) {
            std::string mesh_path = "Meshes/damaged_helmet_source_glb/scene.gltf";
            scenes.add_asset(m_scene, mesh_path);
            scenes.add_entity_fn(m_scene, "damaged_helmet",
                [mesh_path](World& w, Entity e, AssetServer& s) {
                    auto mesh = s.load_sync<MeshAsset>(mesh_path);
                    w.add(e, Transform{
                        .position = glm::vec3{0.0f, 3.0f, 0.0f},
                        .rotation = glm::quat(glm::vec3(
                            glm::radians(90.0f), glm::radians(180.0f), 0.0f))
                    });
                    w.add(e, MeshRenderer{mesh});
                    w.add(e, Tag{.name = "damaged_helmet"});
                });
        } else {
            std::string mesh_path = "Meshes/lion/scene.gltf";
            scenes.add_asset(m_scene, mesh_path);
            scenes.add_entity_fn(m_scene, "lion",
                [mesh_path](World& w, Entity e, AssetServer& s) {
                    auto mesh = s.load_sync<MeshAsset>(mesh_path);
                    w.add(e, Transform{
                        .position = glm::vec3{0.0f, -0.5f, 0.0f},
                        .rotation = glm::quat(glm::vec3(
                            glm::radians(0.0f), glm::radians(180.0f), 0.0f)),
                        .scale = glm::vec3{0.01f}
                    });
                    w.add(e, MeshRenderer{mesh});
                    w.add(e, Tag{.name = "lion"});
                });
        }

        // Preload assets (sync for this demo since loader_threads=0)
        scenes.preload(m_scene, server);

        // Pass the scene handle to the next state via PendingScene
        pending.scene_handle = m_scene;

        m_loaded = true;
        HELIOS_LOG(Scene, Info, "Loading complete for {}",
                   m_target == DemoScene::Scene1 ? "Scene1" : "Scene2");
    }

    ~LoadingState() = default;

    static void describe(StateBuilder<LoadingState>& s) {
        s.opaque();
        s.system(&LoadingState::check_complete);
    }

    void check_complete(ResMut<GameFlow<DemoScene>> flow) {
        if (m_loaded) {
            if (m_target == DemoScene::Scene1) {
                flow->switch_to<Scene1State>();
            } else {
                flow->switch_to<Scene2State>();
            }
        }
    }

private:
    World* m_world = nullptr;
    DemoScene m_target = DemoScene::Scene1;
    SceneHandle m_scene;
    bool m_loaded = false;
};

// ============================================================
// Scene1State -- DamagedHelmet
// ============================================================

class Scene1State : public State<DemoScene> {
public:
    explicit Scene1State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene1: Entering (DamagedHelmet + Physics)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();
        auto& scenes = world.resource<SceneManager>();

        // Retrieve the scene handle created during loading
        m_scene = world.resource<PendingScene>().scene_handle;

        // Spawn the scene entities
        scenes.spawn(m_scene, world, server);
        HELIOS_LOG(Scene, Info, "Scene1: Spawned scene entities");

        // --- Set up physics bodies (using plugin-provided world) ---
        auto& physics = world.resource<std::unique_ptr<physics::PhysicsWorld>>();
        auto& scene = world.resource<ScenePhysics>();

        // Static floor (large box at Y=-2)
        physics::BodyDesc floor_desc;
        floor_desc.type        = physics::BodyType::Static;
        floor_desc.position    = {0.0f, -2.0f, 0.0f};
        floor_desc.shape       = physics::BoxShape{{50.0f, 0.5f, 50.0f}};
        scene.floor_body = physics->create_body(floor_desc, 1);

        // Dynamic helmet sphere at Y=3
        physics::BodyDesc helmet_desc;
        helmet_desc.type        = physics::BodyType::Dynamic;
        helmet_desc.position    = {0.0f, 3.0f, 0.0f};
        helmet_desc.shape       = physics::SphereShape{1.0f};
        helmet_desc.mass        = 2.0f;
        helmet_desc.restitution = 0.6f;
        scene.helmet_body = physics->create_body(helmet_desc, 2);

        HELIOS_LOG(Physics, Info, "Floor body={} helmet body={}", scene.floor_body, scene.helmet_body);

        // --- Set up audio clip ---
        if (scene.bounce_wav.empty()) {
            scene.bounce_wav = audio::generate_bounce_wav();
            HELIOS_LOG(Audio, Info, "Generated bounce WAV ({} bytes)", scene.bounce_wav.size());
        }

        scene.active = true;
        HELIOS_LOG(Scene, Info, "Scene1: Physics and audio ready. Press R to re-drop helmet.");
    }

    ~Scene1State() {
        HELIOS_LOG(Scene, Info, "Scene1: Exiting (DamagedHelmet)");

        // Despawn scene entities via SceneManager
        auto& scenes = m_world->resource<SceneManager>();
        scenes.despawn(m_scene, *m_world);
        scenes.unload(m_scene);

        // Clean up physics bodies (the world itself lives in the plugin resource)
        auto& physics = m_world->resource<std::unique_ptr<physics::PhysicsWorld>>();
        auto& scene = m_world->resource<ScenePhysics>();
        if (physics) {
            if (scene.helmet_body) physics->destroy_body(scene.helmet_body);
            if (scene.floor_body)  physics->destroy_body(scene.floor_body);
            scene.helmet_body = 0;
            scene.floor_body  = 0;
        }
        scene.active = false;
    }

    static void describe(StateBuilder<Scene1State>& s) {
        s.opaque();
        s.system(&Scene1State::handle_input);
    }

    void handle_input(Res<RawInput> input,
                      ResMut<GameFlow<DemoScene>> flow,
                      ResMut<PendingScene> pending) {
        if (input->key_just_pressed(KeyCode::Num2)) {
            HELIOS_LOG(Scene, Info, "Switching to Scene2 (Lion)...");
            pending->target = DemoScene::Scene2;
            pending->pending = true;
            flow->switch_to<LoadingState>();
        }
    }

private:
    World* m_world = nullptr;
    SceneHandle m_scene;
};

// ============================================================
// Scene2State -- Lion
// ============================================================

class Scene2State : public State<DemoScene> {
public:
    explicit Scene2State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene2: Entering (Lion)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();
        auto& scenes = world.resource<SceneManager>();

        // Retrieve the scene handle created during loading
        m_scene = world.resource<PendingScene>().scene_handle;

        // Spawn the scene entities
        scenes.spawn(m_scene, world, server);
        HELIOS_LOG(Scene, Info, "Scene2: Spawned scene entities");
    }

    ~Scene2State() {
        HELIOS_LOG(Scene, Info, "Scene2: Exiting (Lion)");

        // Despawn scene entities via SceneManager
        auto& scenes = m_world->resource<SceneManager>();
        scenes.despawn(m_scene, *m_world);
        scenes.unload(m_scene);
    }

    static void describe(StateBuilder<Scene2State>& s) {
        s.opaque();
        s.system(&Scene2State::handle_input);
    }

    void handle_input(Res<RawInput> input,
                      ResMut<GameFlow<DemoScene>> flow,
                      ResMut<PendingScene> pending) {
        if (input->key_just_pressed(KeyCode::Num1)) {
            HELIOS_LOG(Scene, Info, "Switching to Scene1 (Helmet)...");
            pending->target = DemoScene::Scene1;
            pending->pending = true;
            flow->switch_to<LoadingState>();
        }
    }

private:
    World* m_world = nullptr;
    SceneHandle m_scene;
};

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        // Physics & audio backends (create world + device as resources, register step/update systems)
        app.add_plugin(physics::StubPhysicsPlugin{});
        app.add_plugin(audio::StubAudioPlugin{});

        app.insert_resource(OrbitCamera{});
        app.insert_resource(ScenePhysics{});
        app.add_system(Schedule::Update, orbit_camera_system, "orbit_camera");
        app.add_system(Schedule::Update, physics_update_system, "physics_update");
        app.add_system(Schedule::Update, on_collision, "on_collision");
        HELIOS_LOG(Game, Info, "GamePlugin initialized");
    }
};

struct DemoScenePlugin {
    void build(App& app) {
        auto& world = app.world();

        // Left camera (primary) -- covers left half of the window
        world.spawn(
            Transform{ .position = glm::vec3{0.0f, 0.0f, 3.0f} },
            Camera{
                .fov_degrees = 60.0f,
                .near_plane = 0.1f,
                .far_plane = 100.0f,
                .order = 0,
                .viewport_x = 0.0f,
                .viewport_y = 0.0f,
                .viewport_w = 0.5f,
                .viewport_h = 1.0f,
            },
            ActiveCamera{},
            Tag{ .name = "main_camera" });

        // Right camera (side view) -- covers right half, 90-degree offset
        world.spawn(
            Transform{ .position = glm::vec3{3.0f, 0.0f, 0.0f} },
            Camera{
                .fov_degrees = 60.0f,
                .near_plane = 0.1f,
                .far_plane = 100.0f,
                .order = 1,
                .viewport_x = 0.5f,
                .viewport_y = 0.0f,
                .viewport_w = 0.5f,
                .viewport_h = 1.0f,
                .clear_mode = Camera::ClearMode::None,
            },
            ActiveCamera{},
            Tag{ .name = "side_camera" });

        // Directional light (sun)
        world.spawn(
            Transform{ .position = glm::vec3{0, 50, 0},
                        .rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f)) },
            DirectionalLight{ .color = glm::vec3{1.0f, 0.95f, 0.8f}, .intensity = 3.0f },
            Tag{ .name = "sun" });

        HELIOS_LOG(Scene, Info, "Scene: split-screen cameras + sun spawned (no mesh yet -- states handle that)");
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
        .asset_root = "sandbox/assets",
        .loader_threads = 0,  // sync-only for this demo
    }});

    // Forward+ rendering pipeline -- creates PBR pipeline, skybox, depth buffer
    app.add_plugin(ForwardPlusPlugin{.config = ForwardPlusConfig{
        .skybox_hdr_path = "Textures/default_skybox.hdr",
    }});

    app.insert_resource(PendingScene{
        .target = DemoScene::Scene1,
        .pending = true,
        .scene_handle = {}
    });

    // Scene management
    app.add_plugin(SceneManagerPlugin{});

    // Game plugins (camera, etc.)
    app.add_plugin(GamePlugin{});
    app.add_plugin(DemoScenePlugin{});

    // State management
    app.add_plugin(GameFlowPlugin<DemoScene>{}
        .state<LoadingState>(DemoScene::Loading)
        .state<Scene1State>(DemoScene::Scene1)
        .state<Scene2State>(DemoScene::Scene2)
        .initial<LoadingState>()
    );

    HELIOS_LOG(Core, Info, "All plugins loaded. Starting engine...");
    HELIOS_LOG(Game, Info, "Controls: 1 = Scene1 (Helmet+Physics), 2 = Scene2 (Lion), R = re-drop helmet, RMB = orbit camera");

    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}

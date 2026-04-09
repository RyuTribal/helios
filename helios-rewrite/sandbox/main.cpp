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
#include <helios/input/raw_input.h>
#include <helios/render_plugin.h>
#include <helios/forward_plus/forward_plus_plugin.h>
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
#include "interface/physics_factory.h"
#include "interface/audio_factory.h"
#include "interface/audio_utils.h"

// Scripting
#include <helios/script/scripting_plugin.h>
#include <helios/script/script_instance.h>

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

struct SceneAudio {
    std::vector<uint8_t> bounce_wav;
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
// Systems
// ============================================================

// Separate system: react to collision events from the physics plugin.
// Any system can read these — decoupled from the physics update.
void on_collision(EventReader<physics::ContactEvent> contacts,
                  Res<SceneAudio> scene_audio,
                  ResMut<std::unique_ptr<audio::AudioDevice>> audio) {
    for (const auto& c : contacts) {
        if (*audio && !scene_audio->bounce_wav.empty()) {
            (*audio)->play_at(
                scene_audio->bounce_wav.data(), scene_audio->bounce_wav.size(),
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
                    // Attach C# script for rotation
                    w.add(e, ScriptInstance{
                        .script_class_name = "SandboxScripts.HelmetRotator",
                    });
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

        // --- Add physics components to scene entities ---
        // The physics plugin auto-creates bodies from RigidBody + Collider.
        auto q = world.query<const Tag>();
        for (Entity e : scenes.spawned_entities(m_scene)) {
            auto result = q.get(e);
            if (result.has_value()) {
                const auto& [tag] = *result;
                if (tag.name == "damaged_helmet") {
                    world.add(e, RigidBody{
                        .body_type   = BodyType::Dynamic,
                        .handle      = {},
                        .mass        = 2.0f,
                        .restitution = 0.6f,
                    });
                    world.add(e, physics::Collider{
                        .shape = physics::SphereShape{1.0f}
                    });
                    HELIOS_LOG(Physics, Info, "Added RigidBody + Collider to helmet entity");
                }
            }
        }

        // Spawn a static floor entity with physics components
        m_floor = world.spawn(
            Transform{.position = {0.0f, -2.0f, 0.0f}},
            RigidBody{.body_type = BodyType::Static, .handle = {}},
            physics::Collider{.shape = physics::BoxShape{{50.0f, 0.5f, 50.0f}}},
            Tag{.name = "floor"}
        );
        HELIOS_LOG(Physics, Info, "Spawned floor entity with physics components");

        // --- Set up audio clip ---
        auto& scene_audio = world.resource<SceneAudio>();
        if (scene_audio.bounce_wav.empty()) {
            scene_audio.bounce_wav = audio::generate_bounce_wav();
            HELIOS_LOG(Audio, Info, "Generated bounce WAV ({} bytes)", scene_audio.bounce_wav.size());
        }

        HELIOS_LOG(Scene, Info, "Scene1: Physics components attached. Press R to re-drop helmet.");
    }

    ~Scene1State() {
        HELIOS_LOG(Scene, Info, "Scene1: Exiting (DamagedHelmet)");

        // Despawn scene entities via SceneManager (auto-destroy cleans up bodies)
        auto& scenes = m_world->resource<SceneManager>();
        scenes.despawn(m_scene, *m_world);
        scenes.unload(m_scene);

        // Despawn the floor entity (auto-destroy cleans up its body)
        if (m_world->is_alive(m_floor)) {
            m_world->despawn(m_floor);
        }
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
    Entity m_floor;
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
        app.add_plugin(physics::DefaultPhysicsPlugin{});
        app.add_plugin(audio::DefaultAudioPlugin{});

        app.insert_resource(SceneAudio{});
        app.add_system(Schedule::Update, on_collision, "on_collision");
        HELIOS_LOG(Game, Info, "GamePlugin initialized");
    }
};

struct DemoScenePlugin {
    void build(App& app) {
        auto& world = app.world();

        // Left camera (primary) -- covers left half of the window
        // Orbit behaviour is driven by the C# OrbitCamera script.
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
            Tag{ .name = "main_camera" },
            ScriptInstance{
                .script_class_name = "SandboxScripts.OrbitCamera",
            });

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
            Tag{ .name = "side_camera" },
            ScriptInstance{
                .script_class_name = "SandboxScripts.SideCamera",
            });

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

    // C# Scripting (ScriptCore + SandboxScripts assemblies)
    {
        namespace fs = std::filesystem;
        // Paths relative to the project root (where cmake runs from)
        fs::path project_root = fs::current_path();
        fs::path script_core_dll = project_root / "ScriptCore/bin/Release/net10.0/ScriptCore.dll";
        fs::path script_core_cfg = project_root / "ScriptCore/ScriptCore.runtimeconfig.json";
        fs::path sandbox_dll = project_root / "sandbox/scripts/bin/Release/net10.0/SandboxScripts.dll";
        fs::path watch_dir = project_root / "sandbox/scripts";

        if (fs::exists(script_core_dll) && fs::exists(script_core_cfg)) {
            app.add_plugin(ScriptingPlugin{ScriptingPluginConfig{
                .runtime_config_path = script_core_cfg,
                .script_core_dll_path = script_core_dll,
                .app_assembly_path = fs::exists(sandbox_dll) ? sandbox_dll : fs::path{},
                .watch_directory = watch_dir,
            }});
            HELIOS_LOG(Game, Info, "ScriptingPlugin loaded (ScriptCore: {}, App: {})",
                       script_core_dll.string(),
                       fs::exists(sandbox_dll) ? sandbox_dll.string() : "<none>");
        } else {
            HELIOS_LOG(Game, Warn, "ScriptCore.dll not found at {} -- scripting disabled",
                       script_core_dll.string());
        }
    }

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
    HELIOS_LOG(Game, Info, "Controls: 1 = Scene1, 2 = Scene2, R = reset, E = lift, RMB = orbit, Scroll = zoom");

    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}

// Helios Engine - User Sandbox
// Demonstrates the full engine pipeline: ECS + App + Plugins + ForwardPlus rendering.
// Uses ONLY abstract interfaces — no backend-specific headers.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>
#include <helios/window/window_plugin.h>
#include <helios/window/window_events.h>
#include <helios/input/input_plugin.h>
#include <helios/input/input_map.h>
#include <helios/input/raw_input.h>
#include <helios/render_plugin.h>
#include <helios/forward_plus/forward_plus_plugin.h>
#include <helios/graph/frame_packet.h>

#include <cmath>

using namespace helios;

// ============================================================
// Log channels
// ============================================================

HELIOS_DEFINE_LOG_CHANNEL(Game);
HELIOS_DEFINE_LOG_CHANNEL(Scene);

// ============================================================
// Game components
// ============================================================

struct Velocity { glm::vec3 value{0.0f}; };
struct Health { float current = 100.0f; float max = 100.0f; };
struct Enemy { float speed = 5.0f; };
struct Player {};

// ============================================================
// Systems
// ============================================================

void player_movement(Query<Transform, With<Player>> query, Res<InputMap> input, Res<Time> time) {
    float move_x = input->axis_value("move_x");
    float move_z = input->axis_value("move_z");
    float speed = 10.0f;

    for (auto [transform] : query) {
        transform.position.x += move_x * speed * time->delta();
        transform.position.z += move_z * speed * time->delta();
    }
}

void enemy_orbit(Query<Transform, const Enemy> query, Res<Time> time) {
    for (auto [transform, enemy] : query) {
        float t = time->elapsed() * enemy.speed * 0.1f;
        transform.position.x += std::cos(t) * 0.01f;
        transform.position.z += std::sin(t) * 0.01f;
    }
}

void log_frame_packet(Res<renderer::FramePacket> packet, Res<Time> time) {
    // Log every 60 frames to show extraction is working
    if (time->frame_count() % 60 == 0 && time->frame_count() > 0) {
        HELIOS_LOG(Game, Debug, "Frame {} | FramePacket: {} meshes, {} point lights, {} dir lights | dt={:.3f}ms",
            time->frame_count(),
            packet->mesh_draws.size(),
            packet->point_lights.size(),
            packet->dir_lights.size(),
            time->delta() * 1000.0f);
    }
}

void handle_resize(EventReader<WindowResized> events) {
    for (const auto& e : events) {
        HELIOS_LOG(Game, Debug, "Window resized: {}x{}", e.width, e.height);
    }
}

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        auto& input_map = app.world().resource<InputMap>();
        input_map.action("jump", KeyCode::Space);
        input_map.action("fire", MouseButton::Left);
        input_map.axis("move_x", KeyCode::D, KeyCode::A);
        input_map.axis("move_z", KeyCode::W, KeyCode::S);

        app.add_system(Schedule::Update, player_movement, "player_movement");
        app.add_system(Schedule::Update, enemy_orbit, "enemy_orbit");
        app.add_system(Schedule::PostUpdate, log_frame_packet, "log_frame_packet");
        app.add_system(Schedule::PreUpdate, handle_resize, "handle_resize");

        HELIOS_LOG(Game, Info, "GamePlugin: WASD movement, Space jump, LMB fire");
    }
};

struct ScenePlugin {
    void build(App& app) {
        auto& world = app.world();

        // Camera
        world.spawn(
            Transform{ .position = glm::vec3{0, 5, 10} },
            Camera{ .fov_degrees = 60.0f },
            ActiveCamera{},
            Tag{ .name = "main_camera" });

        // Player
        world.spawn(
            Transform{ .position = glm::vec3{0, 0, 0} },
            MeshRenderer{ .mesh = AssetHandle{1}, .material = AssetHandle{1} },
            Player{},
            Tag{ .name = "player" });

        // Directional light (sun)
        world.spawn(
            Transform{ .position = glm::vec3{0, 50, 0},
                        .rotation = glm::quat(glm::vec3(glm::radians(-45.0f), 0, 0)) },
            DirectionalLight{ .color = glm::vec3{1.0f, 0.95f, 0.8f}, .intensity = 1.5f },
            Tag{ .name = "sun" });

        // Point lights
        for (int i = 0; i < 4; i++) {
            float angle = (float)i / 4.0f * 6.28318f;
            world.spawn(
                Transform{ .position = glm::vec3{std::cos(angle) * 5.0f, 2, std::sin(angle) * 5.0f} },
                PointLight{ .color = glm::vec3{1, 0.8f, 0.5f}, .intensity = 3.0f, .radius = 15.0f },
                Tag{ .name = std::string("light_") + std::to_string(i) });
        }

        // Enemies with meshes
        for (int i = 0; i < 10; i++) {
            float angle = (float)i / 10.0f * 6.28318f;
            world.spawn(
                Transform{ .position = glm::vec3{std::cos(angle) * 12.0f, 0, std::sin(angle) * 12.0f} },
                MeshRenderer{ .mesh = AssetHandle{2}, .material = AssetHandle{2} },
                Enemy{ .speed = 1.0f + (float)i * 0.5f },
                Tag{ .name = std::string("enemy_") + std::to_string(i) });
        }

        // Ground plane
        world.spawn(
            Transform{ .position = glm::vec3{0, -0.5f, 0}, .scale = glm::vec3{50, 0.1f, 50} },
            MeshRenderer{ .mesh = AssetHandle{3}, .material = AssetHandle{3} },
            Tag{ .name = "ground" });

        HELIOS_LOG(Scene, Info, "Scene: camera + player + sun + 4 point lights + 10 enemies + ground");
    }
};

// ============================================================
// Main — clean plugin-based setup, engine handles everything
// ============================================================

int main() {
    LogSystem log(LogConfig{
        .enable_file_sink = false,
        .default_level = LogLevel::Debug,
    });

    HELIOS_LOG(Core, Info, "=== Helios Engine Sandbox ===");

    App app;

    // Engine plugins (order matters: Window → Render → ForwardPlus)
    app.add_plugin(WindowPlugin{ .primary_window = WindowDesc{
        .title = "Helios Sandbox",
        .width = 1280,
        .height = 720,
    }});
    app.add_plugin(InputPlugin{});
    app.add_plugin(RenderPlugin{});          // GPU infrastructure (device, swapchain, present)
    app.add_plugin(ForwardPlusPlugin{});     // shading model (just the render graph passes)

    // Game plugins
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});

    HELIOS_LOG(Core, Info, "All plugins loaded. Starting engine...");
    HELIOS_LOG(Core, Info, "ForwardPlus pipeline: extract → depth → shadow → light cull → forward → skybox → tonemap");
    HELIOS_LOG(Core, Info, "(Render passes are stubs until pipeline initialization is complete)");

    // Run — engine handles everything: window events, input, ECS, rendering
    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}

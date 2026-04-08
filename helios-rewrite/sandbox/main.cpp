// Helios Engine - User Sandbox
// Demonstrates: ECS, App, Plugins, Scheduler, Logging, Window, Input

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>
#include <helios/window/window_plugin.h>
#include <helios/window/window_events.h>
#include <helios/input/input_plugin.h>
#include <helios/input/input_map.h>
#include <helios/input/raw_input.h>

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

void enemy_movement(Query<Transform, const Enemy> query, Res<Time> time) {
    for (auto [transform, enemy] : query) {
        // Simple orbit
        float t = time->elapsed() * enemy.speed * 0.1f;
        transform.position.x += std::cos(t) * 0.01f;
        transform.position.z += std::sin(t) * 0.01f;
    }
}

void handle_quit(Res<RawInput> input) {
    if (input->key_just_pressed(KeyCode::Escape)) {
        HELIOS_LOG(Game, Info, "Escape pressed — quitting");
        // In real usage: send AppExit event
    }
}

void handle_resize(EventReader<WindowResized> events) {
    for (const auto& e : events) {
        HELIOS_LOG(Game, Info, "Window resized: {}x{}", e.width, e.height);
    }
}

void log_frame(Res<Time> time) {
    if (time->frame_count() % 60 == 0 && time->frame_count() > 0) {
        HELIOS_LOG(Game, Debug, "Frame {} | dt={:.3f}ms | elapsed={:.1f}s",
            time->frame_count(), time->delta() * 1000.0f, time->elapsed());
    }
}

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        // Setup input bindings
        auto& input_map = app.world().resource<InputMap>();
        input_map.action("jump", KeyCode::Space);
        input_map.action("fire", MouseButton::Left);
        input_map.axis("move_x", KeyCode::D, KeyCode::A);
        input_map.axis("move_z", KeyCode::W, KeyCode::S);

        app.add_system(Schedule::Update, player_movement, "player_movement");
        app.add_system(Schedule::Update, enemy_movement, "enemy_movement");
        app.add_system(Schedule::Update, handle_quit, "handle_quit");
        app.add_system(Schedule::PreUpdate, handle_resize, "handle_resize");
        app.add_system(Schedule::PostUpdate, log_frame, "log_frame");

        HELIOS_LOG(Game, Info, "GamePlugin loaded: WASD movement, Space jump, LMB fire");
    }
};

struct ScenePlugin {
    void build(App& app) {
        auto& world = app.world();

        world.spawn(
            Transform{ .position = glm::vec3{0, 5, -10} },
            Camera{}, ActiveCamera{}, Tag{ .name = "camera" });

        world.spawn(
            Transform{ .position = glm::vec3{0, 0, 0} },
            Player{}, Tag{ .name = "player" });

        for (int i = 0; i < 5; i++) {
            float angle = (float)i / 5.0f * 6.28318f;
            world.spawn(
                Transform{ .position = glm::vec3{
                    std::cos(angle) * 8.0f, 0, std::sin(angle) * 8.0f} },
                Enemy{ .speed = 1.0f + (float)i },
                Tag{ .name = std::string("enemy_") + std::to_string(i) });
        }

        HELIOS_LOG(Scene, Info, "Scene loaded: camera + player + 5 enemies");
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

    HELIOS_LOG(Core, Info, "=== Helios Engine Sandbox ===");

    App app;

    // Engine plugins
    app.add_plugin(WindowPlugin{ .primary_window = WindowDesc{
        .title = "Helios Sandbox",
        .width = 1280,
        .height = 720,
    }});
    app.add_plugin(InputPlugin{});

    // Game plugins
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});

    // Run the actual game loop (with real window!)
    HELIOS_LOG(Core, Info, "Starting game loop (close window or press Escape to quit)");
    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}

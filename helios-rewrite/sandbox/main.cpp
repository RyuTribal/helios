// Helios Engine - User Sandbox
// Demonstrates the full ECS + App + Plugin + Scheduler + Logging from Plans 1-2.
// This is how a real game would use the engine.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>

#include <cmath>

using namespace helios;

// ============================================================
// Define log channels for our game subsystems
// ============================================================

HELIOS_DEFINE_LOG_CHANNEL(Game);
HELIOS_DEFINE_LOG_CHANNEL(Scene);
HELIOS_DEFINE_LOG_CHANNEL(Physics);
HELIOS_DEFINE_LOG_CHANNEL(Combat);

// ============================================================
// Game-specific components (aggregates, no constructors)
// ============================================================

struct Velocity { glm::vec3 value{0.0f}; };
struct Health { float current = 100.0f; float max = 100.0f; };
struct Enemy { float speed = 5.0f; };
struct GameConfig { float gravity = -9.81f; float arena_radius = 50.0f; };
struct FrameCounter { int count = 0; };

// ============================================================
// Systems (free functions — scheduler auto-detects data access)
// ============================================================

void movement_system(Query<Transform, const Velocity> query, Res<Time> time) {
    for (auto [transform, velocity] : query) {
        transform.position += velocity.value * time->delta();
    }
}

void gravity_system(Query<Velocity, Without<Enemy>> query, Res<GameConfig> config, Res<Time> time) {
    for (auto [velocity] : query) {
        velocity.value.y += config->gravity * time->delta();
    }
}

void count_frames(ResMut<FrameCounter> counter) {
    counter->count++;
}

void print_status(Query<const Transform, const Tag> query, Res<Time> time, Res<FrameCounter> counter) {
    HELIOS_LOG(Game, Debug, "Frame {} (dt={:.4f}, elapsed={:.3f})",
        counter->count, time->delta(), time->elapsed());

    for (auto [t, tag] : query) {
        HELIOS_LOG(Game, Trace, "  {} at ({:.2f}, {:.2f}, {:.2f})",
            tag.name, t.position.x, t.position.y, t.position.z);
    }
}

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        HELIOS_LOG(Game, Info, "GamePlugin loading");

        app.insert_resource(GameConfig{});
        app.insert_resource(FrameCounter{});

        app.add_system(Schedule::Update, movement_system, "movement");
        app.add_system(Schedule::Update, gravity_system, "gravity");
        app.add_system(Schedule::Update, count_frames, "count_frames");
        app.add_system(Schedule::PostUpdate, print_status, "print_status");

        HELIOS_LOG(Game, Info, "GamePlugin loaded: 4 systems registered");
    }
};

struct ScenePlugin {
    void build(App& app) {
        HELIOS_LOG(Scene, Info, "ScenePlugin loading");
        auto& world = app.world();

        // Camera
        world.spawn(
            Transform{ .position = glm::vec3{0, 5, -10} },
            Camera{}, ActiveCamera{}, Tag{ .name = "camera" });
        HELIOS_LOG(Scene, Debug, "Spawned camera at (0, 5, -10)");

        // Player
        world.spawn(
            Transform{ .position = glm::vec3{0, 1, 0} },
            Velocity{ .value = glm::vec3{1.0f, 0, 0} },
            Health{ .current = 100, .max = 100 },
            Tag{ .name = "player" });
        HELIOS_LOG(Scene, Debug, "Spawned player at (0, 1, 0)");

        // Enemies
        for (int i = 0; i < 3; i++) {
            float angle = (float)i / 3.0f * 6.28318f;
            float x = std::cos(angle) * 8.0f;
            float z = std::sin(angle) * 8.0f;
            world.spawn(
                Transform{ .position = glm::vec3{x, 1, z} },
                Velocity{},
                Health{ .current = 50, .max = 50 },
                Enemy{ .speed = 3.0f },
                Tag{ .name = std::string("enemy_") + std::to_string(i) });
            HELIOS_LOG(Scene, Trace, "Spawned enemy_{} at ({:.1f}, 1, {:.1f})", i, x, z);
        }

        HELIOS_LOG(Scene, Info, "ScenePlugin loaded: camera + player + 3 enemies");
    }
};

// ============================================================
// Main
// ============================================================

int main() {
    // --- Initialize logging (RAII — destructor flushes) ---
    LogSystem log(LogConfig{
        .log_directory = "logs",
        .enable_file_sink = false,          // no file for sandbox demo
        .enable_ring_buffer_sink = true,
        .default_level = LogLevel::Trace,   // show everything
    });

    HELIOS_LOG(Core, Info, "=== Helios Engine Sandbox (Plan 1 + 2) ===");

    // --- Also demonstrate standalone logging (power layer) ---
    auto ring = std::make_shared<RingBufferSink>(16);
    LogChannel combat_log("Combat", LogLevel::Debug, {ring});
    combat_log.log(LogLevel::Info, "Combat system initialized with custom ring buffer sink");

    // --- App setup ---
    App app;
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});
    app.enable_parallel(2);

    HELIOS_LOG(Core, Info, "Running 5 frames...");

    // --- Game loop (5 frames) ---
    for (int i = 0; i < 5; i++) {
        app.tick();

        // Simulate combat logging (standalone channel)
        if (i == 2) {
            combat_log.log(LogLevel::Warn, "Enemy_0 took 25 damage!");
            combat_log.log(LogLevel::Info, "Player gained 10 XP");
        }
    }

    // --- Query standalone ring buffer ---
    HELIOS_LOG(Core, Info, "--- Combat log (from standalone ring buffer) ---");
    for (const auto& msg : ring->get_messages()) {
        HELIOS_LOG(Core, Info, "  [combat] {}", msg);
    }

    // --- Final state ---
    HELIOS_LOG(Core, Info, "--- Final state ---");
    auto& world = app.world();
    auto enemies = world.query<const Health, const Tag, With<Enemy>>();
    HELIOS_LOG(Core, Info, "Enemies alive: {}", enemies.count());
    for (auto [health, tag] : enemies) {
        HELIOS_LOG(Core, Info, "  {}: {:.0f}/{:.0f} hp", tag.name, health.current, health.max);
    }

    // --- Demonstrate runtime filter change ---
    HELIOS_LOG(Core, Info, "--- Changing log level to Warn (suppresses Info/Debug/Trace) ---");
    log.set_global_level(LogLevel::Warn);
    HELIOS_LOG(Core, Info, "This message should NOT appear (Info < Warn)");
    HELIOS_LOG(Core, Warn, "This message SHOULD appear (Warn >= Warn)");

    HELIOS_LOG(Core, Info, "=== Sandbox complete ==="); // suppressed
    return 0;
} // LogSystem destructor flushes all sinks

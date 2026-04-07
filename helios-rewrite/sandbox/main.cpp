// Helios Engine - User Sandbox
// Demonstrates the full ECS + App + Plugin + Scheduler from Plans 1-2.
// This is how a real game would use the engine.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>

#include <cstdio>
#include <cmath>

using namespace helios;

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
    if (counter->count % 1 == 0) { // every frame for demo
        std::printf("  Frame %d (dt=%.4f, elapsed=%.3f):\n",
            counter->count, time->delta(), time->elapsed());
        for (auto [t, tag] : query) {
            std::printf("    %s at (%.2f, %.2f, %.2f)\n",
                tag.name.c_str(), t.position.x, t.position.y, t.position.z);
        }
    }
}

// ============================================================
// Plugin (groups related systems + resources)
// ============================================================

struct GamePlugin {
    void build(App& app) {
        app.insert_resource(GameConfig{});
        app.insert_resource(FrameCounter{});

        app.add_system(Schedule::Update, movement_system, "movement");
        app.add_system(Schedule::Update, gravity_system, "gravity");
        app.add_system(Schedule::Update, count_frames, "count_frames");
        app.add_system(Schedule::PostUpdate, print_status, "print_status");
    }
};

struct ScenePlugin {
    void build(App& app) {
        // Spawn scene directly (Commands via system params don't auto-apply yet —
        // that's a scheduler gap to fix. For now, spawn via World directly.)
        auto& world = app.world();

        world.spawn(
            Transform{ .position = glm::vec3{0, 5, -10} },
            Camera{}, ActiveCamera{}, Tag{ .name = "camera" });

        world.spawn(
            Transform{ .position = glm::vec3{0, 1, 0} },
            Velocity{ .value = glm::vec3{1.0f, 0, 0} },
            Health{ .current = 100, .max = 100 },
            Tag{ .name = "player" });

        for (int i = 0; i < 3; i++) {
            float angle = (float)i / 3.0f * 6.28318f;
            world.spawn(
                Transform{ .position = glm::vec3{
                    std::cos(angle) * 8.0f, 1, std::sin(angle) * 8.0f} },
                Velocity{},
                Health{ .current = 50, .max = 50 },
                Enemy{ .speed = 3.0f },
                Tag{ .name = std::string("enemy_") + std::to_string(i) });
        }

        std::printf("  [Setup] Scene spawned: camera + player + 3 enemies\n\n");
    }
};

// Quit after N frames (for demo purposes)
struct QuitAfterPlugin {
    int max_frames = 5;

    void build(App& app) {
        // In a real game, you'd send an AppExit event to quit.
        // For the sandbox, we use tick() manually instead of run().
    }
};

// ============================================================
// Main
// ============================================================

int main() {
    std::printf("=== Helios Engine Sandbox (Plan 1 + 2) ===\n\n");

    App app;

    // Register plugins (order doesn't matter — scheduler handles dependencies)
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});

    // Enable parallel system execution
    app.enable_parallel(2);

    std::printf("--- Running 5 frames ---\n\n");

    // Run 5 frames manually (in a real game, app.run() would loop)
    for (int i = 0; i < 5; i++) {
        app.tick();
    }

    // Final query
    std::printf("\n--- Final state ---\n");
    auto& world = app.world();
    auto enemies = world.query<const Health, const Tag, With<Enemy>>();
    std::printf("Enemies alive: %zu\n", enemies.count());
    for (auto [health, tag] : enemies) {
        std::printf("  %s: %.0f/%.0f hp\n", tag.name.c_str(), health.current, health.max);
    }

    std::printf("\n=== Sandbox complete ===\n");
    return 0;
}

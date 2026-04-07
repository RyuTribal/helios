// Helios Engine - User Sandbox
// Simulates a game using the ECS from Plan 1.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>

#include <cstdio>
#include <cmath>

using namespace helios;

// Game-specific resources
struct GameConfig { float gravity = -9.81f; };

// Game-specific components
struct Velocity { glm::vec3 value{0.0f}; };
struct Health { float current = 100.0f; float max = 100.0f; };
struct Enemy { float speed = 5.0f; };

// Systems (free functions)
void movement_system(Query<Transform, const Velocity>& query, const Time& time) {
    for (auto [transform, velocity] : query) {
        transform.position += velocity.value * time.delta();
    }
}

int main() {
    std::printf("=== Helios ECS Sandbox ===\n\n");

    World world;
    world.insert_resource(Time{});  // helios::Time from engine
    world.insert_resource(GameConfig{});

    // --- Spawn scene ---
    std::printf("--- Spawning scene ---\n");

    // Simple entities first
    Entity camera = world.spawn(
        Transform{ .position = glm::vec3{0, 5, -10} },
        Camera{},
        ActiveCamera{}
    );
    std::printf("  Camera spawned\n");

    Entity light = world.spawn(
        Transform{ .position = glm::vec3{0, 10, 0} },
        PointLight{ .intensity = 2.0f }
    );
    std::printf("  Light spawned\n");

    // Player with multiple components
    Entity player = world.spawn(
        Transform{ .position = glm::vec3{0, 1, 0} },
        Velocity{ .value = glm::vec3{2.0f, 0, 0} },
        Health{ .current = 100, .max = 100 },
        Tag{ .name = "player" }
    );
    std::printf("  Player spawned: %s\n", world.get<Tag>(player).name.c_str());

    // Enemies
    for (int i = 0; i < 3; i++) {
        float angle = (float)i / 3.0f * 6.28318f;
        world.spawn(
            Transform{ .position = glm::vec3{std::cos(angle) * 10.0f, 1, std::sin(angle) * 10.0f} },
            Velocity{},
            Health{ .current = 50, .max = 50 },
            Enemy{ .speed = 3.0f },
            Tag{ .name = std::string("enemy_") + std::to_string(i) }
        );
    }
    std::printf("  3 enemies spawned\n");

    // Disabled entity
    world.spawn(
        Transform{ .position = glm::vec3{999, 999, 999} },
        Disabled{},
        Tag{ .name = "hidden" }
    );
    std::printf("  1 disabled entity spawned\n\n");

    // --- Query tests ---
    std::printf("--- Query tests ---\n");
    {
        auto q = world.query<const Transform, const Tag>();
        std::printf("  All tagged entities: %zu\n", q.count());
        for (auto [t, tag] : q) {
            std::printf("    %s at (%.1f, %.1f, %.1f)\n",
                tag.name.c_str(), t.position.x, t.position.y, t.position.z);
        }
    }
    {
        auto q = world.query<const Tag, Without<Disabled>>();
        std::printf("  Tagged entities (not disabled): %zu\n", q.count());
    }
    {
        auto q = world.query<const Transform, With<ActiveCamera>>();
        std::printf("  Active cameras: %zu\n", q.count());
    }
    {
        auto q = world.query<const Health, With<Enemy>>();
        std::printf("  Enemies with health: %zu\n", q.count());
    }
    {
        auto q = world.query<const Transform, Optional<Health>>();
        std::printf("  All transforms (optional health): %zu\n", q.count());
        for (auto [t, hp] : q) {
            if (hp) std::printf("    pos=(%.1f,%.1f,%.1f) hp=%.0f\n", t.position.x, t.position.y, t.position.z, hp->current);
            else std::printf("    pos=(%.1f,%.1f,%.1f) no-health\n", t.position.x, t.position.y, t.position.z);
        }
    }

    // --- Simulate frames ---
    std::printf("\n--- Simulating 3 frames ---\n");
    for (int frame = 0; frame < 3; frame++) {
        auto& time = world.resource<Time>();

        // Run movement system
        auto q = world.query<Transform, const Velocity>();
        movement_system(q, time);

        auto& ppos = world.get<Transform>(player).position;
        std::printf("  Frame %d: player at (%.2f, %.2f, %.2f)\n",
            frame, ppos.x, ppos.y, ppos.z);

        // Time resource is updated by App::tick(); manual sim just advances.
    }

    // --- Commands test ---
    std::printf("\n--- Commands test ---\n");
    {
        Commands cmd(world.entities());

        // Spawn via commands
        Entity bullet = cmd.spawn()
            .insert(Transform{ .position = glm::vec3{0, 2, 0} })
            .insert(Velocity{ .value = glm::vec3{0, 0, 50} })
            .insert(Tag{ .name = "bullet" })
            .id();
        std::printf("  Queued spawn of bullet (id=%u)\n", bullet.index);

        // Despawn an enemy
        cmd.despawn(light);
        std::printf("  Queued despawn of light\n");

        // Apply
        world.apply_commands(cmd);
        std::printf("  Commands applied\n");

        std::printf("  Bullet alive: %s\n", world.is_alive(bullet) ? "yes" : "no");
        std::printf("  Light alive: %s\n", world.is_alive(light) ? "yes" : "no");
    }

    // --- Add/Remove component test ---
    std::printf("\n--- Add/Remove component test ---\n");
    {
        std::printf("  Player has Enemy: %s\n", world.has<Enemy>(player) ? "yes" : "no");
        world.add(player, Enemy{ .speed = 999.0f });
        std::printf("  Added Enemy to player. Has Enemy: %s, speed=%.0f\n",
            world.has<Enemy>(player) ? "yes" : "no",
            world.get<Enemy>(player).speed);

        world.remove<Enemy>(player);
        std::printf("  Removed Enemy from player. Has Enemy: %s\n",
            world.has<Enemy>(player) ? "yes" : "no");
        std::printf("  Player still has Tag: %s\n",
            world.has<Tag>(player) ? "yes" : "no");
    }

    // --- Events test ---
    std::printf("\n--- Events test ---\n");
    {
        struct DamageEvent { float amount; std::string target_name; };
        world.register_event<DamageEvent>();

        auto writer = world.event_writer<DamageEvent>();
        writer.send(DamageEvent{ .amount = 25.0f, .target_name = "enemy_0" });
        writer.send(DamageEvent{ .amount = 50.0f, .target_name = "enemy_1" });
        std::printf("  Sent 2 damage events\n");

        world.swap_event_buffers();

        auto reader = world.event_reader<DamageEvent>();
        for (const auto& e : reader) {
            std::printf("  Received: %.0f damage to %s\n", e.amount, e.target_name.c_str());
        }
    }

    // --- Final state ---
    std::printf("\n--- Final state ---\n");
    {
        auto q = world.query<const Tag, Without<Disabled>>();
        std::printf("  Living tagged entities: %zu\n", q.count());
        for (auto [tag] : q) {
            std::printf("    - %s\n", tag.name.c_str());
        }
    }

    std::printf("\n=== Sandbox complete ===\n");
    return 0;
}

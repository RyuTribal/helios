#include <gtest/gtest.h>
#include "helios/ecs/world.h"
#include "helios/ecs/commands.h"
#include <unordered_set>

using namespace helios;

namespace {

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health   { int hp; };
struct Disabled {};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Spawn / Despawn
// ---------------------------------------------------------------------------

TEST(World, SpawnNoComponents) {
    World world;
    Entity e = world.spawn();
    EXPECT_TRUE(e);
    EXPECT_TRUE(world.is_alive(e));
}

TEST(World, SpawnWithComponents) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3}, Velocity{4, 5, 6});
    EXPECT_TRUE(world.is_alive(e));
    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_TRUE(world.has<Velocity>(e));
    EXPECT_EQ(world.get<Position>(e).x, 1.0f);
    EXPECT_EQ(world.get<Velocity>(e).dx, 4.0f);
}

TEST(World, SpawnMultipleEntities) {
    World world;
    Entity e0 = world.spawn(Position{1, 0, 0});
    Entity e1 = world.spawn(Position{2, 0, 0});
    EXPECT_NE(e0, e1);
    EXPECT_EQ(world.get<Position>(e0).x, 1.0f);
    EXPECT_EQ(world.get<Position>(e1).x, 2.0f);
}

TEST(World, DespawnRemovesEntity) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    world.despawn(e);
    EXPECT_FALSE(world.is_alive(e));
}

TEST(World, DespawnNonexistentIsNoop) {
    World world;
    Entity phantom{999, 1};
    // Should not throw.
    world.despawn(phantom);
}

// ---------------------------------------------------------------------------
// Component access
// ---------------------------------------------------------------------------

TEST(World, GetComponent) {
    World world;
    Entity e = world.spawn(Health{100});
    EXPECT_EQ(world.get<Health>(e).hp, 100);

    // Mutate through get.
    world.get<Health>(e).hp = 50;
    EXPECT_EQ(world.get<Health>(e).hp, 50);
}

TEST(World, TryGetPresent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    Position* p = world.try_get<Position>(e);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->x, 1.0f);
}

TEST(World, TryGetAbsent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    Velocity* v = world.try_get<Velocity>(e);
    EXPECT_EQ(v, nullptr);
}

TEST(World, TryGetDeadEntity) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    world.despawn(e);
    Position* p = world.try_get<Position>(e);
    EXPECT_EQ(p, nullptr);
}

TEST(World, HasComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_FALSE(world.has<Velocity>(e));
}

// ---------------------------------------------------------------------------
// Add / Remove components
// ---------------------------------------------------------------------------

TEST(World, AddComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    EXPECT_FALSE(world.has<Velocity>(e));

    world.add<Velocity>(e, Velocity{4, 5, 6});
    EXPECT_TRUE(world.has<Velocity>(e));
    EXPECT_EQ(world.get<Velocity>(e).dx, 4.0f);

    // Original component should still be there.
    EXPECT_EQ(world.get<Position>(e).x, 1.0f);
}

TEST(World, AddComponentOverwriteExisting) {
    World world;
    Entity e = world.spawn(Health{100});
    world.add<Health>(e, Health{50});
    EXPECT_EQ(world.get<Health>(e).hp, 50);
}

TEST(World, RemoveComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3}, Velocity{4, 5, 6});
    EXPECT_TRUE(world.has<Velocity>(e));

    world.remove<Velocity>(e);
    EXPECT_FALSE(world.has<Velocity>(e));

    // Position should remain.
    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_EQ(world.get<Position>(e).x, 1.0f);
}

TEST(World, RemoveAbsentComponentIsNoop) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    // Should not throw.
    world.remove<Velocity>(e);
    EXPECT_TRUE(world.has<Position>(e));
}

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

TEST(World, Resources) {
    World world;

    struct Gravity { float g; };
    world.insert_resource(Gravity{9.81f});

    EXPECT_TRUE(world.has_resource<Gravity>());
    EXPECT_EQ(world.resource<Gravity>().g, 9.81f);

    // Overwrite.
    world.insert_resource(Gravity{1.62f});
    EXPECT_EQ(world.resource<Gravity>().g, 1.62f);
}

TEST(World, TryResourceAbsent) {
    World world;
    struct Foo { int x; };
    EXPECT_EQ(world.try_resource<Foo>(), nullptr);
    EXPECT_FALSE(world.has_resource<Foo>());
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

TEST(World, Events) {
    World world;

    struct Explosion { float radius; };
    world.register_event<Explosion>();

    auto writer = world.event_writer<Explosion>();
    writer.send(Explosion{10.0f});
    writer.send(Explosion{5.0f});

    // Before swap, read buffer should be empty.
    auto reader = world.event_reader<Explosion>();
    EXPECT_EQ(reader.count(), 0u);

    world.swap_event_buffers();

    // After swap, events should be readable.
    auto reader2 = world.event_reader<Explosion>();
    EXPECT_EQ(reader2.count(), 2u);
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

TEST(World, QueryIntegration) {
    World world;
    world.spawn(Position{1, 0, 0}, Velocity{10, 0, 0});
    world.spawn(Position{2, 0, 0}, Velocity{20, 0, 0});
    world.spawn(Health{100}); // Should not match.

    auto q = world.query<Position, const Velocity>();
    EXPECT_EQ(q.count(), 2u);

    for (auto [pos, vel] : q) {
        pos.x += vel.dx;
    }

    auto q2 = world.query<const Position>();
    std::unordered_set<float> xs;
    for (auto [pos] : q2) {
        xs.insert(pos.x);
    }
    EXPECT_TRUE(xs.count(11.0f));
    EXPECT_TRUE(xs.count(22.0f));
}

// ---------------------------------------------------------------------------
// apply_commands
// ---------------------------------------------------------------------------

TEST(World, ApplyCommands) {
    World world;
    Commands cmds(world.entities());

    auto builder = cmds.spawn();
    Entity e = builder.id();
    builder.insert(Position{1, 2, 3}).insert(Velocity{4, 5, 6});

    world.apply_commands(cmds);

    EXPECT_TRUE(world.is_alive(e));
    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_EQ(world.get<Position>(e).x, 1.0f);
    EXPECT_EQ(world.get<Velocity>(e).dx, 4.0f);
}

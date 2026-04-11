#include <gtest/gtest.h>
#include "helios/ecs/world.h"
#include "helios/ecs/commands.h"

using namespace helios;

namespace {

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health   { int hp; };

} // anonymous namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(Commands, SpawnQueuesCommand) {
    World world;
    Commands cmds(world.entities());

    auto builder = cmds.spawn();
    Entity e = builder.id();

    // Entity is allocated but not yet in the world's archetypes.
    EXPECT_TRUE(world.entities().is_alive(e));
    EXPECT_EQ(cmds.pending_count(), 1u);

    world.apply_commands(cmds);

    // Now the entity should be tracked in the world.
    EXPECT_TRUE(world.is_alive(e));
    EXPECT_EQ(cmds.pending_count(), 0u);
}

TEST(Commands, SpawnWithInsert) {
    World world;
    Commands cmds(world.entities());

    auto builder = cmds.spawn();
    Entity e = builder.id();
    builder.insert(Position{1, 2, 3}).insert(Velocity{4, 5, 6});

    world.apply_commands(cmds);

    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_TRUE(world.has<Velocity>(e));
    EXPECT_EQ(world.get<Position>(e).x, 1.0f);
    EXPECT_EQ(world.get<Velocity>(e).dx, 4.0f);
}

TEST(Commands, DespawnQueuesCommand) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});

    Commands cmds(world.entities());
    cmds.despawn(e);

    // Entity is still alive until commands are applied.
    EXPECT_TRUE(world.is_alive(e));

    world.apply_commands(cmds);
    EXPECT_FALSE(world.is_alive(e));
}

TEST(Commands, InsertComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});

    Commands cmds(world.entities());
    cmds.insert<Velocity>(e, Velocity{4, 5, 6});

    EXPECT_FALSE(world.has<Velocity>(e));

    world.apply_commands(cmds);
    EXPECT_TRUE(world.has<Velocity>(e));
    EXPECT_EQ(world.get<Velocity>(e).dx, 4.0f);
}

TEST(Commands, RemoveComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3}, Velocity{4, 5, 6});

    Commands cmds(world.entities());
    cmds.remove<Velocity>(e);

    EXPECT_TRUE(world.has<Velocity>(e));

    world.apply_commands(cmds);
    EXPECT_FALSE(world.has<Velocity>(e));
    EXPECT_TRUE(world.has<Position>(e));
}

TEST(Commands, InsertResource) {
    World world;

    struct Gravity { float g; };

    Commands cmds(world.entities());
    cmds.insert_resource(Gravity{9.81f});

    EXPECT_FALSE(world.has_resource<Gravity>());

    world.apply_commands(cmds);
    EXPECT_TRUE(world.has_resource<Gravity>());
    EXPECT_EQ(world.resource<Gravity>().g, 9.81f);
}

TEST(Commands, MultipleSpawns) {
    World world;
    Commands cmds(world.entities());

    auto b0 = cmds.spawn();
    Entity e0 = b0.id();
    b0.insert(Position{1, 0, 0});

    auto b1 = cmds.spawn();
    Entity e1 = b1.id();
    b1.insert(Position{2, 0, 0});

    EXPECT_NE(e0, e1);

    world.apply_commands(cmds);

    EXPECT_EQ(world.get<Position>(e0).x, 1.0f);
    EXPECT_EQ(world.get<Position>(e1).x, 2.0f);
}

TEST(Commands, ApplyClearsCommands) {
    World world;
    Commands cmds(world.entities());

    cmds.spawn();
    cmds.spawn();
    EXPECT_EQ(cmds.pending_count(), 2u);

    world.apply_commands(cmds);
    EXPECT_EQ(cmds.pending_count(), 0u);
}

TEST(Commands, SpawnThenDespawn) {
    World world;
    Commands cmds(world.entities());

    auto builder = cmds.spawn();
    Entity e = builder.id();
    builder.insert(Position{1, 2, 3});
    cmds.despawn(e);

    world.apply_commands(cmds);

    // Entity should have been spawned and then despawned.
    EXPECT_FALSE(world.is_alive(e));
}

TEST(Commands, EntityBuilderReturnsValidId) {
    World world;
    Commands cmds(world.entities());

    auto builder = cmds.spawn();
    Entity e = builder.id();

    // The entity id should be valid (non-zero generation).
    EXPECT_TRUE(static_cast<bool>(e));
}

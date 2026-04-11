#include <gtest/gtest.h>

#include "helios/scene/scene_manager.h"
#include "helios/scene/scene_handle.h"
#include "helios/scene/scene_data.h"
#include "helios/ecs/world.h"
#include "helios/assets/asset_server.h"
#include "helios/components/components.h"

#include <filesystem>
#include <fstream>

namespace helios::test {

// Simple component for testing
struct TestComp {
    int value = 0;
};

class SceneManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_test_dir = std::filesystem::temp_directory_path() / "helios_scene_test";
        std::filesystem::create_directories(m_test_dir);
    }

    void TearDown() override {
        std::filesystem::remove_all(m_test_dir);
    }

    std::filesystem::path m_test_dir;
};

// ----- SceneHandle tests -----

TEST(SceneHandleTest, DefaultIsNull) {
    SceneHandle h;
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_EQ(h.index, 0u);
    EXPECT_EQ(h.generation, 0u);
}

TEST(SceneHandleTest, NonZeroIsValid) {
    SceneHandle h{1, 1};
    EXPECT_TRUE(static_cast<bool>(h));
}

TEST(SceneHandleTest, PackedRoundTrip) {
    SceneHandle h{42, 7};
    uint64_t packed = h.packed();
    SceneHandle unpacked = SceneHandle::from_packed(packed);
    EXPECT_EQ(h, unpacked);
}

TEST(SceneHandleTest, Equality) {
    SceneHandle a{1, 1};
    SceneHandle b{1, 1};
    SceneHandle c{2, 1};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(SceneHandleTest, Hashable) {
    std::unordered_map<SceneHandle, int> map;
    SceneHandle h{5, 3};
    map[h] = 99;
    EXPECT_EQ(map[h], 99);
}

// ----- SceneManager create -----

TEST_F(SceneManagerTest, CreateReturnsValidHandle) {
    SceneManager mgr;
    auto h = mgr.create("test_scene");
    EXPECT_TRUE(static_cast<bool>(h));
    EXPECT_TRUE(mgr.is_valid(h));
}

TEST_F(SceneManagerTest, CreateSetsName) {
    SceneManager mgr;
    auto h = mgr.create("my_scene");
    EXPECT_EQ(mgr.name(h), "my_scene");
}

TEST_F(SceneManagerTest, CreateSetsCreatedState) {
    SceneManager mgr;
    auto h = mgr.create("scene");
    EXPECT_EQ(mgr.state(h), SceneState::Created);
}

TEST_F(SceneManagerTest, MultipleCreatesUniqueHandles) {
    SceneManager mgr;
    auto h1 = mgr.create("a");
    auto h2 = mgr.create("b");
    EXPECT_NE(h1, h2);
}

// ----- add_entity + spawn -----

TEST_F(SceneManagerTest, AddEntityAndSpawn) {
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    auto h = mgr.create("test");
    mgr.add_entity(h, "entity1", TestComp{42});

    mgr.spawn(h, world, server);

    EXPECT_EQ(mgr.state(h), SceneState::Spawned);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 1u);

    Entity e = entities[0];
    EXPECT_TRUE(world.is_alive(e));
    EXPECT_TRUE(world.has<TestComp>(e));
    EXPECT_EQ(world.get<TestComp>(e).value, 42);
}

TEST_F(SceneManagerTest, SpawnedEntitiesGetSceneTag) {
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    auto h = mgr.create("tagged_scene");
    mgr.add_entity(h, "e1", TestComp{1});
    mgr.add_entity(h, "e2", TestComp{2});

    mgr.spawn(h, world, server);

    for (const auto& entity : mgr.spawned_entities(h)) {
        ASSERT_TRUE(world.has<SceneTag>(entity));
        EXPECT_EQ(world.get<SceneTag>(entity).scene, h);
    }
}

TEST_F(SceneManagerTest, MultipleComponentsPerEntity) {
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    auto h = mgr.create("multi");
    mgr.add_entity(h, "complex",
                   Transform{.position = glm::vec3{1, 2, 3}},
                   TestComp{99});

    mgr.spawn(h, world, server);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 1u);
    Entity e = entities[0];

    EXPECT_TRUE(world.has<Transform>(e));
    EXPECT_TRUE(world.has<TestComp>(e));
    EXPECT_FLOAT_EQ(world.get<Transform>(e).position.x, 1.0f);
    EXPECT_EQ(world.get<TestComp>(e).value, 99);
}

// ----- despawn -----

TEST_F(SceneManagerTest, DespawnRemovesEntities) {
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    auto h = mgr.create("despawn_test");
    mgr.add_entity(h, "e1", TestComp{1});
    mgr.add_entity(h, "e2", TestComp{2});

    mgr.spawn(h, world, server);
    EXPECT_EQ(mgr.spawned_entities(h).size(), 2u);

    // Capture entity ids before despawn
    auto e1 = mgr.spawned_entities(h)[0];
    auto e2 = mgr.spawned_entities(h)[1];

    mgr.despawn(h, world);

    EXPECT_FALSE(world.is_alive(e1));
    EXPECT_FALSE(world.is_alive(e2));
    EXPECT_TRUE(mgr.spawned_entities(h).empty());
    EXPECT_EQ(mgr.state(h), SceneState::Created);
}

// ----- unload -----

TEST_F(SceneManagerTest, UnloadInvalidatesHandle) {
    SceneManager mgr;
    auto h = mgr.create("unload_test");
    EXPECT_TRUE(mgr.is_valid(h));

    mgr.unload(h);
    EXPECT_FALSE(mgr.is_valid(h));
    EXPECT_EQ(mgr.state(h), SceneState::Unloaded);
}

// ----- add_entity_fn -----

TEST_F(SceneManagerTest, AddEntityFn) {
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    auto h = mgr.create("fn_test");
    mgr.add_entity_fn(h, "custom_entity",
        [](World& w, Entity e, AssetServer& /*s*/) {
            w.add(e, TestComp{777});
        });

    mgr.spawn(h, world, server);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 1u);
    EXPECT_EQ(world.get<TestComp>(entities[0]).value, 777);
}

// ----- Multiple scenes simultaneously -----

TEST_F(SceneManagerTest, MultipleScenesConcurrently) {
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    auto h1 = mgr.create("scene_a");
    auto h2 = mgr.create("scene_b");

    mgr.add_entity(h1, "a1", TestComp{10});
    mgr.add_entity(h2, "b1", TestComp{20});

    mgr.spawn(h1, world, server);
    mgr.spawn(h2, world, server);

    EXPECT_EQ(mgr.spawned_entities(h1).size(), 1u);
    EXPECT_EQ(mgr.spawned_entities(h2).size(), 1u);

    // Despawn only scene_a
    mgr.despawn(h1, world);
    EXPECT_TRUE(mgr.spawned_entities(h1).empty());
    EXPECT_EQ(mgr.spawned_entities(h2).size(), 1u);

    // scene_b entity should still be alive
    auto b_entity = mgr.spawned_entities(h2)[0];
    EXPECT_TRUE(world.is_alive(b_entity));
    EXPECT_EQ(world.get<TestComp>(b_entity).value, 20);
}

// ----- Invalid handle operations are safe -----

TEST_F(SceneManagerTest, InvalidHandleOperationsAreSafe) {
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    SceneHandle invalid{99, 99};

    EXPECT_FALSE(mgr.is_valid(invalid));
    EXPECT_EQ(mgr.state(invalid), SceneState::Unloaded);
    EXPECT_TRUE(mgr.name(invalid).empty());
    EXPECT_TRUE(mgr.spawned_entities(invalid).empty());

    // These should be no-ops, not crashes
    mgr.add_entity(invalid, "nope", TestComp{0});
    mgr.spawn(invalid, world, server);
    mgr.despawn(invalid, world);
    mgr.unload(invalid);
}

} // namespace helios::test

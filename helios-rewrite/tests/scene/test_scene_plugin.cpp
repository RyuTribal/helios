#include <gtest/gtest.h>

#include "helios/scene/scene_plugin.h"
#include "helios/scene/scene_manager.h"
#include "helios/scene/scene_handle.h"
#include "helios/ecs/world.h"
#include "helios/assets/asset_server.h"
#include "helios/components/components.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>

namespace helios::test {

class ScenePluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_test_dir = std::filesystem::temp_directory_path() / "helios_scene_plugin_test";
        std::filesystem::create_directories(m_test_dir);
    }

    void TearDown() override {
        std::filesystem::remove_all(m_test_dir);
    }

    void write_yaml(const std::string& filename, const std::string& content) {
        std::ofstream f(m_test_dir / filename);
        f << content;
    }

    std::filesystem::path m_test_dir;
};

// ----- Component factory tests -----

TEST_F(ScenePluginTest, TransformFactory) {
    SceneManagerPlugin plugin;
    // Build registers factories internally; we test via YAML loading

    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    write_yaml("test_scene.yaml", R"(
scene:
  name: "test"
  entities:
    - name: "obj1"
      components:
        Transform:
          position: [1.0, 2.0, 3.0]
          rotation: [1.0, 0.0, 0.0, 0.0]
          scale: [2.0, 2.0, 2.0]
)");

    // Register builtin factories via plugin build (we need App for full build,
    // so we'll manually trigger factory registration for testing)
    plugin.build_factories_for_test();

    auto h = mgr.create("test");
    load_scene_from_yaml(mgr, h, m_test_dir / "test_scene.yaml",
                         plugin.factories_for_test());

    mgr.spawn(h, world, server);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 1u);
    Entity e = entities[0];

    ASSERT_TRUE(world.has<Transform>(e));
    auto& t = world.get<Transform>(e);
    EXPECT_FLOAT_EQ(t.position.x, 1.0f);
    EXPECT_FLOAT_EQ(t.position.y, 2.0f);
    EXPECT_FLOAT_EQ(t.position.z, 3.0f);
    EXPECT_FLOAT_EQ(t.scale.x, 2.0f);
}

TEST_F(ScenePluginTest, CameraFactory) {
    SceneManagerPlugin plugin;
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    write_yaml("camera.yaml", R"(
scene:
  name: "cam_test"
  entities:
    - name: "main_cam"
      components:
        Camera:
          projection: "Perspective"
          fov_degrees: 90.0
          near_plane: 0.5
          far_plane: 500.0
        ActiveCamera: {}
)");

    plugin.build_factories_for_test();
    auto h = mgr.create("cam_test");
    load_scene_from_yaml(mgr, h, m_test_dir / "camera.yaml",
                         plugin.factories_for_test());

    mgr.spawn(h, world, server);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 1u);
    Entity e = entities[0];

    ASSERT_TRUE(world.has<Camera>(e));
    EXPECT_FLOAT_EQ(world.get<Camera>(e).fov_degrees, 90.0f);
    EXPECT_FLOAT_EQ(world.get<Camera>(e).far_plane, 500.0f);
    EXPECT_TRUE(world.has<ActiveCamera>(e));
}

TEST_F(ScenePluginTest, LightFactory) {
    SceneManagerPlugin plugin;
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    write_yaml("lights.yaml", R"(
scene:
  name: "light_test"
  entities:
    - name: "sun"
      components:
        DirectionalLight:
          color: [1.0, 0.9, 0.8]
          intensity: 3.0
    - name: "lamp"
      components:
        PointLight:
          color: [1.0, 1.0, 0.5]
          intensity: 5.0
          radius: 15.0
)");

    plugin.build_factories_for_test();
    auto h = mgr.create("light_test");
    load_scene_from_yaml(mgr, h, m_test_dir / "lights.yaml",
                         plugin.factories_for_test());

    mgr.spawn(h, world, server);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 2u);

    ASSERT_TRUE(world.has<DirectionalLight>(entities[0]));
    EXPECT_FLOAT_EQ(world.get<DirectionalLight>(entities[0]).intensity, 3.0f);

    ASSERT_TRUE(world.has<PointLight>(entities[1]));
    EXPECT_FLOAT_EQ(world.get<PointLight>(entities[1]).radius, 15.0f);
}

TEST_F(ScenePluginTest, EntityNamesAsTagComponents) {
    SceneManagerPlugin plugin;
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    write_yaml("names.yaml", R"(
scene:
  name: "name_test"
  entities:
    - name: "my_entity"
      components:
        Transform:
          position: [0, 0, 0]
          rotation: [1, 0, 0, 0]
          scale: [1, 1, 1]
)");

    plugin.build_factories_for_test();
    auto h = mgr.create("name_test");
    load_scene_from_yaml(mgr, h, m_test_dir / "names.yaml",
                         plugin.factories_for_test());

    mgr.spawn(h, world, server);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 1u);
    ASSERT_TRUE(world.has<Tag>(entities[0]));
    EXPECT_EQ(world.get<Tag>(entities[0]).name, "my_entity");
}

TEST_F(ScenePluginTest, AssetPathsRegistered) {
    SceneManagerPlugin plugin;
    SceneManager mgr;

    write_yaml("assets.yaml", R"(
scene:
  name: "asset_test"
  assets:
    - "meshes/house.gltf"
    - "meshes/tree.gltf"
  entities: []
)");

    plugin.build_factories_for_test();
    auto h = mgr.create("asset_test");
    load_scene_from_yaml(mgr, h, m_test_dir / "assets.yaml",
                         plugin.factories_for_test());

    // Scene should have the handle created but not yet spawned
    EXPECT_EQ(mgr.state(h), SceneState::Created);
}

TEST_F(ScenePluginTest, UnknownComponentsSkipped) {
    SceneManagerPlugin plugin;
    SceneManager mgr;
    World world;
    AssetServer server(m_test_dir, 0);

    write_yaml("unknown.yaml", R"(
scene:
  name: "skip_test"
  entities:
    - name: "has_unknown"
      components:
        Transform:
          position: [0, 0, 0]
          rotation: [1, 0, 0, 0]
          scale: [1, 1, 1]
        FakeComponent:
          value: 42
)");

    plugin.build_factories_for_test();
    auto h = mgr.create("skip_test");
    load_scene_from_yaml(mgr, h, m_test_dir / "unknown.yaml",
                         plugin.factories_for_test());

    mgr.spawn(h, world, server);

    auto& entities = mgr.spawned_entities(h);
    ASSERT_EQ(entities.size(), 1u);
    EXPECT_TRUE(world.has<Transform>(entities[0]));
    // FakeComponent should have been silently skipped
}

TEST_F(ScenePluginTest, MissingFileThrows) {
    SceneManagerPlugin plugin;
    SceneManager mgr;

    plugin.build_factories_for_test();
    auto h = mgr.create("missing");
    EXPECT_THROW(
        load_scene_from_yaml(mgr, h, m_test_dir / "nonexistent.yaml",
                             plugin.factories_for_test()),
        std::exception);
}

} // namespace helios::test

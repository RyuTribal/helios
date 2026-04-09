// helios-physics/tests/test_physics_plugin.cpp

#include <gtest/gtest.h>
#include <memory>

#include "interface/physics_world.h"
#include "interface/physics_plugin.h"
#include "stub/stub_physics_world.h"

#if HELIOS_HAS_JOLT
#include "jolt/jolt_physics_world.h"
#endif

using namespace helios::physics;

// Minimal mock App for testing plugin build().
// Just records what resources and systems were registered.
struct MockApp {
    template<typename T>
    void insert_resource(T resource) {
        resource_count++;
        if constexpr (std::is_same_v<T, std::unique_ptr<PhysicsWorld>>) {
            physics_world_inserted = (resource != nullptr);
            stored_world = std::move(resource);
        }
        if constexpr (std::is_same_v<T, PhysicsConfig>) {
            config_inserted = true;
            stored_config = resource;
        }
    }

    // Absorb add_system and add_event calls (just count them)
    template<typename... Args>
    MockApp& add_system(Args&&...) { system_count++; return *this; }

    template<typename T>
    void add_event() { event_count++; }

    int resource_count = 0;
    int system_count = 0;
    int event_count = 0;
    bool physics_world_inserted = false;
    bool config_inserted = false;
    std::unique_ptr<PhysicsWorld> stored_world;
    PhysicsConfig stored_config;
};

TEST(PhysicsPlugin, StubBuildInsertsResources) {
    MockApp app;

    PhysicsPlugin<StubPhysicsWorld> plugin;
    plugin.config.gravity = glm::vec3(0.0f, -10.0f, 0.0f);
    plugin.config.fixed_timestep = 1.0f / 120.0f;

    plugin.build(app);

    EXPECT_TRUE(app.config_inserted);
    EXPECT_TRUE(app.physics_world_inserted);
    // 3 resources: PhysicsConfig, std::unique_ptr<PhysicsWorld>, PhysicsBodyMap
    EXPECT_EQ(app.resource_count, 3);

    EXPECT_NEAR(app.stored_config.fixed_timestep, 1.0f / 120.0f, 0.0001f);
    EXPECT_NEAR(app.stored_config.gravity.y, -10.0f, 0.01f);

    // Verify the world is functional
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = SphereShape{0.5f};
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);

    BodyHandle handle = app.stored_world->create_body(desc, 1);
    EXPECT_NE(handle, 0u);
}

TEST(PhysicsPlugin, MultipleStubWorldInstancesWork) {
    PhysicsConfig config;
    auto world1 = std::make_unique<StubPhysicsWorld>(config);
    auto world2 = std::make_unique<StubPhysicsWorld>(config);

    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle h1 = world1->create_body(desc, 1);
    BodyHandle h2 = world2->create_body(desc, 2);

    EXPECT_NE(h1, 0u);
    EXPECT_NE(h2, 0u);

    world1.reset();
    world2.reset();
}

#if HELIOS_HAS_JOLT
TEST(PhysicsPlugin, JoltBuildInsertsResources) {
    MockApp app;

    PhysicsPlugin<JoltPhysicsWorld> plugin;
    plugin.config.gravity = glm::vec3(0.0f, -10.0f, 0.0f);
    plugin.config.fixed_timestep = 1.0f / 120.0f;

    plugin.build(app);

    EXPECT_TRUE(app.config_inserted);
    EXPECT_TRUE(app.physics_world_inserted);
    // 3 resources: PhysicsConfig, std::unique_ptr<PhysicsWorld>, PhysicsBodyMap
    EXPECT_EQ(app.resource_count, 3);

    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = SphereShape{0.5f};
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);

    BodyHandle handle = app.stored_world->create_body(desc, 1);
    EXPECT_NE(handle, 0u);
}

TEST(PhysicsPlugin, MultipleJoltWorldInstancesWork) {
    PhysicsConfig config;
    auto world1 = std::make_unique<JoltPhysicsWorld>(config);
    auto world2 = std::make_unique<JoltPhysicsWorld>(config);

    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle h1 = world1->create_body(desc, 1);
    BodyHandle h2 = world2->create_body(desc, 2);

    EXPECT_NE(h1, 0u);
    EXPECT_NE(h2, 0u);

    world1.reset();
    world2.reset();
}
#endif

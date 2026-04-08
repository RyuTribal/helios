// helios-physics/tests/test_jolt_physics_world.cpp

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "interface/physics_world.h"
#include "jolt/jolt_physics_world.h"

using namespace helios::physics;

class JoltPhysicsWorldTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsConfig config;
        config.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        config.fixed_timestep = 1.0f / 60.0f;
        world = std::make_unique<JoltPhysicsWorld>(config);
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<JoltPhysicsWorld> world;
};

// --- Body creation / destruction ---

TEST_F(JoltPhysicsWorldTest, CreateBodyReturnsNonZeroHandle) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = BoxShape{glm::vec3(0.5f)};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, /*entity_id=*/42);
    EXPECT_NE(handle, 0u);
}

TEST_F(JoltPhysicsWorldTest, DestroyBodyDoesNotCrash) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = SphereShape{0.5f};

    BodyHandle handle = world->create_body(desc, 100);
    EXPECT_NO_THROW(world->destroy_body(handle));
}

TEST_F(JoltPhysicsWorldTest, DestroyInvalidHandleDoesNotCrash) {
    EXPECT_NO_THROW(world->destroy_body(999999));
}

TEST_F(JoltPhysicsWorldTest, GetPositionReturnsCreationPosition) {
    BodyDesc desc;
    desc.type = BodyType::Static;
    desc.position = glm::vec3(1.0f, 2.0f, 3.0f);
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle handle = world->create_body(desc, 10);

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.x, 1.0f, 0.01f);
    EXPECT_NEAR(pos.y, 2.0f, 0.01f);
    EXPECT_NEAR(pos.z, 3.0f, 0.01f);
}

// --- Simulation step ---

TEST_F(JoltPhysicsWorldTest, DynamicBodyFallsUnderGravity) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = SphereShape{0.5f};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, 1);

    for (int i = 0; i < 60; ++i) {
        world->step(1.0f / 60.0f);
    }

    glm::vec3 pos = world->get_position(handle);
    EXPECT_LT(pos.y, 10.0f);
    EXPECT_LT(pos.y, 6.0f);
}

TEST_F(JoltPhysicsWorldTest, StaticBodyDoesNotMove) {
    BodyDesc desc;
    desc.type = BodyType::Static;
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle handle = world->create_body(desc, 2);

    for (int i = 0; i < 60; ++i) {
        world->step(1.0f / 60.0f);
    }

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.y, 5.0f, 0.01f);
}

// --- Contact detection ---

TEST_F(JoltPhysicsWorldTest, ContactDetectedBetweenCollidingBodies) {
    BodyDesc floor_desc;
    floor_desc.type = BodyType::Static;
    floor_desc.position = glm::vec3(0.0f, 0.0f, 0.0f);
    floor_desc.shape = BoxShape{glm::vec3(50.0f, 0.5f, 50.0f)};
    world->create_body(floor_desc, 100);

    BodyDesc sphere_desc;
    sphere_desc.type = BodyType::Dynamic;
    sphere_desc.position = glm::vec3(0.0f, 2.0f, 0.0f);
    sphere_desc.shape = SphereShape{0.5f};
    sphere_desc.mass = 1.0f;
    world->create_body(sphere_desc, 200);

    bool contact_detected = false;
    for (int i = 0; i < 120; ++i) {
        world->step(1.0f / 60.0f);
        auto contacts = world->drain_contacts();
        if (!contacts.empty()) {
            contact_detected = true;
            auto& c = contacts[0];
            bool involves_floor  = (c.entity_a == 100 || c.entity_b == 100);
            bool involves_sphere = (c.entity_a == 200 || c.entity_b == 200);
            EXPECT_TRUE(involves_floor);
            EXPECT_TRUE(involves_sphere);
            break;
        }
    }
    EXPECT_TRUE(contact_detected);
}

// --- Set transform / velocity ---

TEST_F(JoltPhysicsWorldTest, SetTransformUpdatesPosition) {
    BodyDesc desc;
    desc.type = BodyType::Kinematic;
    desc.position = glm::vec3(0.0f);
    desc.shape = BoxShape{glm::vec3(1.0f)};

    BodyHandle handle = world->create_body(desc, 50);
    world->set_transform(handle, glm::vec3(5.0f, 10.0f, 15.0f),
                         glm::quat(1, 0, 0, 0));

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.x, 5.0f, 0.01f);
    EXPECT_NEAR(pos.y, 10.0f, 0.01f);
    EXPECT_NEAR(pos.z, 15.0f, 0.01f);
}

TEST_F(JoltPhysicsWorldTest, ApplyImpulseChangesPosition) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = SphereShape{0.5f};
    desc.mass = 1.0f;

    world->set_gravity(glm::vec3(0.0f));

    BodyHandle handle = world->create_body(desc, 3);
    world->apply_impulse(handle, glm::vec3(10.0f, 0.0f, 0.0f));

    for (int i = 0; i < 60; ++i) {
        world->step(1.0f / 60.0f);
    }

    glm::vec3 pos = world->get_position(handle);
    EXPECT_GT(pos.x, 0.5f);
}

// --- Multiple shape types ---

TEST_F(JoltPhysicsWorldTest, CapsuleShapeCreatesSuccessfully) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);
    desc.shape = CapsuleShape{0.5f, 0.25f};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, 77);
    EXPECT_NE(handle, 0u);

    glm::vec3 pos = world->get_position(handle);
    EXPECT_NEAR(pos.y, 5.0f, 0.01f);
}

// --- Raycast ---

TEST_F(JoltPhysicsWorldTest, RaycastHitsBody) {
    BodyDesc desc;
    desc.type = BodyType::Static;
    desc.position = glm::vec3(0.0f, 0.0f, -5.0f);
    desc.shape = BoxShape{glm::vec3(2.0f)};

    world->create_body(desc, 300);

    // Need at least one step for broad phase optimization
    world->step(1.0f / 60.0f);

    auto hit = world->raycast(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        100.0f
    );

    EXPECT_TRUE(hit.has_value());
    if (hit) {
        EXPECT_EQ(hit->entity, 300u);
        EXPECT_GT(hit->distance, 0.0f);
        EXPECT_LT(hit->distance, 10.0f);
    }
}

TEST_F(JoltPhysicsWorldTest, RaycastMissesReturnNullopt) {
    world->step(1.0f / 60.0f);

    auto hit = world->raycast(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        100.0f
    );

    EXPECT_FALSE(hit.has_value());
}

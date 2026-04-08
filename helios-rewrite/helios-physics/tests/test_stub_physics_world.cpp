// helios-physics/tests/test_stub_physics_world.cpp

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "interface/physics_world.h"
#include "stub/stub_physics_world.h"

using namespace helios::physics;

class StubPhysicsWorldTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsConfig config;
        config.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        config.fixed_timestep = 1.0f / 60.0f;
        world = std::make_unique<StubPhysicsWorld>(config);
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<StubPhysicsWorld> world;
};

// --- Body creation / destruction ---

TEST_F(StubPhysicsWorldTest, CreateBodyReturnsNonZeroHandle) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = BoxShape{glm::vec3(0.5f)};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, /*entity_id=*/42);
    EXPECT_NE(handle, 0u);
}

TEST_F(StubPhysicsWorldTest, DestroyBodyDoesNotCrash) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = SphereShape{0.5f};

    BodyHandle handle = world->create_body(desc, 100);
    EXPECT_NO_THROW(world->destroy_body(handle));
}

TEST_F(StubPhysicsWorldTest, DestroyInvalidHandleDoesNotCrash) {
    EXPECT_NO_THROW(world->destroy_body(999999));
}

TEST_F(StubPhysicsWorldTest, GetPositionReturnsCreationPosition) {
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

TEST_F(StubPhysicsWorldTest, DynamicBodyFallsUnderGravity) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.position = glm::vec3(0.0f, 10.0f, 0.0f);
    desc.shape = SphereShape{0.5f};
    desc.mass = 1.0f;

    BodyHandle handle = world->create_body(desc, 1);

    // Step 60 times (1 second of simulation)
    for (int i = 0; i < 60; ++i) {
        world->step(1.0f / 60.0f);
    }

    glm::vec3 pos = world->get_position(handle);
    // Stub uses simple Euler integration with gravity
    EXPECT_LT(pos.y, 10.0f);
}

TEST_F(StubPhysicsWorldTest, StaticBodyDoesNotMove) {
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

// --- Set transform / velocity ---

TEST_F(StubPhysicsWorldTest, SetTransformUpdatesPosition) {
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

TEST_F(StubPhysicsWorldTest, ApplyImpulseChangesPosition) {
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

TEST_F(StubPhysicsWorldTest, CapsuleShapeCreatesSuccessfully) {
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

// --- Raycast (stub always returns no hits) ---

TEST_F(StubPhysicsWorldTest, RaycastReturnsNullopt) {
    auto hit = world->raycast(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        100.0f
    );
    EXPECT_FALSE(hit.has_value());
}

TEST_F(StubPhysicsWorldTest, RaycastAllReturnsEmpty) {
    auto hits = world->raycast_all(
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        100.0f
    );
    EXPECT_TRUE(hits.empty());
}

// --- DrainContacts (stub always returns empty) ---

TEST_F(StubPhysicsWorldTest, DrainContactsReturnsEmpty) {
    world->step(1.0f / 60.0f);
    auto contacts = world->drain_contacts();
    EXPECT_TRUE(contacts.empty());
}

// --- Interface polymorphism ---

TEST_F(StubPhysicsWorldTest, WorksThroughBasePointer) {
    std::unique_ptr<PhysicsWorld> base = std::make_unique<StubPhysicsWorld>();

    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape = BoxShape{glm::vec3(1.0f)};
    desc.position = glm::vec3(0.0f, 5.0f, 0.0f);

    BodyHandle handle = base->create_body(desc, 1);
    EXPECT_NE(handle, 0u);

    glm::vec3 pos = base->get_position(handle);
    EXPECT_NEAR(pos.y, 5.0f, 0.01f);

    base->step(1.0f / 60.0f);
    base->destroy_body(handle);
}

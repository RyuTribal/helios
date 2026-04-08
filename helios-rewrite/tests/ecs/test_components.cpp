#include <gtest/gtest.h>

#include "helios/ecs/ecs.h"
#include "helios/components/components.h"

#include <glm/gtc/epsilon.hpp>
#include <cmath>

using namespace helios;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool mat4_approx_equal(const glm::mat4& a, const glm::mat4& b, float eps = 1e-5f) {
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            if (std::fabs(a[col][row] - b[col][row]) > eps)
                return false;
    return true;
}

// ---------------------------------------------------------------------------
// Task 13 – AssetHandle / BodyHandle / SoundHandle
// ---------------------------------------------------------------------------

TEST(AssetHandle, DefaultIsInvalid) {
    AssetHandle h;
    EXPECT_FALSE(h);
    EXPECT_EQ(h.index, 0u);
    EXPECT_EQ(h.generation, 0u);
}

TEST(AssetHandle, NonZeroIsValid) {
    AssetHandle h{42, 1};
    EXPECT_TRUE(h);
}

TEST(AssetHandle, EqualityAndInequality) {
    AssetHandle a{1, 1}, b{1, 1}, c{2, 1};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(AssetHandle, HashSpecialization) {
    std::hash<AssetHandle> hasher;
    AssetHandle h{99, 1};
    EXPECT_EQ(hasher(h), std::hash<uint64_t>{}(h.packed()));
}

TEST(BodyHandle, DefaultIsInvalid) {
    BodyHandle h;
    EXPECT_FALSE(h);
}

TEST(BodyHandle, NonZeroIsValid) {
    BodyHandle h{7};
    EXPECT_TRUE(h);
}

TEST(AssetHandle, PackUnpackRoundTrip) {
    AssetHandle h{123, 456};
    uint64_t packed = h.packed();
    AssetHandle unpacked = AssetHandle::from_packed(packed);
    EXPECT_EQ(h, unpacked);
}

TEST(SoundHandle, DefaultIsInvalid) {
    SoundHandle h;
    EXPECT_FALSE(h);
}

TEST(SoundHandle, NonZeroIsValid) {
    SoundHandle h{3};
    EXPECT_TRUE(h);
}

// ---------------------------------------------------------------------------
// Transform::to_mat4
// ---------------------------------------------------------------------------

TEST(Transform, DefaultIsIdentity) {
    Transform t;
    glm::mat4 m = t.to_mat4();
    EXPECT_TRUE(mat4_approx_equal(m, glm::mat4(1.0f)));
}

TEST(Transform, TranslationOnly) {
    Transform t;
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    glm::mat4 m = t.to_mat4();
    EXPECT_NEAR(m[3][0], 1.0f, 1e-5f);
    EXPECT_NEAR(m[3][1], 2.0f, 1e-5f);
    EXPECT_NEAR(m[3][2], 3.0f, 1e-5f);
}

TEST(Transform, ScaleOnly) {
    Transform t;
    t.scale = glm::vec3(2.0f, 3.0f, 4.0f);
    glm::mat4 m = t.to_mat4();
    EXPECT_NEAR(m[0][0], 2.0f, 1e-5f);
    EXPECT_NEAR(m[1][1], 3.0f, 1e-5f);
    EXPECT_NEAR(m[2][2], 4.0f, 1e-5f);
}

TEST(Transform, TranslationAndScaleCombined) {
    Transform t;
    t.position = glm::vec3(5.0f, 0.0f, 0.0f);
    t.scale    = glm::vec3(2.0f);
    glm::mat4 m = t.to_mat4();
    // Translation column
    EXPECT_NEAR(m[3][0], 5.0f, 1e-5f);
    // Scale on diagonal
    EXPECT_NEAR(m[0][0], 2.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Component default values
// ---------------------------------------------------------------------------

TEST(Camera, DefaultValues) {
    Camera cam;
    EXPECT_EQ(cam.projection, ProjectionType::Perspective);
    EXPECT_NEAR(cam.fov_degrees, 60.0f, 1e-5f);
    EXPECT_NEAR(cam.near_plane, 0.1f, 1e-5f);
    EXPECT_NEAR(cam.far_plane, 1000.0f, 1e-5f);
}

TEST(PointLight, DefaultValues) {
    PointLight pl;
    EXPECT_NEAR(pl.intensity, 1.0f, 1e-5f);
    EXPECT_NEAR(pl.radius, 10.0f, 1e-5f);
}

TEST(RigidBody, DefaultValues) {
    RigidBody rb;
    EXPECT_EQ(rb.body_type, BodyType::Dynamic);
    EXPECT_NEAR(rb.mass, 1.0f, 1e-5f);
    EXPECT_TRUE(Flags::has(rb.flags, RigidBodyFlags::UseGravity));
    EXPECT_FALSE(rb.handle);
}

TEST(BoxCollider, DefaultValues) {
    BoxCollider bc;
    EXPECT_NEAR(bc.half_extents.x, 0.5f, 1e-5f);
    EXPECT_FALSE(Flags::has(bc.flags, ColliderFlags::IsTrigger));
}

// ---------------------------------------------------------------------------
// Spawn + query by component type
// ---------------------------------------------------------------------------

TEST(WorldComponents, SpawnWithTransformAndQuery) {
    World world;
    Transform t;
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);

    Entity e = world.spawn(t, Tag{"player"});
    ASSERT_TRUE(world.is_alive(e));

    auto q = world.query<Transform, Tag>();
    int count = 0;
    for (auto [tr, tag] : q) {
        EXPECT_NEAR(tr.position.x, 1.0f, 1e-5f);
        EXPECT_EQ(tag.name, "player");
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(WorldComponents, SpawnMultipleAndQueryMeshRenderer) {
    World world;
    MeshRenderer mr;
    mr.mesh     = Handle<MeshAsset>{10, 1};
    mr.material = Handle<MaterialAsset>{20, 1};

    world.spawn(Transform{}, mr);
    world.spawn(Transform{}, mr, PointLight{});

    // Both entities have MeshRenderer.
    auto q = world.query<MeshRenderer>();
    EXPECT_EQ(q.count(), 2u);
}

// ---------------------------------------------------------------------------
// Without<Disabled> filter
// ---------------------------------------------------------------------------

TEST(WorldComponents, WithoutDisabledFilter) {
    World world;
    Transform t1; t1.position = glm::vec3(1.0f, 0.0f, 0.0f);
    Transform t2; t2.position = glm::vec3(2.0f, 0.0f, 0.0f);

    world.spawn(t1);                    // active
    world.spawn(t2, Disabled{});        // disabled

    auto q = world.query<Transform, Without<Disabled>>();
    EXPECT_EQ(q.count(), 1u);
    for (auto [tr] : q) {
        EXPECT_NEAR(tr.position.x, 1.0f, 1e-5f);
    }
}

// ---------------------------------------------------------------------------
// Parent–child hierarchy
// ---------------------------------------------------------------------------

TEST(WorldComponents, ParentChildHierarchy) {
    World world;
    Entity parent = world.spawn(Transform{}, Tag{"parent"});
    Entity child  = world.spawn(Transform{}, Parent{parent});

    EXPECT_TRUE(world.is_alive(parent));
    EXPECT_TRUE(world.is_alive(child));

    const Parent& p = world.get<Parent>(child);
    EXPECT_EQ(p.entity, parent);
}

TEST(WorldComponents, ChildrenComponent) {
    World world;
    Entity parent = world.spawn(Transform{});
    Entity c1     = world.spawn(Transform{});
    Entity c2     = world.spawn(Transform{});

    Children ch;
    ch.entities = {c1, c2};
    world.add<Children>(parent, ch);

    const Children& children = world.get<Children>(parent);
    ASSERT_EQ(children.entities.size(), 2u);
    EXPECT_EQ(children.entities[0], c1);
    EXPECT_EQ(children.entities[1], c2);
}

// ---------------------------------------------------------------------------
// Res / ResMut wrappers
// ---------------------------------------------------------------------------

struct GameConfig {
    int max_entities = 1000;
    float time_scale = 1.0f;
};

TEST(ResResMut, ResWrapsConstRef) {
    GameConfig cfg;
    cfg.max_entities = 500;

    Res<GameConfig> res(&cfg);
    EXPECT_EQ(res->max_entities, 500);
    EXPECT_EQ((*res).max_entities, 500);
    EXPECT_EQ(res.get()->max_entities, 500);
}

TEST(ResResMut, ResMutAllowsMutation) {
    GameConfig cfg;
    cfg.time_scale = 1.0f;

    ResMut<GameConfig> res_mut(&cfg);
    res_mut->time_scale = 2.0f;
    EXPECT_NEAR(cfg.time_scale, 2.0f, 1e-5f);

    (*res_mut).max_entities = 200;
    EXPECT_EQ(cfg.max_entities, 200);
}

// Compile-time concept checks
namespace {

struct GoodComponent { float x; int y; std::string name; };
struct MarkerComponent {};

struct BadVirtual { virtual void update() {} float x; };
struct BadConstructor { BadConstructor(int v) : val(v) {} int val; };
struct BadPrivate { float x; private: int secret; };

// These should satisfy the Component concept
static_assert(helios::Component<GoodComponent>);
static_assert(helios::Component<MarkerComponent>);
static_assert(helios::Component<helios::Transform>);
static_assert(helios::Component<helios::Tag>);
static_assert(helios::Component<helios::Children>);
static_assert(helios::Component<helios::Disabled>);
static_assert(helios::Component<helios::MeshRenderer>);
static_assert(helios::Component<helios::Camera>);

// These should NOT satisfy the Component concept
static_assert(!helios::Component<BadVirtual>, "Virtual methods disallowed");
static_assert(!helios::Component<BadConstructor>, "User constructors disallowed");
static_assert(!helios::Component<BadPrivate>, "Private members disallowed");

} // namespace

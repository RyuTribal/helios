#include <gtest/gtest.h>
#include "helios/ecs/world.h"
#include "helios/ecs/transform_propagation.h"
#include "helios/components/components.h"

using namespace helios;

namespace {

// Helper: approximate matrix equality for floating-point comparisons.
void expect_mat4_near(const glm::mat4& a, const glm::mat4& b, float eps = 1e-4f) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            EXPECT_NEAR(a[c][r], b[c][r], eps)
                << "Mismatch at [" << c << "][" << r << "]";
        }
    }
}

// ---------------------------------------------------------------------------
// Root entity propagation
// ---------------------------------------------------------------------------

TEST(TransformPropagation, RootEntityGlobalTransformUpdated) {
    World world;
    auto e = world.spawn(
        Transform{.position = glm::vec3(1.0f, 2.0f, 3.0f)},
        GlobalTransform{}
    );

    propagate_transforms(world);

    auto& gt = world.get<GlobalTransform>(e);
    glm::mat4 expected = Transform{.position = glm::vec3(1.0f, 2.0f, 3.0f)}.to_mat4();
    expect_mat4_near(gt.matrix, expected);
}

TEST(TransformPropagation, RootEntityWithScaleAndRotation) {
    World world;
    Transform t{
        .position = glm::vec3(0.0f, 5.0f, 0.0f),
        .rotation = glm::quat(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f)),
        .scale = glm::vec3(2.0f)
    };
    auto e = world.spawn(t, GlobalTransform{});

    propagate_transforms(world);

    auto& gt = world.get<GlobalTransform>(e);
    expect_mat4_near(gt.matrix, t.to_mat4());
}

// ---------------------------------------------------------------------------
// Parent-child propagation
// ---------------------------------------------------------------------------

TEST(TransformPropagation, ChildGlobalTransformCombinesParent) {
    World world;

    // Parent at position (10, 0, 0)
    auto parent = world.spawn(
        Transform{.position = glm::vec3(10.0f, 0.0f, 0.0f)},
        GlobalTransform{}
    );

    // Child at local offset (0, 5, 0) relative to parent
    auto child = world.spawn(
        Transform{.position = glm::vec3(0.0f, 5.0f, 0.0f)},
        GlobalTransform{},
        Parent{.entity = parent}
    );

    // Wire up the Children component on the parent
    world.add(parent, Children{.entities = {child}});

    propagate_transforms(world);

    // Child's global position should be parent + child = (10, 5, 0)
    auto& gt = world.get<GlobalTransform>(child);
    glm::vec3 world_pos = glm::vec3(gt.matrix[3]);
    EXPECT_NEAR(world_pos.x, 10.0f, 1e-4f);
    EXPECT_NEAR(world_pos.y,  5.0f, 1e-4f);
    EXPECT_NEAR(world_pos.z,  0.0f, 1e-4f);
}

TEST(TransformPropagation, ThreeLevelHierarchy) {
    World world;

    auto grandparent = world.spawn(
        Transform{.position = glm::vec3(1.0f, 0.0f, 0.0f)},
        GlobalTransform{}
    );
    auto parent_e = world.spawn(
        Transform{.position = glm::vec3(0.0f, 2.0f, 0.0f)},
        GlobalTransform{},
        Parent{.entity = grandparent}
    );
    auto child = world.spawn(
        Transform{.position = glm::vec3(0.0f, 0.0f, 3.0f)},
        GlobalTransform{},
        Parent{.entity = parent_e}
    );

    world.add(grandparent, Children{.entities = {parent_e}});
    world.add(parent_e, Children{.entities = {child}});

    propagate_transforms(world);

    // Expected: grandparent at (1,0,0), parent at (1,2,0), child at (1,2,3)
    auto& gp_gt = world.get<GlobalTransform>(grandparent);
    glm::vec3 gp_pos = glm::vec3(gp_gt.matrix[3]);
    EXPECT_NEAR(gp_pos.x, 1.0f, 1e-4f);

    auto& p_gt = world.get<GlobalTransform>(parent_e);
    glm::vec3 p_pos = glm::vec3(p_gt.matrix[3]);
    EXPECT_NEAR(p_pos.x, 1.0f, 1e-4f);
    EXPECT_NEAR(p_pos.y, 2.0f, 1e-4f);

    auto& c_gt = world.get<GlobalTransform>(child);
    glm::vec3 c_pos = glm::vec3(c_gt.matrix[3]);
    EXPECT_NEAR(c_pos.x, 1.0f, 1e-4f);
    EXPECT_NEAR(c_pos.y, 2.0f, 1e-4f);
    EXPECT_NEAR(c_pos.z, 3.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Subtree skipping (unchanged entities are not recomputed)
// ---------------------------------------------------------------------------

TEST(TransformPropagation, SubtreeSkippingWhenUnchanged) {
    World world;

    auto root = world.spawn(
        Transform{.position = glm::vec3(1.0f, 0.0f, 0.0f)},
        GlobalTransform{}
    );
    auto child = world.spawn(
        Transform{.position = glm::vec3(0.0f, 2.0f, 0.0f)},
        GlobalTransform{},
        Parent{.entity = root}
    );
    world.add(root, Children{.entities = {child}});

    // First propagation: establishes GlobalTransforms.
    propagate_transforms(world);

    auto& c_gt = world.get<GlobalTransform>(child);
    EXPECT_NEAR(glm::vec3(c_gt.matrix[3]).y, 2.0f, 1e-4f);

    // Record the tick on the child's GlobalTransform.
    auto loc = world.archetypes().locate(child);
    auto& g_col = loc->archetype->get_column<GlobalTransform>();
    uint32_t tick_after_first = g_col.changed_tick(loc->row);

    // Second propagation without any Transform changes.
    // The child's GlobalTransform tick should NOT change (subtree skipped).
    propagate_transforms(world);

    // Re-locate in case archetype moved (it shouldn't, but be safe).
    loc = world.archetypes().locate(child);
    auto& g_col2 = loc->archetype->get_column<GlobalTransform>();
    uint32_t tick_after_second = g_col2.changed_tick(loc->row);

    EXPECT_EQ(tick_after_first, tick_after_second);
}

TEST(TransformPropagation, ChildUpdatedWhenParentTransformChanges) {
    World world;

    auto root = world.spawn(
        Transform{.position = glm::vec3(0.0f, 0.0f, 0.0f)},
        GlobalTransform{}
    );
    auto child = world.spawn(
        Transform{.position = glm::vec3(0.0f, 1.0f, 0.0f)},
        GlobalTransform{},
        Parent{.entity = root}
    );
    world.add(root, Children{.entities = {child}});

    // First propagation.
    propagate_transforms(world);

    auto& c_gt = world.get<GlobalTransform>(child);
    EXPECT_NEAR(glm::vec3(c_gt.matrix[3]).x, 0.0f, 1e-4f);
    EXPECT_NEAR(glm::vec3(c_gt.matrix[3]).y, 1.0f, 1e-4f);

    // Mutate the root's Transform (stamp it so propagation detects it).
    WorldTestAccess::advance_tick(world);
    {
        auto q = world.query<Transform>(0, world.current_tick());
        auto result = q.get(root);
        ASSERT_TRUE(result.has_value());
        auto& [t] = *result;
        t.position = glm::vec3(10.0f, 0.0f, 0.0f);
    }

    // Second propagation: child should now reflect the parent's new position.
    propagate_transforms(world);

    auto& c_gt2 = world.get<GlobalTransform>(child);
    glm::vec3 c_pos = glm::vec3(c_gt2.matrix[3]);
    EXPECT_NEAR(c_pos.x, 10.0f, 1e-4f);
    EXPECT_NEAR(c_pos.y,  1.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Entity without GlobalTransform is ignored
// ---------------------------------------------------------------------------

TEST(TransformPropagation, EntityWithoutGlobalTransformIgnored) {
    World world;

    // Entity with Transform but no GlobalTransform -- should not crash.
    world.spawn(Transform{.position = glm::vec3(1.0f, 0.0f, 0.0f)});

    EXPECT_NO_THROW(propagate_transforms(world));
}

// ---------------------------------------------------------------------------
// Changed<GlobalTransform> detection after propagation
// ---------------------------------------------------------------------------

TEST(TransformPropagation, ChangedGlobalTransformDetectableByQuery) {
    World world;

    auto e = world.spawn(
        Transform{.position = glm::vec3(5.0f, 0.0f, 0.0f)},
        GlobalTransform{}
    );

    // Record tick before propagation.
    uint32_t tick_before = world.current_tick();

    propagate_transforms(world);

    // Query with Changed<GlobalTransform> should find the entity.
    auto q = world.query<const GlobalTransform, Changed<GlobalTransform>>(
        tick_before, world.current_tick());
    int count = 0;
    for ([[maybe_unused]] auto tup : q) {
        ++count;
    }
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// Multiple children of same parent
// ---------------------------------------------------------------------------

TEST(TransformPropagation, MultipleChildrenAllUpdated) {
    World world;

    auto root = world.spawn(
        Transform{.position = glm::vec3(10.0f, 0.0f, 0.0f)},
        GlobalTransform{}
    );

    auto c1 = world.spawn(
        Transform{.position = glm::vec3(0.0f, 1.0f, 0.0f)},
        GlobalTransform{},
        Parent{.entity = root}
    );
    auto c2 = world.spawn(
        Transform{.position = glm::vec3(0.0f, 2.0f, 0.0f)},
        GlobalTransform{},
        Parent{.entity = root}
    );
    auto c3 = world.spawn(
        Transform{.position = glm::vec3(0.0f, 3.0f, 0.0f)},
        GlobalTransform{},
        Parent{.entity = root}
    );

    world.add(root, Children{.entities = {c1, c2, c3}});

    propagate_transforms(world);

    // All children should have parent offset applied.
    auto& gt1 = world.get<GlobalTransform>(c1);
    EXPECT_NEAR(glm::vec3(gt1.matrix[3]).x, 10.0f, 1e-4f);
    EXPECT_NEAR(glm::vec3(gt1.matrix[3]).y,  1.0f, 1e-4f);

    auto& gt2 = world.get<GlobalTransform>(c2);
    EXPECT_NEAR(glm::vec3(gt2.matrix[3]).x, 10.0f, 1e-4f);
    EXPECT_NEAR(glm::vec3(gt2.matrix[3]).y,  2.0f, 1e-4f);

    auto& gt3 = world.get<GlobalTransform>(c3);
    EXPECT_NEAR(glm::vec3(gt3.matrix[3]).x, 10.0f, 1e-4f);
    EXPECT_NEAR(glm::vec3(gt3.matrix[3]).y,  3.0f, 1e-4f);
}

} // anonymous namespace

#include <gtest/gtest.h>
#include "helios/app/state.h"
#include "helios/app/state_builder.h"
#include "helios/ecs/world.h"

using namespace helios;

// -- Test fixtures --

enum class TestState { A, B, C };

struct PositionComponent {
    float x = 0.0f, y = 0.0f;
};

// A simple state that spawns two tracked entities.
class StateA : public State<TestState> {
public:
    StateA(World& world) {
        m_entity1 = spawn_tracked(world);
        world.add(m_entity1, PositionComponent{1.0f, 2.0f});

        m_entity2 = spawn_tracked(world);
        world.add(m_entity2, PositionComponent{3.0f, 4.0f});
    }

    Entity entity1() const { return m_entity1; }
    Entity entity2() const { return m_entity2; }

    static void describe(StateBuilder<StateA>& s) {
        s.opaque();
    }

private:
    Entity m_entity1;
    Entity m_entity2;
};

// -- Tests --

TEST(StateTest, SpawnTrackedCreatesEntities) {
    World world;
    StateA state(world);

    EXPECT_TRUE(world.is_alive(state.entity1()));
    EXPECT_TRUE(world.is_alive(state.entity2()));

    auto& pos = world.get<PositionComponent>(state.entity1());
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
}

TEST(StateTest, DespawnTrackedRemovesAllEntities) {
    World world;

    {
        StateA state(world);
        Entity e1 = state.entity1();
        Entity e2 = state.entity2();

        EXPECT_TRUE(world.is_alive(e1));
        EXPECT_TRUE(world.is_alive(e2));

        // Explicitly despawn tracked entities (normally called by GameFlow).
        state.despawn_tracked(world);

        EXPECT_FALSE(world.is_alive(e1));
        EXPECT_FALSE(world.is_alive(e2));
    }
}

TEST(StateTest, TrackedEntitiesListIsAccurate) {
    World world;
    StateA state(world);

    const auto& tracked = state.tracked_entities();
    EXPECT_EQ(tracked.size(), 2u);
    EXPECT_EQ(tracked[0], state.entity1());
    EXPECT_EQ(tracked[1], state.entity2());
}

TEST(StateTest, DespawnTrackedHandlesAlreadyDeadEntities) {
    World world;
    StateA state(world);

    // Manually despawn one entity before calling despawn_tracked.
    world.despawn(state.entity1());
    EXPECT_FALSE(world.is_alive(state.entity1()));

    // Should not crash -- skips dead entities.
    EXPECT_NO_THROW(state.despawn_tracked(world));
    EXPECT_FALSE(world.is_alive(state.entity2()));
}

TEST(StateTest, ModifierDefaultsToOpaque) {
    World world;
    StateA state(world);
    EXPECT_EQ(state.modifier(), StateModifier::Opaque);
}

TEST(StateTest, SetModifierChangesValue) {
    World world;
    StateA state(world);
    state.set_modifier(StateModifier::Transparent);
    EXPECT_EQ(state.modifier(), StateModifier::Transparent);
}

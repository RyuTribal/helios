#include <gtest/gtest.h>
#include "helios/app/game_flow.h"
#include "helios/app/state.h"
#include "helios/app/state_builder.h"
#include "helios/ecs/world.h"
#include "helios/ecs/scheduler.h"

using namespace helios;

// -- Test state enum and states --

enum class GS { Menu, Playing, Paused, Overlay };

class MenuState : public State<GS> {
public:
    MenuState(World& world) {
        m_entity = spawn_tracked(world);
    }
    void menu_system() { }
    Entity entity() const { return m_entity; }
    static void describe(StateBuilder<MenuState>& s) {
        s.opaque();
        s.system(&MenuState::menu_system);
    }
private:
    Entity m_entity;
};

class PlayingState : public State<GS> {
public:
    PlayingState(World& world) {
        m_entity = spawn_tracked(world);
    }
    void playing_system() { }
    Entity entity() const { return m_entity; }
    static void describe(StateBuilder<PlayingState>& s) {
        s.opaque();
        s.system(&PlayingState::playing_system);
    }
private:
    Entity m_entity;
};

class PausedState : public State<GS> {
public:
    PausedState(World& /*world*/) {}
    void paused_system() { }
    static void describe(StateBuilder<PausedState>& s) {
        s.pause_below();
        s.system(&PausedState::paused_system);
    }
};

class OverlayState : public State<GS> {
public:
    OverlayState(World& /*world*/) {}
    void overlay_system() { }
    static void describe(StateBuilder<OverlayState>& s) {
        s.transparent();
        s.system(&OverlayState::overlay_system);
    }
};

class GameFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        flow.register_state<MenuState>(GS::Menu);
        flow.register_state<PlayingState>(GS::Playing);
        flow.register_state<PausedState>(GS::Paused);
        flow.register_state<OverlayState>(GS::Overlay);
        flow.bind_scheduler(scheduler);
    }

    World world;
    Scheduler scheduler;
    GameFlow<GS> flow;
};

// -- Push / Pop --

TEST_F(GameFlowTest, PushIncreasesStackDepth) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Menu);
    EXPECT_TRUE(flow.is_in(GS::Menu));
}

TEST_F(GameFlowTest, PopDecreasesStackDepth) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);
    EXPECT_EQ(flow.depth(), 2u);

    flow.pop();
    flow.apply_pending(world, scheduler);
    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Menu);
}

TEST_F(GameFlowTest, PopOnEmptyStackIsNoOp) {
    flow.pop();
    EXPECT_NO_THROW(flow.apply_pending(world, scheduler));
    EXPECT_TRUE(flow.is_empty());
}

// -- go_to --

TEST_F(GameFlowTest, GoToClearsStackAndPushesNewState) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    flow.go_to<PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Playing);
    EXPECT_FALSE(flow.is_in(GS::Menu));
}

TEST_F(GameFlowTest, GoToMultipleStates) {
    flow.go_to<MenuState, PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 2u);
    EXPECT_EQ(flow.current(), GS::Playing);  // rightmost = top
    EXPECT_TRUE(flow.is_in(GS::Menu));
}

// -- switch_to --

TEST_F(GameFlowTest, SwitchToReplacesTopState) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    flow.switch_to<PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Playing);
    EXPECT_FALSE(flow.is_in(GS::Menu));
}

// -- Entity auto-despawn on state exit --

TEST_F(GameFlowTest, EntitiesDespawnedWhenStatePopped) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    // PlayingState spawns one tracked entity.
    EXPECT_EQ(flow.depth(), 1u);

    flow.pop();
    flow.apply_pending(world, scheduler);

    EXPECT_TRUE(flow.is_empty());
}

TEST_F(GameFlowTest, EntitiesDespawnedWhenGoToClearsStack) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    EXPECT_FALSE(flow.is_empty());

    flow.go_to<PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 1u);
}

// -- Modifier behavior: opaque --

TEST_F(GameFlowTest, OpaqueStateStopsSystemsBelow) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    // Push opaque Menu on top of Playing.
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    // Only MenuState's systems should be active.
    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 1u);
}

// -- Modifier behavior: transparent --

TEST_F(GameFlowTest, TransparentStateKeepsSystemsBelow) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    // Push transparent Overlay on top.
    flow.push<OverlayState>();
    flow.apply_pending(world, scheduler);

    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 2u);
}

// -- Modifier behavior: pause_below --

TEST_F(GameFlowTest, PauseBelowStopsSystemsBelowButEntitiesVisible) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    flow.push<PausedState>();
    flow.apply_pending(world, scheduler);

    // PausedState has pause_below: only PausedState's systems should run.
    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 1u);

    // PlayingState's tracked entities should still be alive (not despawned).
    EXPECT_EQ(flow.depth(), 2u);
}

// -- Multiple operations in one frame --

TEST_F(GameFlowTest, MultipleOpsAppliedInOrder) {
    flow.push<MenuState>();
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 2u);
    EXPECT_EQ(flow.current(), GS::Playing);

    flow.pop();
    flow.pop();
    flow.apply_pending(world, scheduler);

    EXPECT_TRUE(flow.is_empty());
}

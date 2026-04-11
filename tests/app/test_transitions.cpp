#include <gtest/gtest.h>
#include "helios/app/game_flow.h"
#include "helios/app/state.h"
#include "helios/app/state_builder.h"
#include "helios/app/transition.h"
#include "helios/ecs/world.h"
#include "helios/ecs/scheduler.h"

using namespace helios;

// -- Test state enum and states --

enum class TS { Menu, Loading, Playing };

static bool g_loading_constructed = false;
static bool g_loading_destructed = false;
static bool g_playing_constructed = false;

class TMenuState : public State<TS> {
public:
    TMenuState(World& /*world*/) {}
    static void describe(StateBuilder<TMenuState>& s) { s.opaque(); }
};

class TLoadingState : public State<TS> {
public:
    TLoadingState(World& world) {
        g_loading_constructed = true;
        m_entity = spawn_tracked(world);
    }
    ~TLoadingState() override {
        g_loading_destructed = true;
    }

    Entity entity() const { return m_entity; }

    static void describe(StateBuilder<TLoadingState>& s) {
        s.opaque();
    }
private:
    Entity m_entity;
};

class TPlayingState : public State<TS> {
public:
    TPlayingState(World& /*world*/) {
        g_playing_constructed = true;
    }
    static void describe(StateBuilder<TPlayingState>& s) { s.opaque(); }
};

class TransitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_loading_constructed = false;
        g_loading_destructed = false;
        g_playing_constructed = false;

        flow.register_state<TMenuState>(TS::Menu);
        flow.register_state<TLoadingState>(TS::Loading);
        flow.register_state<TPlayingState>(TS::Playing);
        flow.bind_scheduler(scheduler);
    }

    World world;
    Scheduler scheduler;
    GameFlow<TS> flow;
};

// -- Transition via intermediate state --

TEST_F(TransitionTest, TransitionViaIntermediateState) {
    // Register transition: Menu -> Playing goes via Loading.
    flow.transition(TS::Menu, TS::Playing)
        .via<TLoadingState>();

    // Start in Menu.
    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);
    EXPECT_EQ(flow.current(), TS::Menu);

    // Request go_to Playing. The transition system should detect the
    // Menu->Playing chain and insert Loading.
    flow.go_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    // Stack should be: [Playing, Loading]  (Loading on top).
    EXPECT_TRUE(g_loading_constructed);
    EXPECT_TRUE(g_playing_constructed);
    EXPECT_EQ(flow.depth(), 2u);
    EXPECT_EQ(flow.current(), TS::Loading);
    EXPECT_TRUE(flow.is_in(TS::Playing));

    // Loading is opaque, so Playing's systems should not be active.
    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 1u);
}

TEST_F(TransitionTest, IntermediateStatePopRevealsTarget) {
    flow.transition(TS::Menu, TS::Playing)
        .via<TLoadingState>();

    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);

    flow.go_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    // Stack: [Playing, Loading]
    // Simulate loading done: pop Loading.
    flow.pop();
    flow.apply_pending(world, scheduler);

    // Loading should be destructed, its tracked entity despawned.
    EXPECT_TRUE(g_loading_destructed);
    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), TS::Playing);
}

TEST_F(TransitionTest, DirectTransitionWithoutChain) {
    // No transition chain registered for Menu -> Playing (no .via() here).
    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);

    flow.switch_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    // Should go directly to Playing, no Loading.
    EXPECT_FALSE(g_loading_constructed);
    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), TS::Playing);
}

TEST_F(TransitionTest, LoadingEntityDespawnedOnPop) {
    flow.transition(TS::Menu, TS::Playing)
        .via<TLoadingState>();

    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);

    flow.go_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    flow.pop();  // pop Loading
    flow.apply_pending(world, scheduler);

    // Loading's tracked entities should be despawned.
    EXPECT_TRUE(g_loading_destructed);
    EXPECT_EQ(flow.depth(), 1u);
}

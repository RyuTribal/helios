#include <gtest/gtest.h>
#include <atomic>
#include <string>
#include <vector>
#include "helios/ecs/app.h"
#include "helios/ecs/plugin.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/system_set.h"
#include "helios/ecs/time.h"

using namespace helios;

// ---- Test resources ----
namespace {

struct GameState {
    int score = 0;
    int physics_ticks = 0;
    std::vector<std::string> log;
};

// ---- Test plugin ----
struct GamePlugin {
    void build(App& app) {
        app.insert_resource<GameState>(GameState{});

        app.add_system(Schedule::PreUpdate, []() {
            // Simulate input polling -- no-op
        }, "poll_input");

        app.add_system(Schedule::Update, []() {
            // Simulate game logic
        }, "game_logic");

        app.add_system(Schedule::FixedUpdate, []() {
            // Simulate physics
        }, "physics_step");

        app.add_system(Schedule::PostUpdate, []() {
            // Simulate transform propagation
        }, "propagate_transforms");

        app.add_system(Schedule::PreRender, []() {
            // Simulate render extraction
        }, "extract_render");
    }
};

static_assert(Plugin<GamePlugin>);

// ---- Non-plugin: concept should reject ----
struct NotAPlugin {
    int x;
};
static_assert(!Plugin<NotAPlugin>);

} // anonymous namespace

TEST(Integration, FullPipeline) {
    App app;
    app.add_plugin<GamePlugin>();

    // Run 5 ticks
    for (int i = 0; i < 5; ++i) {
        app.tick();
    }

    auto& time = app.world().resource<Time>();
    EXPECT_EQ(time.frame_count(), 5u);
    EXPECT_GT(time.elapsed(), 0.0f);
}

TEST(Integration, OrderingChain) {
    static std::vector<std::string> order;
    order.clear();

    App app;

    auto id_a = app.add_system(Schedule::Update, []() {
        order.push_back("a");
    }, "a").id();

    auto id_b = app.add_system(Schedule::Update,
        sys([]() { order.push_back("b"); }).after(id_a),
        "b").id();

    app.add_system(Schedule::Update,
        sys([]() { order.push_back("c"); }).after(id_b),
        "c");

    app.tick();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "a");
    EXPECT_EQ(order[1], "b");
    EXPECT_EQ(order[2], "c");
}

TEST(Integration, ParallelPipeline) {
    App app;
    app.enable_parallel(2);

    std::atomic<int> counter{0};

    // 10 independent systems
    for (int i = 0; i < 10; ++i) {
        app.add_system(Schedule::Update, [&counter]() {
            counter.fetch_add(1);
        });
    }

    app.tick();
    EXPECT_EQ(counter.load(), 10);
}

TEST(Integration, PluginConceptCheck) {
    // Compile-time: GamePlugin satisfies Plugin, NotAPlugin does not.
    // If this file compiles, the concept checks pass.
    static_assert(Plugin<GamePlugin>);
    static_assert(!Plugin<NotAPlugin>);
}

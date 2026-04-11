#include <gtest/gtest.h>
#include <atomic>
#include <string>
#include <vector>
#include "helios/ecs/app.h"
#include "helios/ecs/plugin.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/time.h"

using namespace helios;

// ---- Test resources ----
namespace {

struct Counter { int value = 0; };
struct Label   { std::string text; };

// ---- Test plugin ----
int g_plugin_build_count = 0;

struct TestPlugin {
    void build(App& app) {
        g_plugin_build_count++;
        app.insert_resource<Counter>(Counter{0});
    }
};

// Static assert that TestPlugin satisfies Plugin concept
static_assert(Plugin<TestPlugin>);

// A plugin that depends on TestPlugin
struct DependentPlugin {
    void build(App& app) {
        app.add_plugin<TestPlugin>(); // should be deduplicated
        app.insert_resource<Label>(Label{"hello"});
    }
};

} // anonymous namespace

TEST(App, InsertsTimeAutomatically) {
    App app;
    // App constructor should insert Time and FixedTimeAccumulator
    EXPECT_TRUE(app.world().has_resource<Time>());
    EXPECT_TRUE(app.world().has_resource<FixedTimeAccumulator>());
}

TEST(App, InsertResource) {
    App app;
    app.insert_resource<Counter>(Counter{42});
    EXPECT_EQ(app.world().resource<Counter>().value, 42);
}

TEST(App, PluginRegistration) {
    g_plugin_build_count = 0;

    App app;
    app.add_plugin<TestPlugin>();

    EXPECT_EQ(g_plugin_build_count, 1);
    EXPECT_TRUE(app.world().has_resource<Counter>());
}

TEST(App, PluginDeduplication) {
    g_plugin_build_count = 0;

    App app;
    app.add_plugin<TestPlugin>();
    app.add_plugin<TestPlugin>(); // should be no-op

    EXPECT_EQ(g_plugin_build_count, 1);
}

TEST(App, PluginDependencyDeduplication) {
    g_plugin_build_count = 0;

    App app;
    app.add_plugin<TestPlugin>();
    app.add_plugin<DependentPlugin>(); // calls add_plugin<TestPlugin> internally

    // TestPlugin::build should have been called only once
    EXPECT_EQ(g_plugin_build_count, 1);
    // But DependentPlugin's resource should exist
    EXPECT_TRUE(app.world().has_resource<Label>());
}

TEST(App, AddSystemAndTick) {
    App app;
    app.insert_resource<Counter>(Counter{0});

    app.add_system(Schedule::Update, []() {
        // In a real system this would use ResMut<Counter>, but for testing
        // the scheduler mechanics we use a simpler approach.
    });

    // tick() should not crash
    app.tick();

    // Time should have been updated (delta will be very small)
    EXPECT_EQ(app.world().resource<Time>().frame_count(), 1u);
}

TEST(App, TickIncrementsFrameCount) {
    App app;

    app.tick();
    EXPECT_EQ(app.world().resource<Time>().frame_count(), 1u);

    app.tick();
    EXPECT_EQ(app.world().resource<Time>().frame_count(), 2u);

    app.tick();
    EXPECT_EQ(app.world().resource<Time>().frame_count(), 3u);
}

TEST(App, QuitStopsRun) {
    App app;

    // Add a system that quits after 3 frames
    std::atomic<int> frame_count{0};
    app.add_system(Schedule::Update, [&frame_count, &app]() {
        if (frame_count.fetch_add(1) >= 2) {
            app.quit();
        }
    });

    app.run(); // should return after ~3 frames

    EXPECT_GE(frame_count.load(), 3);
}

TEST(App, StartupSystemsRunOnce) {
    static int startup_count = 0;
    startup_count = 0;

    App app;
    app.add_system(Schedule::Startup, []() {
        startup_count++;
    });

    // run() calls Startup once, then loops Update etc.
    // We need to quit quickly:
    app.add_system(Schedule::Update, [&app]() {
        app.quit();
    });

    app.run();

    EXPECT_EQ(startup_count, 1);
}

TEST(App, ScheduleOrderingInTick) {
    static std::vector<std::string> log;
    log.clear();

    App app;
    app.add_system(Schedule::PreUpdate, []() { log.push_back("pre"); });
    app.add_system(Schedule::Update,    []() { log.push_back("update"); });
    app.add_system(Schedule::PostUpdate,[]() { log.push_back("post"); });
    app.add_system(Schedule::PreRender, []() { log.push_back("render"); });

    app.tick();

    ASSERT_EQ(log.size(), 4u);
    EXPECT_EQ(log[0], "pre");
    EXPECT_EQ(log[1], "update");
    EXPECT_EQ(log[2], "post");
    EXPECT_EQ(log[3], "render");
}

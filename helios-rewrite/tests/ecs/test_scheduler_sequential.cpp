#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "helios/ecs/schedule.h"
#include "helios/ecs/scheduler.h"
#include "helios/ecs/system_descriptor.h"
#include "helios/ecs/world.h"

using namespace helios;

// Track execution order via a shared vector.
// Each test clears this before use.
static std::vector<std::string> g_execution_log;

TEST(SchedulerSequential, SingleSystemRuns) {
    g_execution_log.clear();

    Scheduler scheduler;
    auto id = scheduler.add_system(Schedule::Update,
        []() {
            g_execution_log.push_back("system_a");
        }, "system_a").id();

    (void)id;

    World world;
    scheduler.run(world, Schedule::Update);

    ASSERT_EQ(g_execution_log.size(), 1u);
    EXPECT_EQ(g_execution_log[0], "system_a");
}

TEST(SchedulerSequential, MultipleSystemsAllRun) {
    g_execution_log.clear();

    Scheduler scheduler;
    scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("a");
    }, "a");
    scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("b");
    }, "b");
    scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("c");
    }, "c");

    World world;
    scheduler.run(world, Schedule::Update);

    ASSERT_EQ(g_execution_log.size(), 3u);
    // No access conflicts and no ordering constraints -> same stage,
    // registration order within the stage.
    EXPECT_EQ(g_execution_log[0], "a");
    EXPECT_EQ(g_execution_log[1], "b");
    EXPECT_EQ(g_execution_log[2], "c");
}

TEST(SchedulerSequential, OrderingAfter) {
    g_execution_log.clear();

    Scheduler scheduler;

    auto id_a = scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("a");
    }, "a").id();

    scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("b");
    }, "b").after(id_a);

    World world;
    scheduler.run(world, Schedule::Update);

    ASSERT_EQ(g_execution_log.size(), 2u);
    EXPECT_EQ(g_execution_log[0], "a");
    EXPECT_EQ(g_execution_log[1], "b");
}

TEST(SchedulerSequential, OrderingBefore) {
    g_execution_log.clear();

    Scheduler scheduler;

    auto id_b = scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("b");
    }, "b").id();

    // "a" must run before "b"
    scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("a");
    }, "a").before(id_b);

    World world;
    scheduler.run(world, Schedule::Update);

    ASSERT_EQ(g_execution_log.size(), 2u);
    EXPECT_EQ(g_execution_log[0], "a");
    EXPECT_EQ(g_execution_log[1], "b");
}

TEST(SchedulerSequential, DifferentSchedulesIsolated) {
    g_execution_log.clear();

    Scheduler scheduler;
    scheduler.add_system(Schedule::Update, []() {
        g_execution_log.push_back("update");
    });
    scheduler.add_system(Schedule::PreUpdate, []() {
        g_execution_log.push_back("pre_update");
    });

    World world;

    // Run only Update -- PreUpdate should not run
    scheduler.run(world, Schedule::Update);
    ASSERT_EQ(g_execution_log.size(), 1u);
    EXPECT_EQ(g_execution_log[0], "update");

    // Now run PreUpdate
    scheduler.run(world, Schedule::PreUpdate);
    ASSERT_EQ(g_execution_log.size(), 2u);
    EXPECT_EQ(g_execution_log[1], "pre_update");
}

TEST(SchedulerSequential, StartupRunsTwiceIfCalledTwice) {
    g_execution_log.clear();

    Scheduler scheduler;
    scheduler.add_system(Schedule::Startup, []() {
        g_execution_log.push_back("startup");
    });

    World world;
    scheduler.run(world, Schedule::Startup);
    ASSERT_EQ(g_execution_log.size(), 1u);

    // Running again -- the scheduler itself does not prevent re-running.
    // That is the App's responsibility.
    scheduler.run(world, Schedule::Startup);
    ASSERT_EQ(g_execution_log.size(), 2u);
}

TEST(SchedulerSequential, EmptyScheduleIsNoop) {
    Scheduler scheduler;
    World world;
    // Should not crash
    scheduler.run(world, Schedule::Update);
    scheduler.run(world, Schedule::FixedUpdate);
}

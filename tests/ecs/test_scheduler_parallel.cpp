#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "helios/ecs/schedule.h"
#include "helios/ecs/scheduler.h"
#include "helios/ecs/world.h"

using namespace helios;

// Shared test state
static std::mutex g_log_mutex;
static std::vector<std::string> g_log;
static std::atomic<int> g_concurrent{0};
static std::atomic<int> g_max_concurrent{0};

static void reset_state() {
    std::lock_guard lock(g_log_mutex);
    g_log.clear();
    g_concurrent.store(0);
    g_max_concurrent.store(0);
}

static void log_entry(const std::string& name) {
    std::lock_guard lock(g_log_mutex);
    g_log.push_back(name);
}

static void track_concurrency() {
    int c = g_concurrent.fetch_add(1) + 1;
    int prev = g_max_concurrent.load();
    while (c > prev && !g_max_concurrent.compare_exchange_weak(prev, c)) {}
}

static void untrack_concurrency() {
    g_concurrent.fetch_sub(1);
}

TEST(SchedulerParallel, IndependentSystemsRunParallel) {
    reset_state();

    Scheduler scheduler;
    scheduler.enable_parallel(4);

    // Two systems with NO access descriptors (independent).
    // They should land in the same stage and execute in parallel.
    scheduler.add_system(Schedule::Update, []() {
        track_concurrency();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        log_entry("a");
        untrack_concurrency();
    }, "a");

    scheduler.add_system(Schedule::Update, []() {
        track_concurrency();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        log_entry("b");
        untrack_concurrency();
    }, "b");

    World world;
    auto start = std::chrono::high_resolution_clock::now();
    scheduler.run(world, Schedule::Update);
    auto end = std::chrono::high_resolution_clock::now();

    float elapsed_ms = std::chrono::duration<float, std::milli>(end - start).count();

    {
        std::lock_guard lock(g_log_mutex);
        EXPECT_EQ(g_log.size(), 2u);
    }
    // If they ran in parallel, total time should be ~30ms, not ~60ms.
    // Allow generous margin for CI overhead.
    EXPECT_LT(elapsed_ms, 55.0f);
    EXPECT_GE(g_max_concurrent.load(), 2);
}

TEST(SchedulerParallel, ConflictingSystemsSequentialUnderParallel) {
    reset_state();

    Scheduler scheduler;
    scheduler.enable_parallel(4);

    auto id_a = scheduler.add_system(Schedule::Update, []() {
        track_concurrency();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        log_entry("a");
        untrack_concurrency();
    }, "a").id();

    // Force b after a
    scheduler.add_system(Schedule::Update, []() {
        track_concurrency();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        log_entry("b");
        untrack_concurrency();
    }, "b").after(id_a);

    World world;
    scheduler.run(world, Schedule::Update);

    {
        std::lock_guard lock(g_log_mutex);
        ASSERT_EQ(g_log.size(), 2u);
        // With explicit ordering, max concurrency for these two should be 1
        EXPECT_EQ(g_max_concurrent.load(), 1);

        // Order must be preserved
        EXPECT_EQ(g_log[0], "a");
        EXPECT_EQ(g_log[1], "b");
    }
}

TEST(SchedulerParallel, DiamondExecutionParallel) {
    reset_state();

    Scheduler scheduler;
    scheduler.enable_parallel(4);

    // Diamond: A -> {B, C} -> D
    // B and C should run in parallel. A before both. D after both.

    auto id_a = scheduler.add_system(Schedule::Update, []() {
        log_entry("a");
    }, "a").id();

    auto id_b = scheduler.add_system(Schedule::Update, []() {
        track_concurrency();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        log_entry("b");
        untrack_concurrency();
    }, "b").after(id_a).id();

    auto id_c = scheduler.add_system(Schedule::Update, []() {
        track_concurrency();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        log_entry("c");
        untrack_concurrency();
    }, "c").after(id_a).id();

    scheduler.add_system(Schedule::Update, []() {
        log_entry("d");
    }, "d").after(id_b).after(id_c);

    World world;
    scheduler.run(world, Schedule::Update);

    {
        std::lock_guard lock(g_log_mutex);
        ASSERT_EQ(g_log.size(), 4u);
        // A must be first
        EXPECT_EQ(g_log[0], "a");
        // D must be last
        EXPECT_EQ(g_log[3], "d");
        // B and C can be in either order (they ran in parallel)
        EXPECT_TRUE((g_log[1] == "b" && g_log[2] == "c") ||
                    (g_log[1] == "c" && g_log[2] == "b"));
    }
}

TEST(SchedulerParallel, ManyIndependentSystemsParallel) {
    reset_state();

    Scheduler scheduler;
    scheduler.enable_parallel(4);

    constexpr int N = 20;
    std::atomic<int> counter{0};

    for (int i = 0; i < N; ++i) {
        scheduler.add_system(Schedule::Update, [&counter]() {
            counter.fetch_add(1);
        });
    }

    World world;
    scheduler.run(world, Schedule::Update);

    EXPECT_EQ(counter.load(), N);
}

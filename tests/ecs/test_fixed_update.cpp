#include <gtest/gtest.h>
#include <cmath>
#include "helios/ecs/app.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/time.h"

using namespace helios;

TEST(FixedUpdate, AccumulatorBasic) {
    // Configure a 60Hz fixed timestep (16.67ms)
    App app;

    auto& acc = app.world().resource<FixedTimeAccumulator>();
    acc.timestep = 1.0f / 60.0f; // ~16.67ms

    static int fixed_tick_count = 0;
    fixed_tick_count = 0;

    app.add_system(Schedule::FixedUpdate, []() {
        fixed_tick_count++;
    });

    // Simulate 2.5 ticks worth of remaining time
    acc.remaining = 2.5f * acc.timestep;

    // Run just the fixed update portion by calling the scheduler directly
    auto& scheduler = app.scheduler();
    while (acc.remaining >= acc.timestep) {
        scheduler.run(app.world(), Schedule::FixedUpdate);
        acc.remaining -= acc.timestep;
    }
    acc.alpha = acc.remaining / acc.timestep;

    EXPECT_EQ(fixed_tick_count, 2); // 2 full ticks, 0.5 remains
    EXPECT_NEAR(acc.alpha, 0.5f, 0.01f);
}

TEST(FixedUpdate, ZeroRemaining) {
    App app;
    auto& acc = app.world().resource<FixedTimeAccumulator>();
    acc.timestep = 1.0f / 60.0f;
    acc.remaining = 0.0f;

    static int count = 0;
    count = 0;

    app.add_system(Schedule::FixedUpdate, []() {
        count++;
    });

    // With zero remaining, FixedUpdate should not tick
    auto& scheduler = app.scheduler();
    while (acc.remaining >= acc.timestep) {
        scheduler.run(app.world(), Schedule::FixedUpdate);
        acc.remaining -= acc.timestep;
    }

    EXPECT_EQ(count, 0);
}

TEST(FixedUpdate, ExactMultiple) {
    App app;
    auto& acc = app.world().resource<FixedTimeAccumulator>();
    acc.timestep = 1.0f / 60.0f;
    acc.remaining = 3.0f * acc.timestep; // exactly 3 ticks

    static int count = 0;
    count = 0;

    app.add_system(Schedule::FixedUpdate, []() {
        count++;
    });

    auto& scheduler = app.scheduler();
    while (acc.remaining >= acc.timestep) {
        scheduler.run(app.world(), Schedule::FixedUpdate);
        acc.remaining -= acc.timestep;
    }

    acc.alpha = (acc.timestep > 0.0f) ? (acc.remaining / acc.timestep) : 0.0f;

    EXPECT_EQ(count, 3);
    EXPECT_LT(acc.alpha, 0.01f); // nearly zero remaining
}

TEST(FixedUpdate, LargeDeltaCap) {
    // If the frame delta is very large (e.g. breakpoint, alt-tab), the
    // accumulator would tick many times.
    App app;
    auto& acc = app.world().resource<FixedTimeAccumulator>();
    acc.timestep = 1.0f / 60.0f;
    acc.remaining = 100.0f * acc.timestep; // 100 ticks

    static int count = 0;
    count = 0;

    app.add_system(Schedule::FixedUpdate, []() {
        count++;
    });

    auto& scheduler = app.scheduler();
    while (acc.remaining >= acc.timestep) {
        scheduler.run(app.world(), Schedule::FixedUpdate);
        acc.remaining -= acc.timestep;
    }

    EXPECT_EQ(count, 100);
}

TEST(FixedUpdate, InterpolationAlphaRange) {
    App app;
    auto& acc = app.world().resource<FixedTimeAccumulator>();
    acc.timestep = 1.0f / 60.0f;

    // Test various fractional remainders
    for (float frac = 0.0f; frac < 1.0f; frac += 0.1f) {
        acc.remaining = frac * acc.timestep;
        acc.alpha = acc.remaining / acc.timestep;
        EXPECT_GE(acc.alpha, 0.0f);
        EXPECT_LE(acc.alpha, 1.0f);
    }
}

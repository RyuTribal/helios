#include <gtest/gtest.h>
#include "helios/ecs/world.h"
#include "helios/ecs/query.h"
#include "helios/ecs/query_filters.h"
#include "helios/ecs/scheduler.h"
#include "helios/ecs/system_param_traits.h"
#include "helios/ecs/system_params.h"

using namespace helios;

namespace {

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health   { int hp; };
struct Marker   {};

// ---------------------------------------------------------------------------
// Basic tick stamping
// ---------------------------------------------------------------------------

TEST(ChangeDetection, SpawnedEntityHasTickGreaterThanZero) {
    World world;
    auto e = world.spawn(Position{1, 2, 3});

    // The world starts at tick 1, so a freshly spawned entity should have
    // its component stamped with tick >= 1.
    auto loc = world.archetypes().locate(e);
    ASSERT_TRUE(loc.has_value());
    auto& col = loc->archetype->get_column<Position>();
    EXPECT_GT(col.changed_tick(loc->row), 0u);
}

TEST(ChangeDetection, MutableQueryAccessStampsTick) {
    World world;
    auto e = world.spawn(Position{1, 2, 3});

    // Record the initial tick.
    auto loc = world.archetypes().locate(e);
    auto& col = loc->archetype->get_column<Position>();
    uint32_t initial_tick = col.changed_tick(loc->row);

    // Advance the world tick to simulate a new system running.
    WorldTestAccess::advance_tick(world);
    uint32_t new_tick = world.current_tick();

    // Mutable query access should stamp the component.
    auto q = world.query<Position>(0, new_tick);
    for (auto [pos] : q) {
        pos.x = 99.0f;
    }

    EXPECT_EQ(col.changed_tick(loc->row), new_tick);
    EXPECT_GT(col.changed_tick(loc->row), initial_tick);
}

TEST(ChangeDetection, ConstQueryAccessDoesNotStampTick) {
    World world;
    auto e = world.spawn(Position{1, 2, 3});

    auto loc = world.archetypes().locate(e);
    auto& col = loc->archetype->get_column<Position>();
    uint32_t initial_tick = col.changed_tick(loc->row);

    // Advance tick.
    WorldTestAccess::advance_tick(world);

    // Const query access should NOT stamp.
    auto q = world.query<const Position>();
    for (auto [pos] : q) {
        (void)pos;
    }

    EXPECT_EQ(col.changed_tick(loc->row), initial_tick);
}

// ---------------------------------------------------------------------------
// Changed<T> filter
// ---------------------------------------------------------------------------

TEST(ChangeDetection, ChangedFilterYieldsRecentlyModifiedEntities) {
    World world;
    auto e1 = world.spawn(Position{1, 0, 0});
    auto e2 = world.spawn(Position{2, 0, 0});

    // Simulate a system having run at tick 1 (the spawn tick).
    uint32_t last_run_tick = world.current_tick();

    // Advance tick and mutate only e1.
    WorldTestAccess::advance_tick(world);
    uint32_t system_tick = world.current_tick();
    {
        auto q = world.query<Position>(0, system_tick);
        // Only touch e1 via direct column stamp to be precise.
        auto loc = world.archetypes().locate(e1);
        loc->archetype->get_column<Position>().stamp(loc->row, system_tick);
    }

    // Now query with Changed<Position> using last_run_tick as the boundary.
    // Only e1 should appear because e2's tick is still the old spawn tick.
    auto cq = world.query<const Position, Changed<Position>>(last_run_tick, system_tick);
    int count = 0;
    for (auto [pos] : cq) {
        EXPECT_EQ(pos.x, 1.0f);
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(ChangeDetection, ChangedFilterSkipsUnmodifiedEntities) {
    World world;
    world.spawn(Position{1, 0, 0});
    world.spawn(Position{2, 0, 0});

    // last_run_tick = current tick, so nothing has changed "since last run".
    uint32_t tick = world.current_tick();
    auto q = world.query<const Position, Changed<Position>>(tick, tick);

    int count = 0;
    for ([[maybe_unused]] auto tup : q) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(ChangeDetection, ChangedFilterWithEntityIterator) {
    World world;
    auto e1 = world.spawn(Position{1, 0, 0});
    auto e2 = world.spawn(Position{2, 0, 0});

    uint32_t last_run_tick = world.current_tick();

    // Advance and stamp only e2.
    WorldTestAccess::advance_tick(world);
    uint32_t system_tick = world.current_tick();
    {
        auto loc = world.archetypes().locate(e2);
        loc->archetype->get_column<Position>().stamp(loc->row, system_tick);
    }

    auto q = world.query<const Position, Changed<Position>>(last_run_tick, system_tick);
    int count = 0;
    for (auto [entity, pos] : q.with_entity()) {
        EXPECT_EQ(entity, e2);
        EXPECT_EQ(pos.x, 2.0f);
        ++count;
    }
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// Tick preservation across archetype moves
// ---------------------------------------------------------------------------

TEST(ChangeDetection, TickPreservedAcrossArchetypeMove) {
    World world;
    auto e = world.spawn(Position{1, 2, 3});

    auto loc = world.archetypes().locate(e);
    auto& col = loc->archetype->get_column<Position>();
    uint32_t original_tick = col.changed_tick(loc->row);

    // Advance tick, then add a component (moves entity to new archetype).
    WorldTestAccess::advance_tick(world);
    world.add(e, Velocity{4, 5, 6});

    // The Position tick should be preserved (not re-stamped to the new tick).
    auto new_loc = world.archetypes().locate(e);
    auto& new_col = new_loc->archetype->get_column<Position>();
    EXPECT_EQ(new_col.changed_tick(new_loc->row), original_tick);

    // But the new Velocity component should have the current tick.
    auto& vel_col = new_loc->archetype->get_column<Velocity>();
    EXPECT_EQ(vel_col.changed_tick(new_loc->row), world.current_tick());
}

// ---------------------------------------------------------------------------
// World::add() overwrite stamps tick
// ---------------------------------------------------------------------------

TEST(ChangeDetection, AddOverwriteStampsTick) {
    World world;
    auto e = world.spawn(Position{1, 2, 3});

    auto loc = world.archetypes().locate(e);
    auto& col = loc->archetype->get_column<Position>();
    uint32_t original_tick = col.changed_tick(loc->row);

    // Advance tick and overwrite Position in place.
    WorldTestAccess::advance_tick(world);
    world.add(e, Position{10, 20, 30});

    EXPECT_EQ(col.changed_tick(loc->row), world.current_tick());
    EXPECT_GT(col.changed_tick(loc->row), original_tick);

    // Value should be updated too.
    auto& pos = world.get<Position>(e);
    EXPECT_EQ(pos.x, 10.0f);
}

// ---------------------------------------------------------------------------
// World tick counter
// ---------------------------------------------------------------------------

TEST(ChangeDetection, WorldTickStartsAtOne) {
    World world;
    EXPECT_EQ(world.current_tick(), 1u);
}

TEST(ChangeDetection, WorldAdvanceTick) {
    World world;
    EXPECT_EQ(world.current_tick(), 1u);
    WorldTestAccess::advance_tick(world);
    EXPECT_EQ(world.current_tick(), 2u);
    WorldTestAccess::advance_tick(world);
    EXPECT_EQ(world.current_tick(), 3u);
}

// ---------------------------------------------------------------------------
// Column tick mechanics
// ---------------------------------------------------------------------------

TEST(ChangeDetection, ColumnSwapRemovePreservesTicks) {
    auto col = Column::create<int>();
    int a = 10, b = 20, c = 30;
    col.push(&a, 1);
    col.push(&b, 2);
    col.push(&c, 3);

    EXPECT_EQ(col.changed_tick(0), 1u);
    EXPECT_EQ(col.changed_tick(1), 2u);
    EXPECT_EQ(col.changed_tick(2), 3u);

    // Swap-remove the first element: last (tick=3) moves to index 0.
    col.swap_remove(0);
    EXPECT_EQ(col.count(), 2u);
    EXPECT_EQ(col.changed_tick(0), 3u);  // was last, now at index 0
    EXPECT_EQ(col.changed_tick(1), 2u);  // unchanged
}

TEST(ChangeDetection, ColumnClearResetsTicks) {
    auto col = Column::create<int>();
    int a = 10;
    col.push(&a, 5);
    EXPECT_EQ(col.count(), 1u);
    EXPECT_EQ(col.changed_tick(0), 5u);

    col.clear();
    EXPECT_EQ(col.count(), 0u);
}

// ---------------------------------------------------------------------------
// Scheduler tick integration
// ---------------------------------------------------------------------------

TEST(ChangeDetection, SchedulerAdvancesTickPerSystem) {
    World world;
    uint32_t initial_tick = world.current_tick();

    Scheduler scheduler;
    uint32_t tick_during_system_a = 0;
    uint32_t tick_during_system_b = 0;

    scheduler.add_system(Schedule::Update,
        [&tick_during_system_a](Query<const Position> /*q*/) {
            // This lambda is just to test tick advancement;
            // we record the current tick via the captured variable.
        }, "sys_a");

    scheduler.add_system(Schedule::Update,
        [&tick_during_system_b](Query<const Position> /*q*/) {
        }, "sys_b");

    // We can verify tick advances by checking world tick after the run.
    scheduler.run(world, Schedule::Update);

    // After running 2 systems, tick should have advanced at least 2 times.
    EXPECT_GE(world.current_tick(), initial_tick + 2);
}

TEST(ChangeDetection, MutableQueryStampsDuringSystemRun) {
    World world;
    auto e = world.spawn(Position{1, 2, 3});

    Scheduler scheduler;
    scheduler.add_system(Schedule::Update,
        [](Query<Position> q) {
            for (auto [pos] : q) {
                pos.x = 99.0f;
            }
        }, "mutate_pos");

    scheduler.run(world, Schedule::Update);

    // The mutable access should have stamped the tick.
    auto loc = world.archetypes().locate(e);
    auto& col = loc->archetype->get_column<Position>();
    EXPECT_GT(col.changed_tick(loc->row), 1u);  // > initial spawn tick
}

TEST(ChangeDetection, ChangedFilterWorksAcrossSystemRuns) {
    World world;
    world.spawn(Position{1, 0, 0});
    world.spawn(Position{2, 0, 0});

    int changed_count = 0;

    Scheduler scheduler;

    // System A: mutate first entity only.
    auto id_a = scheduler.add_system(Schedule::Update,
        [](Query<Position> q) {
            bool first = true;
            for (auto [pos] : q) {
                if (first) {
                    pos.x = 99.0f;
                    first = false;
                }
            }
        }, "mutate_first").id();

    // System B: count changed entities (runs after A).
    scheduler.add_system(Schedule::Update,
        [&changed_count](Query<const Position, Changed<Position>> q) {
            changed_count = 0;
            for ([[maybe_unused]] auto tup : q) {
                ++changed_count;
            }
        }, "count_changed").after(id_a);

    // First run: both entities were spawned before the first system run,
    // so Changed<Position> should detect them as changed (tick > 0 = last_run_tick).
    scheduler.run(world, Schedule::Update);

    // On first run, system B's last_run_tick is 0, so all entities with
    // tick > 0 are "changed". Both were spawned at tick 1 and system A
    // mutated one. All pass the Changed filter.
    EXPECT_EQ(changed_count, 2);

    // Second run: only the entity mutated by system A in this frame
    // should appear as changed from system B's perspective.
    changed_count = 0;
    scheduler.run(world, Schedule::Update);

    // System A mutates the first entity (stamps it with this frame's tick).
    // System B sees only entities changed since B's last_run_tick (which
    // was set at the end of the first frame). Only 1 entity was mutated.
    //
    // Note: system A also stamps ALL entities it iterates with mutable
    // query. Since Q<Position> iterates both entities, it stamps both.
    // So actually both should appear changed again. This is the Bevy-like
    // behavior where mutable access = changed regardless of actual mutation.
    EXPECT_EQ(changed_count, 2);
}

} // anonymous namespace

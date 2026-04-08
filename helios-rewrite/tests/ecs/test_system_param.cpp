#include <gtest/gtest.h>
#include <typeindex>
#include <vector>
// Include complete types needed by SystemParamExtractor accesses/fetch
#include "helios/ecs/world.h"
#include "helios/ecs/system_params.h"
#include "helios/ecs/commands.h"
#include "helios/ecs/event_storage.h"
#include "helios/ecs/system_param_traits.h"

using namespace helios;

// ---- Types used in system signatures ----
struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct TimeRes  { float delta; };
struct MyEvent  { int data; };

// ---- Test free functions ----
void system_readonly(Query<const Position, const Velocity> /*q*/, Res<TimeRes> /*t*/) {}
void system_readwrite(Query<Position, const Velocity> /*q*/) {}
void system_resource_write(ResMut<TimeRes> /*t*/) {}
void system_commands_only(Commands& /*cmd*/) {}
void system_events(EventReader<MyEvent> /*r*/, EventWriter<MyEvent> /*w*/) {}

// ---- Helper ----
static bool has_access(const std::vector<AccessDescriptor>& v,
                       std::type_index type, AccessMode mode) {
    for (const auto& a : v) {
        if (a.type == type && a.mode == mode) return true;
    }
    return false;
}

TEST(SystemParamTraits, ReadonlyQueryAndResource) {
    auto accesses = SystemParamExtractor<decltype(&system_readonly)>::accesses();

    // Query<const Position, const Velocity> -> Read Position, Read Velocity
    EXPECT_TRUE(has_access(accesses, typeid(Position), AccessMode::Read));
    EXPECT_TRUE(has_access(accesses, typeid(Velocity), AccessMode::Read));

    // Res<TimeRes> -> Read TimeRes
    EXPECT_TRUE(has_access(accesses, typeid(TimeRes), AccessMode::Read));

    // Should not have any writes
    for (const auto& a : accesses) {
        EXPECT_EQ(a.mode, AccessMode::Read);
    }
}

TEST(SystemParamTraits, ReadWriteQuery) {
    auto accesses = SystemParamExtractor<decltype(&system_readwrite)>::accesses();

    // Query<Position, const Velocity> -> Write Position, Read Velocity
    EXPECT_TRUE(has_access(accesses, typeid(Position), AccessMode::Write));
    EXPECT_TRUE(has_access(accesses, typeid(Velocity), AccessMode::Read));
}

TEST(SystemParamTraits, ResourceWrite) {
    auto accesses = SystemParamExtractor<decltype(&system_resource_write)>::accesses();
    EXPECT_EQ(accesses.size(), 1u);
    EXPECT_TRUE(has_access(accesses, typeid(TimeRes), AccessMode::Write));
}

TEST(SystemParamTraits, CommandsWriteAccess) {
    auto accesses = SystemParamExtractor<decltype(&system_commands_only)>::accesses();
    // Commands reports Write access so the DAG serializes all command-using systems.
    EXPECT_EQ(accesses.size(), 1u);
    EXPECT_TRUE(has_access(accesses, typeid(Commands), AccessMode::Write));
}

TEST(SystemParamTraits, EventReaderWriter) {
    auto accesses = SystemParamExtractor<decltype(&system_events)>::accesses();
    // Both EventReader<MyEvent> and EventWriter<MyEvent> report using typeid(MyEvent)
    // so the DAG scheduler can detect reader-writer and writer-writer conflicts on
    // the same event type T.
    EXPECT_TRUE(has_access(accesses, typeid(MyEvent), AccessMode::Read));
    EXPECT_TRUE(has_access(accesses, typeid(MyEvent), AccessMode::Write));
    // Must NOT use the wrapper types as the conflict key
    EXPECT_FALSE(has_access(accesses, typeid(EventReader<MyEvent>), AccessMode::Read));
    EXPECT_FALSE(has_access(accesses, typeid(EventWriter<MyEvent>), AccessMode::Write));
}

TEST(SystemParamTraits, Lambda) {
    auto lambda = [](Res<TimeRes> /*t*/, ResMut<Position> /*p*/) {};
    auto accesses = SystemParamExtractor<decltype(lambda)>::accesses();

    EXPECT_TRUE(has_access(accesses, typeid(TimeRes),  AccessMode::Read));
    EXPECT_TRUE(has_access(accesses, typeid(Position), AccessMode::Write));
}

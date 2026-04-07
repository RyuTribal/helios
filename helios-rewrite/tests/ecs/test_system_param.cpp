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
void system_commands_only(Commands /*cmd*/) {}
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

TEST(SystemParamTraits, CommandsNoAccess) {
    auto accesses = SystemParamExtractor<decltype(&system_commands_only)>::accesses();
    EXPECT_TRUE(accesses.empty());
}

TEST(SystemParamTraits, EventReaderWriter) {
    auto accesses = SystemParamExtractor<decltype(&system_events)>::accesses();
    // EventReader<MyEvent> -> Read on EventReader<MyEvent>
    EXPECT_TRUE(has_access(accesses, typeid(EventReader<MyEvent>), AccessMode::Read));
    // EventWriter<MyEvent> -> Write on EventWriter<MyEvent>
    EXPECT_TRUE(has_access(accesses, typeid(EventWriter<MyEvent>), AccessMode::Write));
}

TEST(SystemParamTraits, Lambda) {
    auto lambda = [](Res<TimeRes> /*t*/, ResMut<Position> /*p*/) {};
    auto accesses = SystemParamExtractor<decltype(lambda)>::accesses();

    EXPECT_TRUE(has_access(accesses, typeid(TimeRes),  AccessMode::Read));
    EXPECT_TRUE(has_access(accesses, typeid(Position), AccessMode::Write));
}

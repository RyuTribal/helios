#include <gtest/gtest.h>
#include <typeindex>
#include <vector>
#include "helios/ecs/access_descriptor.h"

using namespace helios;

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health   { int hp; };

TEST(AccessDescriptor, SameTypeReadReadNoConflict) {
    AccessDescriptor a{ std::type_index(typeid(Position)), AccessMode::Read };
    AccessDescriptor b{ std::type_index(typeid(Position)), AccessMode::Read };
    EXPECT_FALSE(a.conflicts_with(b));
}

TEST(AccessDescriptor, SameTypeReadWriteConflict) {
    AccessDescriptor a{ std::type_index(typeid(Position)), AccessMode::Read };
    AccessDescriptor b{ std::type_index(typeid(Position)), AccessMode::Write };
    EXPECT_TRUE(a.conflicts_with(b));
}

TEST(AccessDescriptor, SameTypeWriteWriteConflict) {
    AccessDescriptor a{ std::type_index(typeid(Position)), AccessMode::Write };
    AccessDescriptor b{ std::type_index(typeid(Position)), AccessMode::Write };
    EXPECT_TRUE(a.conflicts_with(b));
}

TEST(AccessDescriptor, DifferentTypesNoConflict) {
    AccessDescriptor a{ std::type_index(typeid(Position)), AccessMode::Write };
    AccessDescriptor b{ std::type_index(typeid(Velocity)), AccessMode::Write };
    EXPECT_FALSE(a.conflicts_with(b));
}

TEST(AccessDescriptor, HasConflictEmptyLists) {
    std::vector<AccessDescriptor> a;
    std::vector<AccessDescriptor> b;
    EXPECT_FALSE(has_conflict(a, b));
}

TEST(AccessDescriptor, HasConflictMixedLists) {
    std::vector<AccessDescriptor> a = {
        { std::type_index(typeid(Position)), AccessMode::Read },
        { std::type_index(typeid(Health)),   AccessMode::Write },
    };
    std::vector<AccessDescriptor> b = {
        { std::type_index(typeid(Velocity)), AccessMode::Write },
        { std::type_index(typeid(Health)),   AccessMode::Read },
    };
    // Health: Write in a, Read in b -> conflict
    EXPECT_TRUE(has_conflict(a, b));
}

TEST(AccessDescriptor, HasConflictNoOverlap) {
    std::vector<AccessDescriptor> a = {
        { std::type_index(typeid(Position)), AccessMode::Write },
    };
    std::vector<AccessDescriptor> b = {
        { std::type_index(typeid(Velocity)), AccessMode::Write },
    };
    EXPECT_FALSE(has_conflict(a, b));
}

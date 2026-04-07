#include <gtest/gtest.h>
#include <unordered_set>
#include "helios/ecs/entity.h"

using namespace helios;

TEST(Entity, DefaultIsInvalid) {
    Entity e{};
    EXPECT_FALSE(static_cast<bool>(e));
    EXPECT_EQ(e, Entity::INVALID);
}

TEST(Entity, ValidEntityIsTrue) {
    Entity e{1, 1};
    EXPECT_TRUE(static_cast<bool>(e));
}

TEST(Entity, EqualityComparison) {
    Entity a{1, 1};
    Entity b{1, 1};
    Entity c{2, 1};
    Entity d{1, 2};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(Entity, InvalidConstant) {
    EXPECT_EQ(Entity::INVALID.index, 0u);
    EXPECT_EQ(Entity::INVALID.generation, 0u);
    EXPECT_FALSE(static_cast<bool>(Entity::INVALID));
}

TEST(Entity, Hashable) {
    std::unordered_set<Entity> set;
    set.insert(Entity{1, 1});
    set.insert(Entity{2, 1});
    set.insert(Entity{1, 1});
    EXPECT_EQ(set.size(), 2u);
}

TEST(Entity, DifferentGenerationsHashDifferently) {
    std::hash<Entity> hasher;
    EXPECT_NE(hasher(Entity{5, 1}), hasher(Entity{5, 2}));
}

#include <gtest/gtest.h>
#include "helios/ecs/query.h"
#include <type_traits>
#include <unordered_set>

using namespace helios;

namespace {

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health   { int hp; };
struct Disabled {};

// ---------------------------------------------------------------------------
// Test fixture – provides a storage and entity spawning helpers.
// ---------------------------------------------------------------------------
class QueryTest : public ::testing::Test {
protected:
    ArchetypeStorage storage;
    uint32_t next_index = 0;

    void SetUp() override {
        storage.register_component<Position>();
        storage.register_component<Velocity>();
        storage.register_component<Health>();
        storage.register_component<Disabled>();
    }

    Entity next_entity() {
        return Entity{next_index++, 1};
    }

    template <typename... Ts>
    Entity spawn(Ts... components) {
        Entity e = next_entity();
        auto id = make_archetype_id<Ts...>();
        Archetype& arch = storage.get_or_create(id);
        storage.add_entity(arch, e);
        (arch.get_column<Ts>().push(&components), ...);
        return e;
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(QueryTest, BasicIteration) {
    auto e0 = spawn(Position{1, 2, 3}, Velocity{4, 5, 6});
    auto e1 = spawn(Position{7, 8, 9}, Velocity{10, 11, 12});

    Query<Position, Velocity> q(storage);

    int count = 0;
    std::unordered_set<float> xs;
    for (auto [pos, vel] : q) {
        xs.insert(pos.x);
        ++count;
    }
    EXPECT_EQ(count, 2);
    EXPECT_TRUE(xs.count(1.0f));
    EXPECT_TRUE(xs.count(7.0f));
}

TEST_F(QueryTest, ConstAccess) {
    spawn(Position{1, 2, 3}, Velocity{4, 5, 6});

    Query<const Position, const Velocity> q(storage);
    for (auto [pos, vel] : q) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(pos)>>,
                      "Position must be const");
        static_assert(std::is_const_v<std::remove_reference_t<decltype(vel)>>,
                      "Velocity must be const");
        EXPECT_EQ(pos.x, 1.0f);
    }
}

TEST_F(QueryTest, MixedConstMutable) {
    spawn(Position{1, 2, 3}, Velocity{4, 5, 6});

    Query<Position, const Velocity> q(storage);
    for (auto [pos, vel] : q) {
        static_assert(!std::is_const_v<std::remove_reference_t<decltype(pos)>>,
                      "Position must be mutable");
        static_assert(std::is_const_v<std::remove_reference_t<decltype(vel)>>,
                      "Velocity must be const");
        pos.x = 99.0f;
    }

    // Verify the mutation stuck.
    Query<const Position> q2(storage);
    for (auto [pos] : q2) {
        EXPECT_EQ(pos.x, 99.0f);
    }
}

TEST_F(QueryTest, MatchesMultipleArchetypes) {
    // Three different archetypes, all containing Position.
    spawn(Position{1, 0, 0});
    spawn(Position{2, 0, 0}, Velocity{0, 0, 0});
    spawn(Position{3, 0, 0}, Health{100});

    Query<Position> q(storage);
    EXPECT_EQ(q.count(), 3u);

    std::unordered_set<float> xs;
    for (auto [pos] : q) {
        xs.insert(pos.x);
    }
    EXPECT_TRUE(xs.count(1.0f));
    EXPECT_TRUE(xs.count(2.0f));
    EXPECT_TRUE(xs.count(3.0f));
}

TEST_F(QueryTest, WithFilter) {
    // Entity with Position only.
    spawn(Position{1, 0, 0});
    // Entity with Position + Velocity.
    spawn(Position{2, 0, 0}, Velocity{0, 0, 0});

    Query<Position, With<Velocity>> q(storage);
    EXPECT_EQ(q.count(), 1u);

    for (auto [pos] : q) {
        EXPECT_EQ(pos.x, 2.0f);
    }
}

TEST_F(QueryTest, WithoutFilter) {
    spawn(Position{1, 0, 0});
    spawn(Position{2, 0, 0}, Disabled{});

    Query<Position, Without<Disabled>> q(storage);
    EXPECT_EQ(q.count(), 1u);

    for (auto [pos] : q) {
        EXPECT_EQ(pos.x, 1.0f);
    }
}

TEST_F(QueryTest, OptionalPresent) {
    spawn(Position{1, 0, 0}, Health{42});

    Query<Position, Optional<Health>> q(storage);
    EXPECT_EQ(q.count(), 1u);

    for (auto [pos, hp] : q) {
        ASSERT_NE(hp, nullptr);
        EXPECT_EQ(hp->hp, 42);
    }
}

TEST_F(QueryTest, OptionalAbsent) {
    spawn(Position{1, 0, 0});

    Query<Position, Optional<Health>> q(storage);
    EXPECT_EQ(q.count(), 1u);

    for (auto [pos, hp] : q) {
        EXPECT_EQ(hp, nullptr);
    }
}

TEST_F(QueryTest, EmptyResult) {
    spawn(Position{1, 0, 0});

    Query<Velocity> q(storage);
    EXPECT_EQ(q.count(), 0u);
    EXPECT_TRUE(q.is_empty());

    int count = 0;
    for ([[maybe_unused]] auto tup : q) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST_F(QueryTest, GetSpecificEntity) {
    auto e0 = spawn(Position{1, 2, 3}, Velocity{4, 5, 6});
    auto e1 = spawn(Position{7, 8, 9}, Velocity{10, 11, 12});

    Query<Position, Velocity> q(storage);

    auto result = q.get(e0);
    ASSERT_TRUE(result.has_value());
    auto& [pos, vel] = *result;
    EXPECT_EQ(pos.x, 1.0f);
    EXPECT_EQ(vel.dx, 4.0f);

    // Non-existent entity.
    Entity phantom{999, 1};
    EXPECT_FALSE(q.get(phantom).has_value());
}

TEST_F(QueryTest, MutationDuringIteration) {
    spawn(Position{1, 0, 0}, Velocity{10, 0, 0});
    spawn(Position{2, 0, 0}, Velocity{20, 0, 0});

    Query<Position, const Velocity> q(storage);
    for (auto [pos, vel] : q) {
        pos.x += vel.dx;
    }

    // Verify results.
    Query<const Position> q2(storage);
    std::unordered_set<float> xs;
    for (auto [pos] : q2) {
        xs.insert(pos.x);
    }
    EXPECT_TRUE(xs.count(11.0f));
    EXPECT_TRUE(xs.count(22.0f));
}

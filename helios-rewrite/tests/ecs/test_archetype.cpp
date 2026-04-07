#include <gtest/gtest.h>
#include "helios/ecs/archetype.h"

using namespace helios;

namespace {

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health { int hp; };

std::unordered_map<ComponentId, std::function<Column()>> make_factories() {
    return {
        {component_id<Position>(), [] { return Column::create<Position>(); }},
        {component_id<Velocity>(), [] { return Column::create<Velocity>(); }},
        {component_id<Health>(),   [] { return Column::create<Health>(); }},
    };
}

Archetype make_pv_archetype() {
    auto factories = make_factories();
    return create_archetype(make_archetype_id<Position, Velocity>(), factories);
}

} // anonymous namespace

TEST(Archetype, CreateEmpty) {
    auto arch = make_pv_archetype();
    EXPECT_TRUE(arch.empty());
    EXPECT_EQ(arch.size(), 0u);
    EXPECT_EQ(arch.columns.size(), 2u);
}

TEST(Archetype, HasComponent) {
    auto arch = make_pv_archetype();
    EXPECT_TRUE(arch.has_component(component_id<Position>()));
    EXPECT_TRUE(arch.has_component(component_id<Velocity>()));
    EXPECT_FALSE(arch.has_component(component_id<Health>()));
}

TEST(Archetype, HasAll) {
    auto arch = make_pv_archetype();
    EXPECT_TRUE((arch.has_all<Position, Velocity>()));
    EXPECT_FALSE((arch.has_all<Position, Health>()));
}

TEST(Archetype, HasNone) {
    auto arch = make_pv_archetype();
    EXPECT_TRUE((arch.has_none<Health>()));
    EXPECT_FALSE((arch.has_none<Position>()));
}

TEST(Archetype, PushAndAccess) {
    auto arch = make_pv_archetype();
    Entity e{1, 1};
    arch.entities.push_back(e);
    Position p{1.0f, 2.0f, 3.0f};
    Velocity v{4.0f, 5.0f, 6.0f};
    arch.get_column<Position>().push(&p);
    arch.get_column<Velocity>().push(&v);

    EXPECT_EQ(arch.size(), 1u);
    EXPECT_EQ(arch.get<Position>(0).x, 1.0f);
    EXPECT_EQ(arch.get<Velocity>(0).dy, 5.0f);
}

TEST(Archetype, SwapRemoveLastElement) {
    auto arch = make_pv_archetype();
    Entity e{1, 1};
    arch.entities.push_back(e);
    Position p{1.0f, 2.0f, 3.0f};
    Velocity v{4.0f, 5.0f, 6.0f};
    arch.get_column<Position>().push(&p);
    arch.get_column<Velocity>().push(&v);

    arch.swap_remove(0);
    EXPECT_TRUE(arch.empty());
}

TEST(Archetype, SwapRemoveMiddleElement) {
    auto arch = make_pv_archetype();
    for (int i = 0; i < 3; ++i) {
        Entity e{static_cast<uint32_t>(i), 1};
        arch.entities.push_back(e);
        Position p{static_cast<float>(i), 0, 0};
        Velocity v{0, static_cast<float>(i), 0};
        arch.get_column<Position>().push(&p);
        arch.get_column<Velocity>().push(&v);
    }

    arch.swap_remove(0);
    EXPECT_EQ(arch.size(), 2u);
    EXPECT_EQ(arch.entities[0].index, 2u);
    EXPECT_EQ(arch.get<Position>(0).x, 2.0f);
}

TEST(Archetype, GetColumn) {
    auto arch = make_pv_archetype();
    Entity e{1, 1};
    arch.entities.push_back(e);
    Position p{10.0f, 20.0f, 30.0f};
    Velocity v{1.0f, 2.0f, 3.0f};
    arch.get_column<Position>().push(&p);
    arch.get_column<Velocity>().push(&v);

    Column& col = arch.get_column<Position>();
    EXPECT_EQ(col.get<Position>(0).x, 10.0f);
}

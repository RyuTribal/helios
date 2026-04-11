#include <gtest/gtest.h>
#include "helios/ecs/archetype_storage.h"

using namespace helios;

namespace {

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health { int hp; };

ArchetypeStorage make_storage() {
    ArchetypeStorage storage;
    storage.register_component<Position>();
    storage.register_component<Velocity>();
    storage.register_component<Health>();
    return storage;
}

} // anonymous namespace

TEST(ArchetypeStorage, GetOrCreateCreatesArchetype) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position, Velocity>();
    Archetype& arch = storage.get_or_create(id);

    EXPECT_EQ(arch.id, id);
    EXPECT_EQ(arch.columns.size(), 2u);
    EXPECT_TRUE(arch.has_component(component_id<Position>()));
    EXPECT_TRUE(arch.has_component(component_id<Velocity>()));
    EXPECT_TRUE(arch.empty());
}

TEST(ArchetypeStorage, GetOrCreateReturnsSame) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position, Velocity>();
    Archetype& first = storage.get_or_create(id);
    Archetype& second = storage.get_or_create(id);

    EXPECT_EQ(&first, &second);
}

TEST(ArchetypeStorage, AddEntity) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position>();
    Archetype& arch = storage.get_or_create(id);

    Entity e{1, 1};
    size_t row = storage.add_entity(arch, e);
    EXPECT_EQ(row, 0u);
    EXPECT_EQ(arch.entities.size(), 1u);
    EXPECT_EQ(arch.entities[0], e);

    auto loc = storage.locate(e);
    ASSERT_TRUE(loc.has_value());
    EXPECT_EQ(loc->archetype, &arch);
    EXPECT_EQ(loc->row, 0u);
}

TEST(ArchetypeStorage, RemoveEntity) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position>();
    Archetype& arch = storage.get_or_create(id);

    // Add three entities with component data.
    Entity e0{0, 1}, e1{1, 1}, e2{2, 1};
    storage.add_entity(arch, e0);
    Position p0{0, 0, 0};
    arch.get_column<Position>().push(&p0);

    storage.add_entity(arch, e1);
    Position p1{1, 1, 1};
    arch.get_column<Position>().push(&p1);

    storage.add_entity(arch, e2);
    Position p2{2, 2, 2};
    arch.get_column<Position>().push(&p2);

    // Remove the middle entity. Last entity (e2) should swap into row 1.
    storage.remove_entity(e1);

    EXPECT_EQ(arch.size(), 2u);
    EXPECT_FALSE(storage.contains(e1));

    // e2 was the last element and should now be at row 1 (e1's old slot).
    auto loc2 = storage.locate(e2);
    ASSERT_TRUE(loc2.has_value());
    EXPECT_EQ(loc2->row, 1u);

    // Component data of e2 should be at row 1.
    EXPECT_EQ(arch.get<Position>(1).x, 2.0f);

    // e0 should be unchanged at row 0.
    auto loc0 = storage.locate(e0);
    ASSERT_TRUE(loc0.has_value());
    EXPECT_EQ(loc0->row, 0u);
    EXPECT_EQ(arch.get<Position>(0).x, 0.0f);
}

TEST(ArchetypeStorage, LocateNonexistent) {
    auto storage = make_storage();
    Entity e{42, 1};
    EXPECT_FALSE(storage.locate(e).has_value());
    EXPECT_FALSE(storage.contains(e));
}

TEST(ArchetypeStorage, ForEachMatchingFindsCorrectArchetypes) {
    auto storage = make_storage();

    auto id_pv = make_archetype_id<Position, Velocity>();
    auto id_ph = make_archetype_id<Position, Health>();
    auto id_h  = make_archetype_id<Health>();

    storage.get_or_create(id_pv);
    storage.get_or_create(id_ph);
    storage.get_or_create(id_h);

    // Query for archetypes containing Position: should match id_pv and id_ph.
    int count = 0;
    storage.for_each_matching<Position>([&](Archetype& arch) {
        EXPECT_TRUE(arch.has_component(component_id<Position>()));
        ++count;
    });
    EXPECT_EQ(count, 2);

    // Query for archetypes containing Position AND Velocity: only id_pv.
    count = 0;
    storage.for_each_matching<Position, Velocity>([&](Archetype& arch) {
        EXPECT_TRUE((arch.has_all<Position, Velocity>()));
        ++count;
    });
    EXPECT_EQ(count, 1);

    // Query for Health: should match id_ph and id_h.
    count = 0;
    storage.for_each_matching<Health>([&](Archetype& arch) {
        EXPECT_TRUE(arch.has_component(component_id<Health>()));
        ++count;
    });
    EXPECT_EQ(count, 2);
}

TEST(ArchetypeStorage, MoveEntityAddsComponent) {
    auto storage = make_storage();

    auto id_p  = make_archetype_id<Position>();
    auto id_pv = make_archetype_id<Position, Velocity>();

    Archetype& arch_p  = storage.get_or_create(id_p);
    Archetype& arch_pv = storage.get_or_create(id_pv);

    // Add entity to Position-only archetype.
    Entity e{1, 1};
    storage.add_entity(arch_p, e);
    Position p{10.0f, 20.0f, 30.0f};
    arch_p.get_column<Position>().push(&p);

    // Move entity from Position -> Position+Velocity.
    storage.move_entity(e, arch_p, arch_pv);

    // Source should be empty.
    EXPECT_TRUE(arch_p.empty());

    // Entity should be in destination.
    auto loc = storage.locate(e);
    ASSERT_TRUE(loc.has_value());
    EXPECT_EQ(loc->archetype, &arch_pv);
    EXPECT_EQ(loc->row, 0u);

    // Position data should have been moved.
    EXPECT_EQ(arch_pv.get<Position>(0).x, 10.0f);
    EXPECT_EQ(arch_pv.get<Position>(0).y, 20.0f);
    EXPECT_EQ(arch_pv.get<Position>(0).z, 30.0f);

    // Velocity column should be one shorter than entities (caller pushes it).
    EXPECT_EQ(arch_pv.entities.size(), 1u);
    EXPECT_EQ(arch_pv.get_column<Velocity>().count(), 0u);

    // Now caller pushes the new component.
    Velocity v{1.0f, 2.0f, 3.0f};
    arch_pv.get_column<Velocity>().push(&v);
    EXPECT_EQ(arch_pv.get<Velocity>(0).dx, 1.0f);
}

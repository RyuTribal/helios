#include <gtest/gtest.h>
#include "helios/ecs/entity_allocator.h"

using namespace helios;

TEST(EntityAllocator, AllocateReturnsValidEntity) {
    EntityAllocator alloc;
    Entity e = alloc.allocate();
    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.generation, 1u);
}

TEST(EntityAllocator, AllocateSequentialIndices) {
    EntityAllocator alloc;
    Entity a = alloc.allocate();
    Entity b = alloc.allocate();
    Entity c = alloc.allocate();
    EXPECT_EQ(a.index, 0u);
    EXPECT_EQ(b.index, 1u);
    EXPECT_EQ(c.index, 2u);
}

TEST(EntityAllocator, IsAlive) {
    EntityAllocator alloc;
    Entity e = alloc.allocate();
    EXPECT_TRUE(alloc.is_alive(e));
}

TEST(EntityAllocator, DeallocateMakesEntityDead) {
    EntityAllocator alloc;
    Entity e = alloc.allocate();
    alloc.deallocate(e);
    EXPECT_FALSE(alloc.is_alive(e));
}

TEST(EntityAllocator, ReusesDeallocatedSlot) {
    EntityAllocator alloc;
    Entity a = alloc.allocate();
    uint32_t old_index = a.index;
    alloc.deallocate(a);
    Entity b = alloc.allocate();
    EXPECT_EQ(b.index, old_index);
    EXPECT_GT(b.generation, a.generation);
}

TEST(EntityAllocator, OldHandleInvalidAfterReuse) {
    EntityAllocator alloc;
    Entity old_entity = alloc.allocate();
    alloc.deallocate(old_entity);
    Entity new_entity = alloc.allocate();
    EXPECT_TRUE(alloc.is_alive(new_entity));
    EXPECT_FALSE(alloc.is_alive(old_entity));
}

TEST(EntityAllocator, AliveCount) {
    EntityAllocator alloc;
    EXPECT_EQ(alloc.alive_count(), 0u);
    Entity a = alloc.allocate();
    Entity b = alloc.allocate();
    EXPECT_EQ(alloc.alive_count(), 2u);
    alloc.deallocate(a);
    EXPECT_EQ(alloc.alive_count(), 1u);
    alloc.allocate();
    EXPECT_EQ(alloc.alive_count(), 2u);
}

TEST(EntityAllocator, InvalidEntityIsNotAlive) {
    EntityAllocator alloc;
    EXPECT_FALSE(alloc.is_alive(Entity::INVALID));
    EXPECT_FALSE(alloc.is_alive(Entity{999, 1}));
}

TEST(EntityAllocator, MultipleReusesBumpGeneration) {
    EntityAllocator alloc;
    Entity e1 = alloc.allocate();
    EXPECT_EQ(e1.generation, 1u);
    alloc.deallocate(e1);
    Entity e2 = alloc.allocate();
    EXPECT_EQ(e2.generation, 2u);
    alloc.deallocate(e2);
    Entity e3 = alloc.allocate();
    EXPECT_EQ(e3.generation, 3u);
    EXPECT_EQ(e3.index, e1.index);
}

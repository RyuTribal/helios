#include <gtest/gtest.h>
#include <memory>
#include "helios/ecs/resource_storage.h"

using namespace helios;

struct Health {
    int value = 100;
};

struct Speed {
    float value = 5.0f;
};

TEST(ResourceStorage, InsertAndGet) {
    ResourceStorage storage;
    storage.insert(Health{42});
    EXPECT_EQ(storage.get<Health>().value, 42);
}

TEST(ResourceStorage, GetConst) {
    ResourceStorage storage;
    storage.insert(Health{77});
    const ResourceStorage& const_storage = storage;
    EXPECT_EQ(const_storage.get<Health>().value, 77);
}

TEST(ResourceStorage, MutateViaGet) {
    ResourceStorage storage;
    storage.insert(Health{10});
    storage.get<Health>().value = 99;
    EXPECT_EQ(storage.get<Health>().value, 99);
}

TEST(ResourceStorage, TryGetPresent) {
    ResourceStorage storage;
    storage.insert(Health{50});
    Health* ptr = storage.try_get<Health>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, 50);
}

TEST(ResourceStorage, TryGetAbsent) {
    ResourceStorage storage;
    EXPECT_EQ(storage.try_get<Health>(), nullptr);
}

TEST(ResourceStorage, Has) {
    ResourceStorage storage;
    EXPECT_FALSE(storage.has<Health>());
    storage.insert(Health{});
    EXPECT_TRUE(storage.has<Health>());
}

TEST(ResourceStorage, Remove) {
    ResourceStorage storage;
    storage.insert(Health{});
    EXPECT_TRUE(storage.has<Health>());
    storage.remove<Health>();
    EXPECT_FALSE(storage.has<Health>());
    EXPECT_EQ(storage.count(), 0u);
}

TEST(ResourceStorage, GetThrowsWhenMissing) {
    ResourceStorage storage;
    EXPECT_THROW(storage.get<Health>(), std::out_of_range);
}

TEST(ResourceStorage, MultipleResourceTypes) {
    ResourceStorage storage;
    storage.insert(Health{100});
    storage.insert(Speed{3.5f});
    EXPECT_EQ(storage.count(), 2u);
    EXPECT_EQ(storage.get<Health>().value, 100);
    EXPECT_FLOAT_EQ(storage.get<Speed>().value, 3.5f);
}

TEST(ResourceStorage, OverwriteExisting) {
    ResourceStorage storage;
    storage.insert(Health{10});
    storage.insert(Health{20});
    EXPECT_EQ(storage.count(), 1u);
    EXPECT_EQ(storage.get<Health>().value, 20);
}

// Test with shared_ptr to simulate polymorphic backend pattern
// (std::any requires CopyConstructible; shared_ptr wraps move-only resources)
struct BackendBase {
    virtual ~BackendBase() = default;
    virtual int id() const = 0;
};

struct ConcreteBackend : BackendBase {
    int id() const override { return 42; }
};

TEST(ResourceStorage, UniquePtr) {
    ResourceStorage storage;
    // Store as shared_ptr<BackendBase> for the polymorphic backend pattern
    std::shared_ptr<BackendBase> backend = std::make_shared<ConcreteBackend>();
    storage.insert(backend);
    auto* ptr = storage.try_get<std::shared_ptr<BackendBase>>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ((*ptr)->id(), 42);
}

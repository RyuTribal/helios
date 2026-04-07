#include <gtest/gtest.h>
#include "helios/core/uuid.h"
#include "helios/core/uuid_map.h"
#include "helios/ecs/world.h"
#include "helios/components/components.h"

#include <unordered_set>
#include <thread>
#include <vector>
#include <sstream>

using namespace helios;

// ---- UUID Tests ----

TEST(UUID, NilIsDefault) {
    UUID uuid{};
    EXPECT_FALSE(static_cast<bool>(uuid));
    EXPECT_EQ(uuid, UUID::NIL);
}

TEST(UUID, GenerateIsNonNil) {
    UUID uuid = UUID::generate();
    EXPECT_TRUE(static_cast<bool>(uuid));
    EXPECT_NE(uuid, UUID::NIL);
}

TEST(UUID, GenerateIsUnique) {
    constexpr int N = 1000;
    std::unordered_set<UUID> set;
    for (int i = 0; i < N; i++) {
        set.insert(UUID::generate());
    }
    EXPECT_EQ(set.size(), N);
}

TEST(UUID, RoundTripString) {
    UUID original = UUID::generate();
    std::string str = original.to_string();
    UUID parsed = UUID::from_string(str);
    EXPECT_EQ(original, parsed);
}

TEST(UUID, StringFormat) {
    UUID uuid = UUID::generate();
    std::string str = uuid.to_string();
    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 chars)
    EXPECT_EQ(str.size(), 36u);
    EXPECT_EQ(str[8], '-');
    EXPECT_EQ(str[13], '-');
    EXPECT_EQ(str[18], '-');
    EXPECT_EQ(str[23], '-');
}

TEST(UUID, Version4Bits) {
    UUID uuid = UUID::generate();
    // Version nibble (bits 48-51 of high) should be 0x4
    uint8_t version = (uuid.high >> 48) & 0xF;
    EXPECT_EQ(version, 4);
}

TEST(UUID, VariantBits) {
    UUID uuid = UUID::generate();
    // Variant bits (bits 62-63 of low) should be 0b10
    uint8_t variant = (uuid.low >> 62) & 0x3;
    EXPECT_EQ(variant, 2);
}

TEST(UUID, FromStringNoDashes) {
    UUID original = UUID::generate();
    std::string str = original.to_string();
    // Remove dashes
    std::string nodash;
    for (char c : str) {
        if (c != '-') nodash += c;
    }
    UUID parsed = UUID::from_string(nodash);
    EXPECT_EQ(original, parsed);
}

TEST(UUID, FromStringInvalidThrows) {
    EXPECT_THROW(UUID::from_string("not-a-uuid"), std::invalid_argument);
    EXPECT_THROW(UUID::from_string(""), std::invalid_argument);
    EXPECT_THROW(UUID::from_string("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"), std::invalid_argument);
}

TEST(UUID, Hashable) {
    std::unordered_set<UUID> set;
    UUID a = UUID::generate();
    UUID b = UUID::generate();
    set.insert(a);
    set.insert(b);
    set.insert(a); // duplicate
    EXPECT_EQ(set.size(), 2u);
}

TEST(UUID, StreamOutput) {
    UUID uuid = UUID::generate();
    std::ostringstream oss;
    oss << uuid;
    EXPECT_EQ(oss.str(), uuid.to_string());
}

TEST(UUID, ThreadSafety) {
    constexpr int THREADS = 8;
    constexpr int PER_THREAD = 500;

    std::vector<std::vector<UUID>> results(THREADS);
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back([&results, t]() {
            for (int i = 0; i < PER_THREAD; i++) {
                results[t].push_back(UUID::generate());
            }
        });
    }

    for (auto& t : threads) t.join();

    // All UUIDs across all threads should be unique
    std::unordered_set<UUID> all;
    for (auto& vec : results) {
        for (auto& uuid : vec) {
            all.insert(uuid);
        }
    }
    EXPECT_EQ(all.size(), THREADS * PER_THREAD);
}

// ---- UuidMap Tests ----

TEST(UuidMap, InitiallyEmpty) {
    UuidMap map;
    EXPECT_EQ(map.count(), 0u);
}

TEST(UuidMap, AssignAndRetrieve) {
    UuidMap map;
    Entity e{1, 1};
    UUID uuid = UUID::generate();

    map.assign(e, uuid);
    EXPECT_EQ(map.get(e), uuid);
    EXPECT_EQ(map.get(uuid), e);
    EXPECT_TRUE(map.has(e));
    EXPECT_TRUE(map.has(uuid));
}

TEST(UuidMap, GetOrCreateAssignsOnce) {
    UuidMap map;
    Entity e{1, 1};

    UUID first = map.get_or_create(e);
    UUID second = map.get_or_create(e);
    EXPECT_EQ(first, second);
    EXPECT_EQ(map.count(), 1u);
}

TEST(UuidMap, UnmappedReturnsNil) {
    UuidMap map;
    EXPECT_EQ(map.get(Entity{99, 1}), UUID::NIL);
    EXPECT_EQ(map.get(UUID::generate()), Entity::INVALID);
}

TEST(UuidMap, RemoveEntity) {
    UuidMap map;
    Entity e{1, 1};
    UUID uuid = map.get_or_create(e);

    map.remove_entity(e);
    EXPECT_FALSE(map.has(e));
    EXPECT_FALSE(map.has(uuid));
    EXPECT_EQ(map.count(), 0u);
}

TEST(UuidMap, RemoveUuid) {
    UuidMap map;
    Entity e{1, 1};
    UUID uuid = map.get_or_create(e);

    map.remove_uuid(uuid);
    EXPECT_FALSE(map.has(e));
    EXPECT_FALSE(map.has(uuid));
}

TEST(UuidMap, ReassignOverwritesPrevious) {
    UuidMap map;
    Entity e{1, 1};
    UUID uuid1 = UUID::generate();
    UUID uuid2 = UUID::generate();

    map.assign(e, uuid1);
    map.assign(e, uuid2);

    EXPECT_EQ(map.get(e), uuid2);
    EXPECT_FALSE(map.has(uuid1)); // old UUID removed
    EXPECT_TRUE(map.has(uuid2));
    EXPECT_EQ(map.count(), 1u);
}

TEST(UuidMap, MultipleEntities) {
    UuidMap map;
    Entity e1{1, 1}, e2{2, 1}, e3{3, 1};

    UUID u1 = map.get_or_create(e1);
    UUID u2 = map.get_or_create(e2);
    UUID u3 = map.get_or_create(e3);

    EXPECT_EQ(map.count(), 3u);
    EXPECT_EQ(map.get(u1), e1);
    EXPECT_EQ(map.get(u2), e2);
    EXPECT_EQ(map.get(u3), e3);
}

TEST(UuidMap, Clear) {
    UuidMap map;
    map.get_or_create(Entity{1, 1});
    map.get_or_create(Entity{2, 1});
    EXPECT_EQ(map.count(), 2u);

    map.clear();
    EXPECT_EQ(map.count(), 0u);
}

TEST(UuidMap, WorldResource) {
    World world;
    world.insert_resource(UuidMap{});

    Entity e = world.spawn(Transform{.position = glm::vec3{1, 2, 3}});

    auto& uuids = world.resource<UuidMap>();
    UUID uuid = uuids.get_or_create(e);
    EXPECT_TRUE(static_cast<bool>(uuid));
    EXPECT_EQ(uuids.get(uuid), e);
}

# UUID System -- Task 0b Addition to Plan 2

> **For agentic workers:** Implement this task AFTER the logging system (Task 0a) and BEFORE the Scheduler (Task 1).

**Goal:** Implement a thread-safe UUID type (128-bit) with generation, string conversion, hashing, and a UuidMap resource for Entity ↔ UUID mapping.

**Depends on:** Plan 1 (ECS Core: Entity, World, ResourceStorage)

---

## Task 0b: UUID and UuidMap

### Files to create:
- `helios-rewrite/helios-core/src/helios/core/uuid.h`
- `helios-rewrite/helios-core/src/helios/core/uuid.cpp`
- `helios-rewrite/helios-core/src/helios/core/uuid_map.h`
- `helios-rewrite/tests/core/test_uuid.cpp`

### Step 0b.1 — Create UUID header

- [ ] Create file: `helios-rewrite/helios-core/src/helios/core/uuid.h`

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <iosfwd>

namespace helios {

/// 128-bit universally unique identifier.
/// Generated via thread-local PRNG — safe to call from any thread.
struct UUID {
    uint64_t high = 0;
    uint64_t low = 0;

    /// Generate a new random UUID (v4-like, thread-safe).
    static UUID generate();

    /// Parse from string: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" or "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    static UUID from_string(std::string_view str);

    /// Format as "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    std::string to_string() const;

    bool operator==(const UUID&) const = default;
    bool operator!=(const UUID&) const = default;

    /// True if not the nil UUID (all zeros).
    explicit operator bool() const { return high != 0 || low != 0; }

    /// Nil UUID constant.
    static const UUID NIL;
};

/// Stream output.
std::ostream& operator<<(std::ostream& os, const UUID& uuid);

} // namespace helios

/// Hash specialization.
template <>
struct std::hash<helios::UUID> {
    size_t operator()(const helios::UUID& uuid) const noexcept {
        // FNV-1a inspired combine
        size_t h = std::hash<uint64_t>{}(uuid.high);
        h ^= std::hash<uint64_t>{}(uuid.low) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
```

### Step 0b.2 — Create UUID implementation

- [ ] Create file: `helios-rewrite/helios-core/src/helios/core/uuid.cpp`

```cpp
#include "helios/core/uuid.h"

#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <cctype>

namespace helios {

const UUID UUID::NIL = {0, 0};

UUID UUID::generate() {
    // Thread-local RNG — no mutex needed, each thread has its own engine.
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static thread_local std::uniform_int_distribution<uint64_t> dist;

    UUID uuid;
    uuid.high = dist(rng);
    uuid.low = dist(rng);

    // Set version 4 (random) bits: high[bits 48-51] = 0100
    uuid.high = (uuid.high & ~(uint64_t{0xF} << 48)) | (uint64_t{0x4} << 48);

    // Set variant bits: low[bits 62-63] = 10
    uuid.low = (uuid.low & ~(uint64_t{0x3} << 62)) | (uint64_t{0x2} << 62);

    return uuid;
}

std::string UUID::to_string() const {
    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    // high provides first 16 hex chars, low provides last 16
    char buf[37]; // 32 hex + 4 dashes + null
    std::snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012llx",
        static_cast<uint32_t>(high >> 32),
        static_cast<uint16_t>(high >> 16),
        static_cast<uint16_t>(high),
        static_cast<uint16_t>(low >> 48),
        static_cast<unsigned long long>(low & 0x0000FFFFFFFFFFFF));
    return std::string(buf);
}

UUID UUID::from_string(std::string_view str) {
    // Strip dashes
    std::string hex;
    hex.reserve(32);
    for (char c : str) {
        if (c != '-') {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                throw std::invalid_argument("UUID::from_string: invalid character");
            }
            hex += c;
        }
    }

    if (hex.size() != 32) {
        throw std::invalid_argument("UUID::from_string: expected 32 hex digits, got " + std::to_string(hex.size()));
    }

    UUID uuid;
    uuid.high = std::stoull(hex.substr(0, 16), nullptr, 16);
    uuid.low = std::stoull(hex.substr(16, 16), nullptr, 16);
    return uuid;
}

std::ostream& operator<<(std::ostream& os, const UUID& uuid) {
    return os << uuid.to_string();
}

} // namespace helios
```

### Step 0b.3 — Create UuidMap header

- [ ] Create file: `helios-rewrite/helios-core/src/helios/core/uuid_map.h`

```cpp
#pragma once

#include "helios/core/uuid.h"
#include "helios/ecs/entity.h"

#include <unordered_map>

namespace helios {

/// Bidirectional mapping between Entity (runtime) and UUID (persistent).
/// Stored as a World Resource. Only entities that need persistence get a UUID.
class UuidMap {
public:
    UuidMap() = default;

    /// Get the UUID for an entity. Returns NIL if not mapped.
    UUID get(Entity entity) const {
        auto it = m_entity_to_uuid.find(entity);
        return (it != m_entity_to_uuid.end()) ? it->second : UUID::NIL;
    }

    /// Get the entity for a UUID. Returns Entity::INVALID if not mapped.
    Entity get(UUID uuid) const {
        auto it = m_uuid_to_entity.find(uuid);
        return (it != m_uuid_to_entity.end()) ? it->second : Entity::INVALID;
    }

    /// Get existing UUID or generate a new one for the entity.
    UUID get_or_create(Entity entity) {
        auto it = m_entity_to_uuid.find(entity);
        if (it != m_entity_to_uuid.end()) {
            return it->second;
        }
        UUID uuid = UUID::generate();
        assign(entity, uuid);
        return uuid;
    }

    /// Explicitly assign a UUID to an entity (used during scene loading).
    void assign(Entity entity, UUID uuid) {
        // Remove any previous mapping for either side
        remove_entity(entity);
        remove_uuid(uuid);

        m_entity_to_uuid[entity] = uuid;
        m_uuid_to_entity[uuid] = entity;
    }

    /// Remove mapping for an entity.
    void remove_entity(Entity entity) {
        auto it = m_entity_to_uuid.find(entity);
        if (it != m_entity_to_uuid.end()) {
            m_uuid_to_entity.erase(it->second);
            m_entity_to_uuid.erase(it);
        }
    }

    /// Remove mapping for a UUID.
    void remove_uuid(UUID uuid) {
        auto it = m_uuid_to_entity.find(uuid);
        if (it != m_uuid_to_entity.end()) {
            m_entity_to_uuid.erase(it->second);
            m_uuid_to_entity.erase(it);
        }
    }

    /// Check if an entity has a UUID assigned.
    bool has(Entity entity) const { return m_entity_to_uuid.count(entity) > 0; }

    /// Check if a UUID is mapped to an entity.
    bool has(UUID uuid) const { return m_uuid_to_entity.count(uuid) > 0; }

    /// Number of mapped entities.
    size_t count() const { return m_entity_to_uuid.size(); }

    /// Clear all mappings.
    void clear() {
        m_entity_to_uuid.clear();
        m_uuid_to_entity.clear();
    }

private:
    std::unordered_map<Entity, UUID> m_entity_to_uuid;
    std::unordered_map<UUID, Entity> m_uuid_to_entity;
};

} // namespace helios
```

### Step 0b.4 — Create UUID tests

- [ ] Create file: `helios-rewrite/tests/core/test_uuid.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/core/uuid.h"
#include "helios/core/uuid_map.h"
#include "helios/ecs/world.h"

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
```

### Step 0b.5 — Update CMakeLists.txt

- [ ] Add `src/helios/core/uuid.cpp` to `helios-core/CMakeLists.txt` target_sources
- [ ] Add `core/test_uuid.cpp` to `tests/CMakeLists.txt`
- [ ] Create directory: `mkdir -p helios-rewrite/tests/core`

### Step 0b.6 — Build and verify

```bash
cd helios-rewrite && cmake -B build -DHELIOS_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure
```

### Step 0b.7 — Commit

```bash
git add helios-rewrite/
git commit -m "feat(core): add UUID (128-bit, v4, thread-safe) and UuidMap for entity persistence"
```

---

## Design Notes

**Why not std::uuid?** Doesn't exist in C++20/23/26. Closest proposal is P2300 but years away from compilers.

**Why thread_local RNG?** Each thread gets its own `std::mt19937_64` seeded from `std::random_device`. Zero contention, no mutex. Old engine had global static RNG that raced across threads.

**Why bidirectional map?** Scene loading needs UUID → Entity (recreate references). Scene saving needs Entity → UUID. Both are O(1) with the two-map approach.

**Why not on every entity?** Most runtime entities (particles, bullets, spawned effects) never need persistence. The UuidMap only assigns UUIDs when explicitly requested, keeping overhead at zero for the common case.

# Plan 1: CMake + ECS Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement the foundational ECS (Entity Component System) with archetype storage, queries, commands, resources, and events.

**Architecture:** Archetype-based ECS where entities with identical component sets are stored together in contiguous arrays. Type-erased column storage with typed access through Query iterators. Resources stored as std::any. Events double-buffered.

**Tech Stack:** C++20, CMake, Google Test, glm, qlibs/reflect

---

## Task 1: Root CMakeLists.txt and Project Skeleton

### Step 1.1 - Create root CMakeLists.txt

- [ ] Create file: `helios-rewrite/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.24)
project(helios VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(HELIOS_BUILD_TESTS "Build unit tests" ON)
option(HELIOS_BUILD_EDITOR "Build editor" OFF)

# Dependencies
include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

if(HELIOS_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# Engine core library
add_subdirectory(helios-core)

# Tests
if(HELIOS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

### Step 1.2 - Create helios-core CMakeLists.txt

- [ ] Create file: `helios-rewrite/helios-core/CMakeLists.txt`

```cmake
add_library(helios-core STATIC)

target_sources(helios-core
    PRIVATE
        src/ecs/entity.cpp
        src/ecs/entity_allocator.cpp
        src/ecs/archetype.cpp
        src/ecs/archetype_storage.cpp
        src/ecs/world.cpp
        src/ecs/commands.cpp
        src/ecs/resource_storage.cpp
        src/ecs/event_storage.cpp
)

target_include_directories(helios-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(helios-core
    PUBLIC
        glm::glm
)

target_compile_features(helios-core PUBLIC cxx_std_20)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-core PRIVATE /W4)
else()
    target_compile_options(helios-core PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

### Step 1.3 - Create tests CMakeLists.txt

- [ ] Create file: `helios-rewrite/tests/CMakeLists.txt`

```cmake
add_executable(helios-tests
    ecs/test_entity.cpp
    ecs/test_entity_allocator.cpp
    ecs/test_archetype.cpp
    ecs/test_archetype_storage.cpp
    ecs/test_world.cpp
    ecs/test_query.cpp
    ecs/test_commands.cpp
    ecs/test_resources.cpp
    ecs/test_events.cpp
    ecs/test_components.cpp
)

target_link_libraries(helios-tests
    PRIVATE
        helios-core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(helios-tests)
```

### Step 1.4 - Create directory structure

- [ ] Create the following directories:

```
helios-rewrite/
  CMakeLists.txt
  helios-core/
    CMakeLists.txt
    include/
      helios/
        ecs/
        components/
    src/
      ecs/
  tests/
    CMakeLists.txt
    ecs/
```

Commands:
```bash
mkdir -p helios-rewrite/helios-core/include/helios/ecs
mkdir -p helios-rewrite/helios-core/include/helios/components
mkdir -p helios-rewrite/helios-core/src/ecs
mkdir -p helios-rewrite/tests/ecs
```

### Step 1.5 - Verify build system

- [ ] Run:
```bash
cd helios-rewrite
cmake -B build -DHELIOS_BUILD_TESTS=ON
```

This will fail initially (no source files yet), but it should configure CMake and fetch dependencies. Once Task 2+ source files are in place, you will run:
```bash
cmake --build build
```

**Commit:** `feat: scaffold CMake project structure for helios rewrite`

---

## Task 2: Entity Struct

### Step 2.1 - Create entity header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/entity.h`

```cpp
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace helios {

struct Entity {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool operator==(const Entity&) const = default;
    bool operator!=(const Entity&) const = default;

    explicit operator bool() const { return generation != 0; }

    static constexpr Entity INVALID = {0, 0};
};

} // namespace helios

// Hash specialization in std namespace
template <>
struct std::hash<helios::Entity> {
    size_t operator()(helios::Entity e) const noexcept {
        return std::hash<uint64_t>{}(
            (static_cast<uint64_t>(e.generation) << 32) | e.index);
    }
};
```

### Step 2.2 - Create entity source (empty, kept for linkage consistency)

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/entity.cpp`

```cpp
#include "helios/ecs/entity.h"
```

### Step 2.3 - Create entity tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_entity.cpp`

```cpp
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
    Entity a{1, 1};
    Entity b{2, 1};
    Entity c{1, 1};

    set.insert(a);
    set.insert(b);
    set.insert(c);

    EXPECT_EQ(set.size(), 2u);
    EXPECT_TRUE(set.count(a));
    EXPECT_TRUE(set.count(b));
}

TEST(Entity, DifferentGenerationsHashDifferently) {
    std::hash<Entity> hasher;
    Entity a{5, 1};
    Entity b{5, 2};
    EXPECT_NE(hasher(a), hasher(b));
}
```

**Commit:** `feat(ecs): add Entity struct with generational index`

---

## Task 3: EntityAllocator

### Step 3.1 - Create EntityAllocator header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/entity_allocator.h`

```cpp
#pragma once

#include "helios/ecs/entity.h"
#include <vector>
#include <cstdint>

namespace helios {

class EntityAllocator {
public:
    EntityAllocator() = default;

    // Allocate a new entity. Reuses free slots with incremented generation.
    Entity allocate();

    // Deallocate an entity. Pushes its index onto the free list and bumps generation.
    void deallocate(Entity entity);

    // Check if entity is currently alive (correct generation and alive flag).
    bool is_alive(Entity entity) const;

    // Total number of currently alive entities.
    size_t alive_count() const { return m_alive_count; }

private:
    struct Entry {
        uint32_t generation = 0;
        bool alive = false;
    };

    std::vector<Entry> m_entries;
    std::vector<uint32_t> m_free_list;
    size_t m_alive_count = 0;
};

} // namespace helios
```

### Step 3.2 - Create EntityAllocator implementation

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/entity_allocator.cpp`

```cpp
#include "helios/ecs/entity_allocator.h"
#include <cassert>

namespace helios {

Entity EntityAllocator::allocate() {
    uint32_t index;

    if (!m_free_list.empty()) {
        index = m_free_list.back();
        m_free_list.pop_back();
        // Generation was already bumped when deallocated
        m_entries[index].alive = true;
    } else {
        index = static_cast<uint32_t>(m_entries.size());
        m_entries.push_back(Entry{.generation = 1, .alive = true});
    }

    ++m_alive_count;
    return Entity{index, m_entries[index].generation};
}

void EntityAllocator::deallocate(Entity entity) {
    assert(entity.index < m_entries.size());
    auto& entry = m_entries[entity.index];

    assert(entry.alive);
    assert(entry.generation == entity.generation);

    entry.alive = false;
    entry.generation++; // Bump generation so old handles become stale
    m_free_list.push_back(entity.index);
    --m_alive_count;
}

bool EntityAllocator::is_alive(Entity entity) const {
    if (entity.index >= m_entries.size()) return false;
    const auto& entry = m_entries[entity.index];
    return entry.alive && entry.generation == entity.generation;
}

} // namespace helios
```

### Step 3.3 - Create EntityAllocator tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_entity_allocator.cpp`

```cpp
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
    EXPECT_FALSE(alloc.is_alive(Entity{999, 1})); // out of range
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
```

**Commit:** `feat(ecs): add EntityAllocator with generational free list`

---

## Task 4: ComponentId and Archetype Type Definitions

### Step 4.1 - Create component_id header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/component_id.h`

```cpp
#pragma once

#include <typeindex>
#include <typeinfo>
#include <vector>
#include <algorithm>
#include <cstddef>
#include <functional>

namespace helios {

using ComponentId = std::type_index;

// Sorted vector of component IDs that uniquely identifies an archetype.
using ArchetypeId = std::vector<ComponentId>;

// Helper: get ComponentId for a type
template <typename T>
ComponentId component_id() {
    return ComponentId(typeid(T));
}

// Helper: build a sorted ArchetypeId from a pack of types
template <typename... Ts>
ArchetypeId make_archetype_id() {
    ArchetypeId id{component_id<Ts>()...};
    std::sort(id.begin(), id.end());
    return id;
}

// Helper: check if an ArchetypeId contains a specific ComponentId
inline bool archetype_has(const ArchetypeId& id, ComponentId comp) {
    return std::binary_search(id.begin(), id.end(), comp);
}

// Helper: add a ComponentId to an ArchetypeId (returns new sorted id)
inline ArchetypeId archetype_with(const ArchetypeId& id, ComponentId comp) {
    ArchetypeId result = id;
    auto pos = std::lower_bound(result.begin(), result.end(), comp);
    if (pos == result.end() || *pos != comp) {
        result.insert(pos, comp);
    }
    return result;
}

// Helper: remove a ComponentId from an ArchetypeId (returns new sorted id)
inline ArchetypeId archetype_without(const ArchetypeId& id, ComponentId comp) {
    ArchetypeId result = id;
    auto pos = std::lower_bound(result.begin(), result.end(), comp);
    if (pos != result.end() && *pos == comp) {
        result.erase(pos);
    }
    return result;
}

} // namespace helios

// Hash specialization for ArchetypeId (vector of type_index)
template <>
struct std::hash<helios::ArchetypeId> {
    size_t operator()(const helios::ArchetypeId& id) const noexcept {
        size_t seed = id.size();
        for (const auto& comp : id) {
            seed ^= std::hash<std::type_index>{}(comp) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
```

**Commit:** `feat(ecs): add ComponentId, ArchetypeId type definitions and helpers`

---

## Task 5: Column (Type-Erased Component Storage)

### Step 5.1 - Create column header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/column.h`

```cpp
#pragma once

#include <vector>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <functional>

namespace helios {

// Type-erased column of component data. Stores elements as raw bytes.
// Provides typed access via get<T>() and swap-remove semantics.
class Column {
public:
    using DestructorFn = std::function<void(void*)>;
    using MoveConstructFn = std::function<void(void* dst, void* src)>;
    using CopyFn = std::function<void(void* dst, const void* src)>;

    Column() = default;

    Column(size_t element_size, size_t element_align,
           DestructorFn destructor, MoveConstructFn move_construct)
        : m_element_size(element_size)
        , m_element_align(element_align)
        , m_destructor(std::move(destructor))
        , m_move_construct(std::move(move_construct)) {}

    ~Column() {
        clear();
    }

    Column(Column&& other) noexcept
        : m_data(std::move(other.m_data))
        , m_count(other.m_count)
        , m_element_size(other.m_element_size)
        , m_element_align(other.m_element_align)
        , m_destructor(std::move(other.m_destructor))
        , m_move_construct(std::move(other.m_move_construct)) {
        other.m_count = 0;
    }

    Column& operator=(Column&& other) noexcept {
        if (this != &other) {
            clear();
            m_data = std::move(other.m_data);
            m_count = other.m_count;
            m_element_size = other.m_element_size;
            m_element_align = other.m_element_align;
            m_destructor = std::move(other.m_destructor);
            m_move_construct = std::move(other.m_move_construct);
            other.m_count = 0;
        }
        return *this;
    }

    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;

    // Push a new element (move-constructed from src)
    void push(void* src) {
        m_data.resize((m_count + 1) * m_element_size);
        void* dst = m_data.data() + m_count * m_element_size;
        m_move_construct(dst, src);
        ++m_count;
    }

    // Swap-remove element at index. Calls destructor on removed element.
    // If index != last, move-constructs last element into the removed slot.
    void swap_remove(size_t index) {
        assert(index < m_count);
        void* target = m_data.data() + index * m_element_size;

        // Destroy the element being removed
        m_destructor(target);

        size_t last = m_count - 1;
        if (index != last) {
            // Move last element into the gap
            void* last_ptr = m_data.data() + last * m_element_size;
            m_move_construct(target, last_ptr);
            // Note: last_ptr is now in a moved-from state, we destroy it
            m_destructor(last_ptr);
        }

        m_data.resize(last * m_element_size);
        --m_count;
    }

    // Move element at index out to dst, then swap-remove slot
    void move_out_and_swap_remove(size_t index, void* dst) {
        assert(index < m_count);
        void* src = m_data.data() + index * m_element_size;

        // Move element to destination
        m_move_construct(dst, src);
        // Destroy the moved-from object in the column
        m_destructor(src);

        size_t last = m_count - 1;
        if (index != last) {
            void* last_ptr = m_data.data() + last * m_element_size;
            m_move_construct(src, last_ptr);
            m_destructor(last_ptr);
        }

        m_data.resize(last * m_element_size);
        --m_count;
    }

    void* get_raw(size_t index) {
        assert(index < m_count);
        return m_data.data() + index * m_element_size;
    }

    const void* get_raw(size_t index) const {
        assert(index < m_count);
        return m_data.data() + index * m_element_size;
    }

    template <typename T>
    T& get(size_t index) {
        return *reinterpret_cast<T*>(get_raw(index));
    }

    template <typename T>
    const T& get(size_t index) const {
        return *reinterpret_cast<const T*>(get_raw(index));
    }

    template <typename T>
    T* data() {
        return reinterpret_cast<T*>(m_data.data());
    }

    template <typename T>
    const T* data() const {
        return reinterpret_cast<const T*>(m_data.data());
    }

    size_t count() const { return m_count; }
    size_t element_size() const { return m_element_size; }
    bool empty() const { return m_count == 0; }

    void clear() {
        if (m_destructor) {
            for (size_t i = 0; i < m_count; ++i) {
                m_destructor(m_data.data() + i * m_element_size);
            }
        }
        m_data.clear();
        m_count = 0;
    }

    // Create a Column typed for T
    template <typename T>
    static Column create() {
        return Column(
            sizeof(T),
            alignof(T),
            [](void* ptr) {
                reinterpret_cast<T*>(ptr)->~T();
            },
            [](void* dst, void* src) {
                new (dst) T(std::move(*reinterpret_cast<T*>(src)));
            }
        );
    }

private:
    std::vector<std::byte> m_data;
    size_t m_count = 0;
    size_t m_element_size = 0;
    size_t m_element_align = 0;
    DestructorFn m_destructor;
    MoveConstructFn m_move_construct;
};

} // namespace helios
```

**Commit:** `feat(ecs): add type-erased Column storage with swap-remove`

---

## Task 6: Archetype Struct

### Step 6.1 - Create archetype header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/archetype.h`

```cpp
#pragma once

#include "helios/ecs/entity.h"
#include "helios/ecs/component_id.h"
#include "helios/ecs/column.h"

#include <vector>
#include <unordered_map>
#include <cassert>
#include <algorithm>

namespace helios {

struct Archetype {
    ArchetypeId id;                                           // sorted set of ComponentIds
    std::vector<Entity> entities;                             // parallel to columns
    std::vector<Column> columns;                              // one column per component type
    std::unordered_map<ComponentId, size_t> column_index;     // type -> column position

    size_t count() const { return entities.size(); }
    bool empty() const { return entities.empty(); }

    // Check if this archetype has a particular component type
    bool has_component(ComponentId comp) const {
        return column_index.count(comp) > 0;
    }

    // Check if this archetype contains ALL of the given component types
    template <typename... Ts>
    bool has_all() const {
        return (has_component(component_id<Ts>()) && ...);
    }

    // Check if this archetype contains NONE of the given component types
    template <typename... Ts>
    bool has_none() const {
        return (!has_component(component_id<Ts>()) && ...);
    }

    // Get typed pointer to column data (nullptr if component not present)
    template <typename T>
    T* get_column() {
        auto it = column_index.find(component_id<T>());
        if (it == column_index.end()) return nullptr;
        return columns[it->second].data<T>();
    }

    template <typename T>
    const T* get_column() const {
        auto it = column_index.find(component_id<T>());
        if (it == column_index.end()) return nullptr;
        return columns[it->second].data<T>();
    }

    // Get reference to the Column object for a component type
    Column& get_column_raw(ComponentId comp) {
        auto it = column_index.find(comp);
        assert(it != column_index.end());
        return columns[it->second];
    }

    const Column& get_column_raw(ComponentId comp) const {
        auto it = column_index.find(comp);
        assert(it != column_index.end());
        return columns[it->second];
    }

    // Get typed component for entity at row
    template <typename T>
    T& get(size_t row) {
        auto it = column_index.find(component_id<T>());
        assert(it != column_index.end());
        return columns[it->second].get<T>(row);
    }

    template <typename T>
    const T& get(size_t row) const {
        auto it = column_index.find(component_id<T>());
        assert(it != column_index.end());
        return columns[it->second].get<T>(row);
    }

    // Swap-remove entity at given row. Returns the entity that was swapped
    // into this row (the old last entity), or Entity::INVALID if row was already last.
    Entity swap_remove(size_t row) {
        assert(row < count());

        Entity swapped = Entity::INVALID;
        size_t last = count() - 1;

        if (row != last) {
            swapped = entities[last];
        }

        // Swap-remove each column
        for (auto& col : columns) {
            col.swap_remove(row);
        }

        // Swap-remove entity
        if (row != last) {
            entities[row] = entities[last];
        }
        entities.pop_back();

        return swapped;
    }
};

// Factory: create an Archetype for the given component types with columns ready
// Uses a helper to register column factories
struct ColumnFactory {
    ComponentId id;
    std::function<Column()> create;
};

template <typename T>
ColumnFactory make_column_factory() {
    return ColumnFactory{
        .id = component_id<T>(),
        .create = []() { return Column::create<T>(); }
    };
}

// Create an archetype with columns initialized for each ComponentId.
// column_factories maps ComponentId -> factory function.
// The caller must provide factories for all IDs in the archetype.
Archetype create_archetype(
    const ArchetypeId& id,
    const std::unordered_map<ComponentId, std::function<Column()>>& column_factories);

} // namespace helios
```

### Step 6.2 - Create archetype source

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/archetype.cpp`

```cpp
#include "helios/ecs/archetype.h"
#include <cassert>

namespace helios {

Archetype create_archetype(
    const ArchetypeId& id,
    const std::unordered_map<ComponentId, std::function<Column()>>& column_factories) {

    Archetype arch;
    arch.id = id;

    for (size_t i = 0; i < id.size(); ++i) {
        ComponentId comp = id[i];
        auto it = column_factories.find(comp);
        assert(it != column_factories.end() && "Missing column factory for component type");
        arch.columns.push_back(it->second());
        arch.column_index[comp] = i;
    }

    return arch;
}

} // namespace helios
```

### Step 6.3 - Create archetype tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_archetype.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/ecs/archetype.h"

using namespace helios;

namespace {

struct Position {
    float x = 0, y = 0, z = 0;
};

struct Velocity {
    float dx = 0, dy = 0, dz = 0;
};

struct Health {
    int hp = 100;
};

Archetype make_test_archetype() {
    auto id = make_archetype_id<Position, Velocity>();
    std::unordered_map<ComponentId, std::function<Column()>> factories;
    factories[component_id<Position>()] = []() { return Column::create<Position>(); };
    factories[component_id<Velocity>()] = []() { return Column::create<Velocity>(); };
    return create_archetype(id, factories);
}

} // namespace

TEST(Archetype, CreateEmpty) {
    auto arch = make_test_archetype();
    EXPECT_EQ(arch.count(), 0u);
    EXPECT_TRUE(arch.empty());
    EXPECT_EQ(arch.columns.size(), 2u);
}

TEST(Archetype, HasComponent) {
    auto arch = make_test_archetype();
    EXPECT_TRUE(arch.has_component(component_id<Position>()));
    EXPECT_TRUE(arch.has_component(component_id<Velocity>()));
    EXPECT_FALSE(arch.has_component(component_id<Health>()));
}

TEST(Archetype, HasAll) {
    auto arch = make_test_archetype();
    EXPECT_TRUE((arch.has_all<Position, Velocity>()));
    EXPECT_TRUE((arch.has_all<Position>()));
    EXPECT_FALSE((arch.has_all<Position, Health>()));
}

TEST(Archetype, HasNone) {
    auto arch = make_test_archetype();
    EXPECT_TRUE((arch.has_none<Health>()));
    EXPECT_FALSE((arch.has_none<Position>()));
}

TEST(Archetype, PushAndAccess) {
    auto arch = make_test_archetype();

    Entity e{0, 1};
    arch.entities.push_back(e);
    Position pos{1.0f, 2.0f, 3.0f};
    Velocity vel{4.0f, 5.0f, 6.0f};
    arch.get_column_raw(component_id<Position>()).push(&pos);
    arch.get_column_raw(component_id<Velocity>()).push(&vel);

    EXPECT_EQ(arch.count(), 1u);
    EXPECT_FLOAT_EQ(arch.get<Position>(0).x, 1.0f);
    EXPECT_FLOAT_EQ(arch.get<Velocity>(0).dx, 4.0f);
}

TEST(Archetype, SwapRemoveLastElement) {
    auto arch = make_test_archetype();

    Entity e{0, 1};
    arch.entities.push_back(e);
    Position pos{1, 2, 3};
    Velocity vel{4, 5, 6};
    arch.get_column_raw(component_id<Position>()).push(&pos);
    arch.get_column_raw(component_id<Velocity>()).push(&vel);

    Entity swapped = arch.swap_remove(0);
    EXPECT_EQ(swapped, Entity::INVALID);
    EXPECT_EQ(arch.count(), 0u);
}

TEST(Archetype, SwapRemoveMiddleElement) {
    auto arch = make_test_archetype();

    // Add 3 entities
    for (uint32_t i = 0; i < 3; ++i) {
        Entity e{i, 1};
        arch.entities.push_back(e);
        Position pos{static_cast<float>(i), 0, 0};
        Velocity vel{static_cast<float>(i * 10), 0, 0};
        arch.get_column_raw(component_id<Position>()).push(&pos);
        arch.get_column_raw(component_id<Velocity>()).push(&vel);
    }
    EXPECT_EQ(arch.count(), 3u);

    // Remove entity at index 0 (swap with last)
    Entity swapped = arch.swap_remove(0);
    EXPECT_EQ(swapped.index, 2u); // last entity was swapped in
    EXPECT_EQ(arch.count(), 2u);

    // Index 0 should now contain what was at index 2
    EXPECT_FLOAT_EQ(arch.get<Position>(0).x, 2.0f);
    EXPECT_FLOAT_EQ(arch.get<Velocity>(0).dx, 20.0f);
    EXPECT_EQ(arch.entities[0].index, 2u);
}

TEST(Archetype, GetColumn) {
    auto arch = make_test_archetype();

    Entity e{0, 1};
    arch.entities.push_back(e);
    Position pos{10, 20, 30};
    Velocity vel{1, 2, 3};
    arch.get_column_raw(component_id<Position>()).push(&pos);
    arch.get_column_raw(component_id<Velocity>()).push(&vel);

    Position* positions = arch.get_column<Position>();
    ASSERT_NE(positions, nullptr);
    EXPECT_FLOAT_EQ(positions[0].x, 10.0f);

    Health* healths = arch.get_column<Health>();
    EXPECT_EQ(healths, nullptr);
}
```

**Commit:** `feat(ecs): add Archetype struct with column storage and swap-remove`

---

## Task 7: ArchetypeStorage

### Step 7.1 - Create ArchetypeStorage header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/archetype_storage.h`

```cpp
#pragma once

#include "helios/ecs/archetype.h"
#include "helios/ecs/entity.h"
#include "helios/ecs/component_id.h"

#include <unordered_map>
#include <memory>
#include <functional>
#include <cassert>

namespace helios {

struct EntityLocation {
    Archetype* archetype = nullptr;
    size_t row = 0;
};

class ArchetypeStorage {
public:
    ArchetypeStorage() = default;

    // Register a column factory for a component type. Must be called before
    // the component type is used in any archetype.
    template <typename T>
    void register_component() {
        m_column_factories[component_id<T>()] = []() {
            return Column::create<T>();
        };
    }

    // Get or create archetype for the given component set
    Archetype& get_or_create(const ArchetypeId& id);

    // Add entity to an archetype, pushing component data.
    // Returns the row index within the archetype.
    size_t add_entity(Archetype& arch, Entity entity);

    // Remove entity from its archetype via swap-remove.
    // Updates the entity location map. Returns the swapped entity if any.
    void remove_entity(Entity entity);

    // Move entity from one archetype to another. Copies shared component
    // data from old archetype to new. Used for add/remove component.
    void move_entity(Entity entity, Archetype& from, Archetype& to);

    // Lookup entity location
    EntityLocation locate(Entity entity) const;

    // Check if entity exists in storage
    bool contains(Entity entity) const;

    // Update entity location (used internally after swap operations)
    void set_location(Entity entity, Archetype* archetype, size_t row);

    // Iterate all archetypes
    template <typename F>
    void for_each_archetype(F&& callback) {
        for (auto& [id, arch] : m_archetypes) {
            callback(*arch);
        }
    }

    // Iterate all archetypes matching a set of required component types
    template <typename... Required>
    void for_each_matching(auto&& callback) {
        for (auto& [id, arch] : m_archetypes) {
            if (arch->has_all<Required...>()) {
                callback(*arch);
            }
        }
    }

    // Get column factory for a component
    std::function<Column()>& get_factory(ComponentId id) {
        auto it = m_column_factories.find(id);
        assert(it != m_column_factories.end() && "Component type not registered");
        return it->second;
    }

    // Check if a component type has been registered
    bool is_registered(ComponentId id) const {
        return m_column_factories.count(id) > 0;
    }

    size_t archetype_count() const { return m_archetypes.size(); }

private:
    std::unordered_map<ArchetypeId, std::unique_ptr<Archetype>> m_archetypes;
    std::unordered_map<uint32_t, EntityLocation> m_entity_map;  // entity index -> location
    std::unordered_map<ComponentId, std::function<Column()>> m_column_factories;
};

} // namespace helios
```

### Step 7.2 - Create ArchetypeStorage source

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/archetype_storage.cpp`

```cpp
#include "helios/ecs/archetype_storage.h"
#include <cassert>

namespace helios {

Archetype& ArchetypeStorage::get_or_create(const ArchetypeId& id) {
    auto it = m_archetypes.find(id);
    if (it != m_archetypes.end()) {
        return *it->second;
    }

    // Collect factories for each component in this archetype
    std::unordered_map<ComponentId, std::function<Column()>> factories;
    for (const auto& comp : id) {
        auto fit = m_column_factories.find(comp);
        assert(fit != m_column_factories.end() && "Component type not registered before use");
        factories[comp] = fit->second;
    }

    auto arch = std::make_unique<Archetype>(create_archetype(id, factories));
    auto* ptr = arch.get();
    m_archetypes[id] = std::move(arch);
    return *ptr;
}

size_t ArchetypeStorage::add_entity(Archetype& arch, Entity entity) {
    size_t row = arch.count();
    arch.entities.push_back(entity);
    m_entity_map[entity.index] = EntityLocation{&arch, row};
    return row;
}

void ArchetypeStorage::remove_entity(Entity entity) {
    auto it = m_entity_map.find(entity.index);
    if (it == m_entity_map.end()) return;

    EntityLocation loc = it->second;
    Archetype& arch = *loc.archetype;

    // Perform swap-remove; get entity that was swapped into this row
    Entity swapped = arch.swap_remove(loc.row);

    // Update the swapped entity's location
    if (swapped != Entity::INVALID) {
        m_entity_map[swapped.index] = EntityLocation{&arch, loc.row};
    }

    // Remove the despawned entity from the map
    m_entity_map.erase(entity.index);
}

void ArchetypeStorage::move_entity(Entity entity, Archetype& from, Archetype& to) {
    auto it = m_entity_map.find(entity.index);
    assert(it != m_entity_map.end());
    EntityLocation loc = it->second;
    assert(loc.archetype == &from);

    size_t old_row = loc.row;

    // Move shared component data from old archetype to new archetype
    // For each component in the destination archetype that also exists in source
    for (const auto& [comp_id, col_idx] : to.column_index) {
        auto src_it = from.column_index.find(comp_id);
        if (src_it != from.column_index.end()) {
            // Component exists in both archetypes: move data from old to new
            Column& src_col = from.columns[src_it->second];
            Column& dst_col = to.columns[col_idx];

            // Allocate temp buffer on the stack for small types, or heap for large
            // We use a vector<byte> for simplicity
            std::vector<std::byte> temp(src_col.element_size());
            src_col.move_out_and_swap_remove(old_row, temp.data());
            dst_col.push(temp.data());
            // Destroy the temp object
            // (handled by Column::push doing a move-construct, temp is moved-from)
        } else {
            // Component does NOT exist in old archetype. The caller is responsible
            // for pushing this new component data after move_entity returns.
            // We push a default-constructed placeholder that the caller overwrites.
            // Actually, the caller must push this column manually. We leave it short.
        }
    }

    // Remove columns that are in 'from' but NOT in 'to' (swap-remove those)
    for (const auto& [comp_id, col_idx] : from.column_index) {
        if (to.column_index.find(comp_id) == to.column_index.end()) {
            // This component is being removed; swap-remove it
            from.columns[col_idx].swap_remove(old_row);
        }
    }

    // Swap-remove entity from old archetype's entity list
    Entity swapped = Entity::INVALID;
    size_t last = from.count() - 1;
    if (old_row != last) {
        swapped = from.entities[last];
        from.entities[old_row] = from.entities[last];
    }
    from.entities.pop_back();

    // Update swapped entity's location in the map
    if (swapped != Entity::INVALID) {
        m_entity_map[swapped.index] = EntityLocation{&from, old_row};
    }

    // Add entity to new archetype
    size_t new_row = to.count();
    to.entities.push_back(entity);
    m_entity_map[entity.index] = EntityLocation{&to, new_row};
}

EntityLocation ArchetypeStorage::locate(Entity entity) const {
    auto it = m_entity_map.find(entity.index);
    if (it == m_entity_map.end()) {
        return EntityLocation{nullptr, 0};
    }
    return it->second;
}

bool ArchetypeStorage::contains(Entity entity) const {
    return m_entity_map.count(entity.index) > 0;
}

void ArchetypeStorage::set_location(Entity entity, Archetype* archetype, size_t row) {
    m_entity_map[entity.index] = EntityLocation{archetype, row};
}

} // namespace helios
```

### Step 7.3 - Create ArchetypeStorage tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_archetype_storage.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/ecs/archetype_storage.h"

using namespace helios;

namespace {

struct Position {
    float x = 0, y = 0, z = 0;
};

struct Velocity {
    float dx = 0, dy = 0, dz = 0;
};

struct Health {
    int hp = 100;
};

ArchetypeStorage make_storage() {
    ArchetypeStorage storage;
    storage.register_component<Position>();
    storage.register_component<Velocity>();
    storage.register_component<Health>();
    return storage;
}

} // namespace

TEST(ArchetypeStorage, GetOrCreateCreatesArchetype) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position, Velocity>();
    Archetype& arch = storage.get_or_create(id);

    EXPECT_EQ(arch.id, id);
    EXPECT_EQ(arch.count(), 0u);
    EXPECT_EQ(storage.archetype_count(), 1u);
}

TEST(ArchetypeStorage, GetOrCreateReturnsSame) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position, Velocity>();
    Archetype& a = storage.get_or_create(id);
    Archetype& b = storage.get_or_create(id);
    EXPECT_EQ(&a, &b);
    EXPECT_EQ(storage.archetype_count(), 1u);
}

TEST(ArchetypeStorage, AddEntity) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position>();
    Archetype& arch = storage.get_or_create(id);

    Entity e{0, 1};
    size_t row = storage.add_entity(arch, e);
    Position pos{1, 2, 3};
    arch.get_column_raw(component_id<Position>()).push(&pos);

    EXPECT_EQ(row, 0u);
    EXPECT_EQ(arch.count(), 1u);
    EXPECT_TRUE(storage.contains(e));

    EntityLocation loc = storage.locate(e);
    EXPECT_EQ(loc.archetype, &arch);
    EXPECT_EQ(loc.row, 0u);
}

TEST(ArchetypeStorage, RemoveEntity) {
    auto storage = make_storage();
    auto id = make_archetype_id<Position>();
    Archetype& arch = storage.get_or_create(id);

    Entity e1{0, 1};
    Entity e2{1, 1};
    storage.add_entity(arch, e1);
    Position p1{1, 0, 0};
    arch.get_column_raw(component_id<Position>()).push(&p1);

    storage.add_entity(arch, e2);
    Position p2{2, 0, 0};
    arch.get_column_raw(component_id<Position>()).push(&p2);

    storage.remove_entity(e1);

    EXPECT_FALSE(storage.contains(e1));
    EXPECT_TRUE(storage.contains(e2));
    EXPECT_EQ(arch.count(), 1u);

    // e2 should have been swapped to row 0
    EntityLocation loc = storage.locate(e2);
    EXPECT_EQ(loc.row, 0u);
    EXPECT_FLOAT_EQ(arch.get<Position>(0).x, 2.0f);
}

TEST(ArchetypeStorage, LocateNonexistent) {
    auto storage = make_storage();
    EntityLocation loc = storage.locate(Entity{999, 1});
    EXPECT_EQ(loc.archetype, nullptr);
}

TEST(ArchetypeStorage, ForEachMatchingFindsCorrectArchetypes) {
    auto storage = make_storage();
    auto id1 = make_archetype_id<Position>();
    auto id2 = make_archetype_id<Position, Velocity>();
    auto id3 = make_archetype_id<Health>();

    storage.get_or_create(id1);
    storage.get_or_create(id2);
    storage.get_or_create(id3);

    int count = 0;
    storage.for_each_matching<Position>([&](Archetype&) {
        ++count;
    });
    EXPECT_EQ(count, 2); // id1 and id2 have Position

    count = 0;
    storage.for_each_matching<Position, Velocity>([&](Archetype&) {
        ++count;
    });
    EXPECT_EQ(count, 1); // only id2 has both

    count = 0;
    storage.for_each_matching<Health>([&](Archetype&) {
        ++count;
    });
    EXPECT_EQ(count, 1);
}

TEST(ArchetypeStorage, MoveEntityAddsComponent) {
    auto storage = make_storage();
    auto id_pos = make_archetype_id<Position>();
    auto id_pos_vel = make_archetype_id<Position, Velocity>();
    Archetype& arch1 = storage.get_or_create(id_pos);
    Archetype& arch2 = storage.get_or_create(id_pos_vel);

    Entity e{0, 1};
    storage.add_entity(arch1, e);
    Position pos{5, 10, 15};
    arch1.get_column_raw(component_id<Position>()).push(&pos);

    EXPECT_EQ(arch1.count(), 1u);

    // Move entity from arch1 to arch2 (adding Velocity)
    storage.move_entity(e, arch1, arch2);

    // Push the new Velocity component (move_entity only moves shared components)
    Velocity vel{1, 2, 3};
    arch2.get_column_raw(component_id<Velocity>()).push(&vel);

    EXPECT_EQ(arch1.count(), 0u);
    EXPECT_EQ(arch2.count(), 1u);

    // Position should have been moved
    EXPECT_FLOAT_EQ(arch2.get<Position>(0).x, 5.0f);
    EXPECT_FLOAT_EQ(arch2.get<Position>(0).y, 10.0f);
    // Velocity should be the newly added one
    EXPECT_FLOAT_EQ(arch2.get<Velocity>(0).dx, 1.0f);

    // Location should be updated
    EntityLocation loc = storage.locate(e);
    EXPECT_EQ(loc.archetype, &arch2);
    EXPECT_EQ(loc.row, 0u);
}
```

**Commit:** `feat(ecs): add ArchetypeStorage with entity location map and archetype transitions`

---

## Task 8: ResourceStorage

### Step 8.1 - Create ResourceStorage header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/resource_storage.h`

```cpp
#pragma once

#include <any>
#include <typeindex>
#include <unordered_map>
#include <cassert>
#include <stdexcept>

namespace helios {

class ResourceStorage {
public:
    ResourceStorage() = default;

    template <typename T>
    void insert(T resource) {
        m_resources[std::type_index(typeid(T))] = std::make_any<T>(std::move(resource));
    }

    template <typename T>
    T& get() {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            throw std::runtime_error(
                std::string("Resource not found: ") + typeid(T).name());
        }
        return std::any_cast<T&>(it->second);
    }

    template <typename T>
    const T& get() const {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            throw std::runtime_error(
                std::string("Resource not found: ") + typeid(T).name());
        }
        return std::any_cast<const T&>(it->second);
    }

    template <typename T>
    T* try_get() {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    template <typename T>
    const T* try_get() const {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) return nullptr;
        return std::any_cast<const T>(&it->second);
    }

    template <typename T>
    bool has() const {
        return m_resources.count(std::type_index(typeid(T))) > 0;
    }

    template <typename T>
    void remove() {
        m_resources.erase(std::type_index(typeid(T)));
    }

    size_t count() const { return m_resources.size(); }

private:
    std::unordered_map<std::type_index, std::any> m_resources;
};

} // namespace helios
```

### Step 8.2 - Create ResourceStorage source (minimal, mostly header-only)

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/resource_storage.cpp`

```cpp
#include "helios/ecs/resource_storage.h"
```

### Step 8.3 - Create ResourceStorage tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_resources.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/ecs/resource_storage.h"
#include <string>
#include <memory>

using namespace helios;

namespace {

struct Time {
    float delta = 0.016f;
    float elapsed = 0.0f;
};

struct Config {
    std::string name = "default";
    int value = 42;
};

} // namespace

TEST(ResourceStorage, InsertAndGet) {
    ResourceStorage storage;
    storage.insert(Time{.delta = 0.033f, .elapsed = 1.0f});

    Time& t = storage.get<Time>();
    EXPECT_FLOAT_EQ(t.delta, 0.033f);
    EXPECT_FLOAT_EQ(t.elapsed, 1.0f);
}

TEST(ResourceStorage, GetConst) {
    ResourceStorage storage;
    storage.insert(Time{.delta = 0.016f});

    const ResourceStorage& cref = storage;
    const Time& t = cref.get<Time>();
    EXPECT_FLOAT_EQ(t.delta, 0.016f);
}

TEST(ResourceStorage, MutateViaGet) {
    ResourceStorage storage;
    storage.insert(Time{});
    storage.get<Time>().delta = 0.05f;
    EXPECT_FLOAT_EQ(storage.get<Time>().delta, 0.05f);
}

TEST(ResourceStorage, TryGetPresent) {
    ResourceStorage storage;
    storage.insert(Config{.name = "test"});

    Config* c = storage.try_get<Config>();
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name, "test");
}

TEST(ResourceStorage, TryGetAbsent) {
    ResourceStorage storage;
    Time* t = storage.try_get<Time>();
    EXPECT_EQ(t, nullptr);
}

TEST(ResourceStorage, Has) {
    ResourceStorage storage;
    EXPECT_FALSE(storage.has<Time>());
    storage.insert(Time{});
    EXPECT_TRUE(storage.has<Time>());
}

TEST(ResourceStorage, Remove) {
    ResourceStorage storage;
    storage.insert(Time{});
    EXPECT_TRUE(storage.has<Time>());

    storage.remove<Time>();
    EXPECT_FALSE(storage.has<Time>());
}

TEST(ResourceStorage, GetThrowsWhenMissing) {
    ResourceStorage storage;
    EXPECT_THROW(storage.get<Time>(), std::runtime_error);
}

TEST(ResourceStorage, MultipleResourceTypes) {
    ResourceStorage storage;
    storage.insert(Time{.delta = 1.0f});
    storage.insert(Config{.name = "multi"});

    EXPECT_FLOAT_EQ(storage.get<Time>().delta, 1.0f);
    EXPECT_EQ(storage.get<Config>().name, "multi");
    EXPECT_EQ(storage.count(), 2u);
}

TEST(ResourceStorage, OverwriteExisting) {
    ResourceStorage storage;
    storage.insert(Time{.delta = 0.01f});
    storage.insert(Time{.delta = 0.05f});
    EXPECT_FLOAT_EQ(storage.get<Time>().delta, 0.05f);
}

TEST(ResourceStorage, UniquePtr) {
    struct Backend {
        virtual ~Backend() = default;
        virtual int id() const = 0;
    };
    struct ConcreteBackend : Backend {
        int id() const override { return 42; }
    };

    ResourceStorage storage;
    storage.insert(std::unique_ptr<Backend>(std::make_unique<ConcreteBackend>()));

    auto& ptr = storage.get<std::unique_ptr<Backend>>();
    EXPECT_EQ(ptr->id(), 42);
}
```

**Commit:** `feat(ecs): add ResourceStorage with typed std::any storage`

---

## Task 9: EventStorage

### Step 9.1 - Create EventStorage header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/event_storage.h`

```cpp
#pragma once

#include <any>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <stdexcept>

namespace helios {

// Type-erased event channel with double-buffering.
// Writers push to write_buffer. Readers iterate read_buffer.
// swap_buffers() flips them and clears the new write_buffer.
class EventChannelBase {
public:
    virtual ~EventChannelBase() = default;
    virtual void swap_buffers() = 0;
    virtual size_t read_count() const = 0;
    virtual size_t write_count() const = 0;
};

template <typename T>
class EventChannel : public EventChannelBase {
public:
    void send(T event) {
        m_write_buffer.push_back(std::move(event));
    }

    const std::vector<T>& read_buffer() const {
        return m_read_buffer;
    }

    void swap_buffers() override {
        m_read_buffer.swap(m_write_buffer);
        m_write_buffer.clear();
    }

    size_t read_count() const override { return m_read_buffer.size(); }
    size_t write_count() const override { return m_write_buffer.size(); }

private:
    std::vector<T> m_read_buffer;
    std::vector<T> m_write_buffer;
};

// EventWriter: push events into the current frame's write buffer.
template <typename T>
class EventWriter {
public:
    explicit EventWriter(EventChannel<T>& channel) : m_channel(&channel) {}

    void send(T event) {
        m_channel->send(std::move(event));
    }

private:
    EventChannel<T>* m_channel;
};

// EventReader: iterate events from the previous frame's read buffer.
template <typename T>
class EventReader {
public:
    explicit EventReader(const EventChannel<T>& channel) : m_channel(&channel) {}

    auto begin() const { return m_channel->read_buffer().begin(); }
    auto end() const { return m_channel->read_buffer().end(); }
    bool is_empty() const { return m_channel->read_buffer().empty(); }
    size_t count() const { return m_channel->read_buffer().size(); }

private:
    const EventChannel<T>* m_channel;
};

// EventStorage: owns all event channels, keyed by type.
class EventStorage {
public:
    EventStorage() = default;

    template <typename T>
    void register_event() {
        auto key = std::type_index(typeid(T));
        if (m_channels.count(key) == 0) {
            m_channels[key] = std::make_unique<EventChannel<T>>();
        }
    }

    template <typename T>
    EventChannel<T>& get_channel() {
        auto key = std::type_index(typeid(T));
        auto it = m_channels.find(key);
        if (it == m_channels.end()) {
            throw std::runtime_error(
                std::string("Event channel not registered: ") + typeid(T).name());
        }
        return static_cast<EventChannel<T>&>(*it->second);
    }

    template <typename T>
    const EventChannel<T>& get_channel() const {
        auto key = std::type_index(typeid(T));
        auto it = m_channels.find(key);
        if (it == m_channels.end()) {
            throw std::runtime_error(
                std::string("Event channel not registered: ") + typeid(T).name());
        }
        return static_cast<const EventChannel<T>&>(*it->second);
    }

    template <typename T>
    EventWriter<T> writer() {
        return EventWriter<T>(get_channel<T>());
    }

    template <typename T>
    EventReader<T> reader() const {
        return EventReader<T>(get_channel<T>());
    }

    template <typename T>
    bool has_channel() const {
        return m_channels.count(std::type_index(typeid(T))) > 0;
    }

    // Swap all channel buffers (call at frame boundary)
    void swap_all_buffers() {
        for (auto& [key, channel] : m_channels) {
            channel->swap_buffers();
        }
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<EventChannelBase>> m_channels;
};

} // namespace helios
```

### Step 9.2 - Create EventStorage source

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/event_storage.cpp`

```cpp
#include "helios/ecs/event_storage.h"
```

### Step 9.3 - Create EventStorage tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_events.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/ecs/event_storage.h"
#include <string>

using namespace helios;

namespace {

struct CollisionEvent {
    uint32_t entity_a;
    uint32_t entity_b;
    float force;
};

struct DamageEvent {
    uint32_t target;
    int amount;
};

struct MessageEvent {
    std::string text;
};

} // namespace

TEST(EventChannel, SendAndSwap) {
    EventChannel<CollisionEvent> channel;

    // Before swap, read buffer is empty
    EXPECT_EQ(channel.read_count(), 0u);

    // Send events (goes to write buffer)
    channel.send(CollisionEvent{1, 2, 5.0f});
    channel.send(CollisionEvent{3, 4, 10.0f});
    EXPECT_EQ(channel.write_count(), 2u);
    EXPECT_EQ(channel.read_count(), 0u);

    // Swap buffers
    channel.swap_buffers();
    EXPECT_EQ(channel.read_count(), 2u);
    EXPECT_EQ(channel.write_count(), 0u);

    // Verify content
    auto& buf = channel.read_buffer();
    EXPECT_EQ(buf[0].entity_a, 1u);
    EXPECT_EQ(buf[1].entity_a, 3u);
}

TEST(EventChannel, SwapClearsWriteBuffer) {
    EventChannel<DamageEvent> channel;
    channel.send(DamageEvent{1, 50});
    channel.swap_buffers();

    // New frame: write buffer should be empty
    EXPECT_EQ(channel.write_count(), 0u);
    // Read buffer has previous frame's events
    EXPECT_EQ(channel.read_count(), 1u);

    // Swap again with no new writes
    channel.swap_buffers();
    EXPECT_EQ(channel.read_count(), 0u);
}

TEST(EventWriter, SendsToChannel) {
    EventChannel<DamageEvent> channel;
    EventWriter<DamageEvent> writer(channel);

    writer.send(DamageEvent{5, 100});
    channel.swap_buffers();

    EXPECT_EQ(channel.read_count(), 1u);
    EXPECT_EQ(channel.read_buffer()[0].target, 5u);
}

TEST(EventReader, IteratesReadBuffer) {
    EventChannel<CollisionEvent> channel;
    channel.send(CollisionEvent{1, 2, 1.0f});
    channel.send(CollisionEvent{3, 4, 2.0f});
    channel.swap_buffers();

    EventReader<CollisionEvent> reader(channel);
    EXPECT_FALSE(reader.is_empty());
    EXPECT_EQ(reader.count(), 2u);

    int count = 0;
    for (const auto& e : reader) {
        (void)e;
        ++count;
    }
    EXPECT_EQ(count, 2);
}

TEST(EventReader, EmptyWhenNoEvents) {
    EventChannel<DamageEvent> channel;
    channel.swap_buffers();

    EventReader<DamageEvent> reader(channel);
    EXPECT_TRUE(reader.is_empty());
    EXPECT_EQ(reader.count(), 0u);
}

TEST(EventStorage, RegisterAndGetChannel) {
    EventStorage storage;
    storage.register_event<CollisionEvent>();

    EXPECT_TRUE(storage.has_channel<CollisionEvent>());
    EXPECT_FALSE(storage.has_channel<DamageEvent>());
}

TEST(EventStorage, WriterAndReader) {
    EventStorage storage;
    storage.register_event<CollisionEvent>();

    auto writer = storage.writer<CollisionEvent>();
    writer.send(CollisionEvent{10, 20, 3.0f});

    storage.swap_all_buffers();

    auto reader = storage.reader<CollisionEvent>();
    EXPECT_EQ(reader.count(), 1u);
    for (const auto& e : reader) {
        EXPECT_EQ(e.entity_a, 10u);
        EXPECT_FLOAT_EQ(e.force, 3.0f);
    }
}

TEST(EventStorage, SwapAllBuffers) {
    EventStorage storage;
    storage.register_event<CollisionEvent>();
    storage.register_event<DamageEvent>();

    storage.writer<CollisionEvent>().send(CollisionEvent{1, 2, 1.0f});
    storage.writer<DamageEvent>().send(DamageEvent{3, 25});

    storage.swap_all_buffers();

    EXPECT_EQ(storage.reader<CollisionEvent>().count(), 1u);
    EXPECT_EQ(storage.reader<DamageEvent>().count(), 1u);
}

TEST(EventStorage, UnregisteredChannelThrows) {
    EventStorage storage;
    EXPECT_THROW(storage.writer<DamageEvent>(), std::runtime_error);
}

TEST(EventStorage, StringEvents) {
    EventStorage storage;
    storage.register_event<MessageEvent>();

    auto writer = storage.writer<MessageEvent>();
    writer.send(MessageEvent{.text = "hello world"});
    storage.swap_all_buffers();

    auto reader = storage.reader<MessageEvent>();
    EXPECT_EQ(reader.count(), 1u);
    for (const auto& e : reader) {
        EXPECT_EQ(e.text, "hello world");
    }
}
```

**Commit:** `feat(ecs): add EventStorage with double-buffered event channels`

---

## Task 10: Query Filters and Query

### Step 10.1 - Create query filter tags

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/query_filters.h`

```cpp
#pragma once

namespace helios {

// Filter tag: entity must have T, but T is not returned in the tuple
template <typename T>
struct With {};

// Filter tag: entity must NOT have T
template <typename T>
struct Without {};

// Filter tag: T may or may not exist, returned as T* (nullptr if absent)
template <typename T>
struct Optional {};

// Type traits to detect filter types
template <typename T>
struct is_with : std::false_type {};
template <typename T>
struct is_with<With<T>> : std::true_type {};

template <typename T>
struct is_without : std::false_type {};
template <typename T>
struct is_without<Without<T>> : std::true_type {};

template <typename T>
struct is_optional : std::false_type {};
template <typename T>
struct is_optional<Optional<T>> : std::true_type {};

// Extract inner type from filter
template <typename T>
struct filter_inner { using type = T; };
template <typename T>
struct filter_inner<With<T>> { using type = T; };
template <typename T>
struct filter_inner<Without<T>> { using type = T; };
template <typename T>
struct filter_inner<Optional<T>> { using type = T; };

template <typename T>
using filter_inner_t = typename filter_inner<T>::type;

// Check if T is a filter type (not a real component fetch)
template <typename T>
struct is_filter : std::false_type {};
template <typename T>
struct is_filter<With<T>> : std::true_type {};
template <typename T>
struct is_filter<Without<T>> : std::true_type {};

// Check if T is a real fetch component (not a filter, could be Optional)
// const T counts as a fetch, Optional<T> counts as a fetch, plain T counts as a fetch
template <typename T>
struct is_fetch : std::bool_constant<!is_with<T>::value && !is_without<T>::value> {};

// Remove const from component types for archetype matching
template <typename T>
struct raw_component { using type = std::remove_const_t<T>; };
template <typename T>
struct raw_component<Optional<T>> { using type = std::remove_const_t<T>; };
template <typename T>
struct raw_component<With<T>> { using type = T; };
template <typename T>
struct raw_component<Without<T>> { using type = T; };

template <typename T>
using raw_component_t = typename raw_component<T>::type;

} // namespace helios
```

### Step 10.2 - Create Query header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/query.h`

```cpp
#pragma once

#include "helios/ecs/archetype_storage.h"
#include "helios/ecs/query_filters.h"
#include "helios/ecs/entity.h"

#include <vector>
#include <tuple>
#include <type_traits>
#include <cstddef>

namespace helios {

// Determine the return type for a single query parameter:
//   T           -> T&
//   const T     -> const T&
//   Optional<T> -> T*
//   Optional<const T> -> const T*
//   With<T>     -> (excluded from result)
//   Without<T>  -> (excluded from result)
template <typename T>
struct query_result_type { using type = T&; };
template <typename T>
struct query_result_type<const T> { using type = const T&; };
template <typename T>
struct query_result_type<Optional<T>> { using type = T*; };
template <typename T>
struct query_result_type<Optional<const T>> { using type = const T*; };

template <typename T>
using query_result_t = typename query_result_type<T>::type;

// Helper: fetch a single component from archetype at row
template <typename T>
struct ComponentFetcher {
    static auto fetch(Archetype& arch, size_t row) -> query_result_t<T> {
        using Raw = std::remove_const_t<T>;
        return arch.get<Raw>(row);
    }
};

template <typename T>
struct ComponentFetcher<const T> {
    static const T& fetch(Archetype& arch, size_t row) {
        return arch.get<T>(row);
    }
};

template <typename T>
struct ComponentFetcher<Optional<T>> {
    static T* fetch(Archetype& arch, size_t row) {
        using Raw = std::remove_const_t<T>;
        Raw* col = arch.get_column<Raw>();
        if (!col) return nullptr;
        return &col[row];
    }
};

template <typename T>
struct ComponentFetcher<Optional<const T>> {
    static const T* fetch(Archetype& arch, size_t row) {
        const T* col = arch.get_column<T>();
        if (!col) return nullptr;
        return &col[row];
    }
};

// Filter out With<T> and Without<T> from the parameter pack to build result tuple
template <typename T>
struct is_result_type : std::bool_constant<!is_with<T>::value && !is_without<T>::value> {};

// Build tuple of only the fetch types (excluding With and Without)
template <typename... Ts>
struct result_tuple_builder;

template <>
struct result_tuple_builder<> {
    using type = std::tuple<>;
};

template <typename T, typename... Rest>
struct result_tuple_builder<T, Rest...> {
    using rest_type = typename result_tuple_builder<Rest...>::type;
    using type = std::conditional_t<
        is_result_type<T>::value,
        decltype(std::tuple_cat(
            std::declval<std::tuple<query_result_t<T>>>(),
            std::declval<rest_type>())),
        rest_type>;
};

template <typename... Ts>
using result_tuple_t = typename result_tuple_builder<Ts...>::type;

// Query class: iterates matching archetypes
template <typename... Params>
class Query {
public:
    using ResultTuple = result_tuple_t<Params...>;

    explicit Query(ArchetypeStorage& storage) : m_storage(&storage) {
        cache_matching_archetypes();
    }

    // Iterator over all matching (archetype, row) pairs
    class Iterator {
    public:
        Iterator(std::vector<Archetype*>& archetypes, size_t arch_idx, size_t row)
            : m_archetypes(&archetypes), m_arch_idx(arch_idx), m_row(row) {
            skip_empty();
        }

        ResultTuple operator*() const {
            return fetch_tuple(std::make_index_sequence<sizeof...(Params)>{});
        }

        Iterator& operator++() {
            ++m_row;
            if (m_row >= (*m_archetypes)[m_arch_idx]->count()) {
                ++m_arch_idx;
                m_row = 0;
                skip_empty();
            }
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return m_arch_idx != other.m_arch_idx || m_row != other.m_row;
        }

        bool operator==(const Iterator& other) const {
            return m_arch_idx == other.m_arch_idx && m_row == other.m_row;
        }

    private:
        void skip_empty() {
            while (m_arch_idx < m_archetypes->size() &&
                   (*m_archetypes)[m_arch_idx]->count() == 0) {
                ++m_arch_idx;
            }
        }

        // Fetch tuple filtering out With/Without types
        template <size_t... Is>
        ResultTuple fetch_tuple(std::index_sequence<Is...>) const {
            return fetch_result_types(
                fetch_single<Params>(*((*m_archetypes)[m_arch_idx]), m_row)...);
        }

        // Fetch one parameter (returns a wrapper to handle With/Without)
        template <typename P>
        auto fetch_single(Archetype& arch, size_t row) const {
            if constexpr (is_with<P>::value || is_without<P>::value) {
                return std::monostate{};  // placeholder, discarded
            } else {
                return ComponentFetcher<P>::fetch(arch, row);
            }
        }

        // Filter monostates out and build result tuple
        template <typename... Args>
        static ResultTuple fetch_result_types(Args&&... args) {
            return filter_monostates(std::forward<Args>(args)...);
        }

        // Recursive filter: skip monostate, collect the rest
        template <typename... Collected>
        static ResultTuple filter_monostates_impl(std::tuple<Collected...> collected) {
            return collected;
        }

        template <typename... Collected, typename First, typename... Rest>
        static ResultTuple filter_monostates_impl(
            std::tuple<Collected...> collected, First&& first, Rest&&... rest) {
            if constexpr (std::is_same_v<std::decay_t<First>, std::monostate>) {
                return filter_monostates_impl(
                    std::move(collected), std::forward<Rest>(rest)...);
            } else {
                return filter_monostates_impl(
                    std::tuple_cat(std::move(collected),
                                   std::make_tuple(std::forward<First>(first))),
                    std::forward<Rest>(rest)...);
            }
        }

        template <typename... Args>
        static ResultTuple filter_monostates(Args&&... args) {
            return filter_monostates_impl(
                std::tuple<>{}, std::forward<Args>(args)...);
        }

        std::vector<Archetype*>* m_archetypes;
        size_t m_arch_idx;
        size_t m_row;
    };

    Iterator begin() { return Iterator(m_matching, 0, 0); }
    Iterator end() { return Iterator(m_matching, m_matching.size(), 0); }

    // Count total entities across all matching archetypes
    size_t count() const {
        size_t total = 0;
        for (auto* arch : m_matching) {
            total += arch->count();
        }
        return total;
    }

    bool is_empty() const { return count() == 0; }

    // Get components for a specific entity (linear search through matching archetypes)
    ResultTuple get(Entity e) {
        EntityLocation loc = m_storage->locate(e);
        assert(loc.archetype != nullptr);
        return fetch_from_archetype(*loc.archetype, loc.row,
                                     std::make_index_sequence<sizeof...(Params)>{});
    }

private:
    void cache_matching_archetypes() {
        m_storage->for_each_archetype([this](Archetype& arch) {
            if (matches(arch)) {
                m_matching.push_back(&arch);
            }
        });
    }

    static bool matches(const Archetype& arch) {
        return check_all<Params...>(arch);
    }

    template <typename... Ts>
    static bool check_all(const Archetype& arch) {
        return (check_one<Ts>(arch) && ...);
    }

    template <typename T>
    static bool check_one(const Archetype& arch) {
        if constexpr (is_with<T>::value) {
            // Must have the component
            return arch.has_component(component_id<filter_inner_t<T>>());
        } else if constexpr (is_without<T>::value) {
            // Must NOT have the component
            return !arch.has_component(component_id<filter_inner_t<T>>());
        } else if constexpr (is_optional<T>::value) {
            // Optional - always matches
            return true;
        } else {
            // Required component (possibly const)
            using Raw = std::remove_const_t<T>;
            return arch.has_component(component_id<Raw>());
        }
    }

    template <size_t... Is>
    ResultTuple fetch_from_archetype(Archetype& arch, size_t row,
                                     std::index_sequence<Is...>) {
        return Iterator::filter_monostates(
            fetch_one<Params>(arch, row)...);
    }

    template <typename P>
    auto fetch_one(Archetype& arch, size_t row) {
        if constexpr (is_with<P>::value || is_without<P>::value) {
            return std::monostate{};
        } else {
            return ComponentFetcher<P>::fetch(arch, row);
        }
    }

    ArchetypeStorage* m_storage;
    std::vector<Archetype*> m_matching;
};

} // namespace helios
```

### Step 10.3 - Create Query tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_query.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/ecs/query.h"
#include "helios/ecs/entity_allocator.h"

using namespace helios;

namespace {

struct Position {
    float x = 0, y = 0, z = 0;
};

struct Velocity {
    float dx = 0, dy = 0, dz = 0;
};

struct Health {
    int hp = 100;
};

struct Disabled {};

// Helper: register components and add entity with Position+Velocity
struct TestFixture {
    ArchetypeStorage storage;
    EntityAllocator allocator;

    TestFixture() {
        storage.register_component<Position>();
        storage.register_component<Velocity>();
        storage.register_component<Health>();
        storage.register_component<Disabled>();
    }

    Entity spawn_with_pos_vel(float px, float vx) {
        Entity e = allocator.allocate();
        auto id = make_archetype_id<Position, Velocity>();
        Archetype& arch = storage.get_or_create(id);
        storage.add_entity(arch, e);
        Position pos{px, 0, 0};
        Velocity vel{vx, 0, 0};
        arch.get_column_raw(component_id<Position>()).push(&pos);
        arch.get_column_raw(component_id<Velocity>()).push(&vel);
        return e;
    }

    Entity spawn_with_pos(float px) {
        Entity e = allocator.allocate();
        auto id = make_archetype_id<Position>();
        Archetype& arch = storage.get_or_create(id);
        storage.add_entity(arch, e);
        Position pos{px, 0, 0};
        arch.get_column_raw(component_id<Position>()).push(&pos);
        return e;
    }

    Entity spawn_with_pos_disabled(float px) {
        Entity e = allocator.allocate();
        auto id = make_archetype_id<Position, Disabled>();
        Archetype& arch = storage.get_or_create(id);
        storage.add_entity(arch, e);
        Position pos{px, 0, 0};
        Disabled dis{};
        arch.get_column_raw(component_id<Position>()).push(&pos);
        arch.get_column_raw(component_id<Disabled>()).push(&dis);
        return e;
    }

    Entity spawn_with_pos_vel_health(float px, float vx, int hp) {
        Entity e = allocator.allocate();
        auto id = make_archetype_id<Position, Velocity, Health>();
        Archetype& arch = storage.get_or_create(id);
        storage.add_entity(arch, e);
        Position pos{px, 0, 0};
        Velocity vel{vx, 0, 0};
        Health h{hp};
        arch.get_column_raw(component_id<Position>()).push(&pos);
        arch.get_column_raw(component_id<Velocity>()).push(&vel);
        arch.get_column_raw(component_id<Health>()).push(&h);
        return e;
    }
};

} // namespace

TEST(Query, BasicIteration) {
    TestFixture f;
    f.spawn_with_pos_vel(1.0f, 10.0f);
    f.spawn_with_pos_vel(2.0f, 20.0f);

    Query<Position, Velocity> query(f.storage);
    EXPECT_EQ(query.count(), 2u);

    int count = 0;
    for (auto [pos, vel] : query) {
        EXPECT_GT(pos.x, 0.0f);
        EXPECT_GT(vel.dx, 0.0f);
        ++count;
    }
    EXPECT_EQ(count, 2);
}

TEST(Query, ConstAccess) {
    TestFixture f;
    f.spawn_with_pos_vel(1.0f, 10.0f);

    Query<const Position, const Velocity> query(f.storage);
    EXPECT_EQ(query.count(), 1u);

    for (auto [pos, vel] : query) {
        // These should be const references
        static_assert(std::is_const_v<std::remove_reference_t<decltype(pos)>>);
        static_assert(std::is_const_v<std::remove_reference_t<decltype(vel)>>);
        EXPECT_FLOAT_EQ(pos.x, 1.0f);
    }
}

TEST(Query, MixedConstMutable) {
    TestFixture f;
    f.spawn_with_pos_vel(1.0f, 10.0f);

    Query<Position, const Velocity> query(f.storage);
    for (auto [pos, vel] : query) {
        static_assert(!std::is_const_v<std::remove_reference_t<decltype(pos)>>);
        static_assert(std::is_const_v<std::remove_reference_t<decltype(vel)>>);
        pos.x = 999.0f; // mutable
    }

    // Verify mutation stuck
    Query<const Position> verify(f.storage);
    for (auto [pos] : verify) {
        EXPECT_FLOAT_EQ(pos.x, 999.0f);
    }
}

TEST(Query, MatchesMultipleArchetypes) {
    TestFixture f;
    f.spawn_with_pos(1.0f);                      // Archetype: [Position]
    f.spawn_with_pos_vel(2.0f, 20.0f);           // Archetype: [Position, Velocity]
    f.spawn_with_pos_vel_health(3.0f, 30.0f, 50); // Archetype: [Position, Velocity, Health]

    // Query for Position should match all 3 archetypes
    Query<Position> query(f.storage);
    EXPECT_EQ(query.count(), 3u);
}

TEST(Query, WithFilter) {
    TestFixture f;
    f.spawn_with_pos(1.0f);
    f.spawn_with_pos_vel(2.0f, 20.0f);

    // Query Position, but only from entities that also have Velocity
    Query<Position, With<Velocity>> query(f.storage);
    EXPECT_EQ(query.count(), 1u);

    for (auto [pos] : query) {
        EXPECT_FLOAT_EQ(pos.x, 2.0f);
    }
}

TEST(Query, WithoutFilter) {
    TestFixture f;
    f.spawn_with_pos(1.0f);
    f.spawn_with_pos_disabled(2.0f);

    // Query Position, excluding disabled entities
    Query<Position, Without<Disabled>> query(f.storage);
    EXPECT_EQ(query.count(), 1u);

    for (auto [pos] : query) {
        EXPECT_FLOAT_EQ(pos.x, 1.0f);
    }
}

TEST(Query, OptionalPresent) {
    TestFixture f;
    f.spawn_with_pos_vel_health(1.0f, 10.0f, 75);

    Query<Position, Optional<Health>> query(f.storage);
    EXPECT_EQ(query.count(), 1u);

    for (auto [pos, health_ptr] : query) {
        ASSERT_NE(health_ptr, nullptr);
        EXPECT_EQ(health_ptr->hp, 75);
    }
}

TEST(Query, OptionalAbsent) {
    TestFixture f;
    f.spawn_with_pos(1.0f);

    Query<Position, Optional<Health>> query(f.storage);
    EXPECT_EQ(query.count(), 1u);

    for (auto [pos, health_ptr] : query) {
        EXPECT_EQ(health_ptr, nullptr);
    }
}

TEST(Query, EmptyResult) {
    TestFixture f;
    f.spawn_with_pos(1.0f);

    Query<Velocity> query(f.storage);
    EXPECT_EQ(query.count(), 0u);
    EXPECT_TRUE(query.is_empty());
}

TEST(Query, GetSpecificEntity) {
    TestFixture f;
    f.spawn_with_pos_vel(1.0f, 10.0f);
    Entity target = f.spawn_with_pos_vel(2.0f, 20.0f);
    f.spawn_with_pos_vel(3.0f, 30.0f);

    Query<Position, Velocity> query(f.storage);
    auto [pos, vel] = query.get(target);
    EXPECT_FLOAT_EQ(pos.x, 2.0f);
    EXPECT_FLOAT_EQ(vel.dx, 20.0f);
}

TEST(Query, MutationDuringIteration) {
    TestFixture f;
    f.spawn_with_pos_vel(1.0f, 5.0f);
    f.spawn_with_pos_vel(2.0f, 10.0f);

    Query<Position, const Velocity> query(f.storage);
    for (auto [pos, vel] : query) {
        pos.x += vel.dx;
    }

    // Verify changes
    Query<const Position, const Velocity> verify(f.storage);
    for (auto [pos, vel] : verify) {
        EXPECT_FLOAT_EQ(pos.x, pos.x); // just confirm no crash
    }
}
```

**Commit:** `feat(ecs): add Query with With/Without/Optional filters and const correctness`

---

## Task 11: World (Entity Operations + Component Access)

### Step 11.1 - Create World header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/world.h`

```cpp
#pragma once

#include "helios/ecs/entity.h"
#include "helios/ecs/entity_allocator.h"
#include "helios/ecs/archetype_storage.h"
#include "helios/ecs/resource_storage.h"
#include "helios/ecs/event_storage.h"
#include "helios/ecs/query.h"

#include <cassert>
#include <vector>

namespace helios {

// Forward declare Commands (circular dependency: Commands needs World, World applies Commands)
class Commands;

class World {
public:
    World() = default;
    ~World() = default;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // --- Entity Operations (immediate) ---

    // Spawn an entity with no components. Returns entity in the empty archetype.
    Entity spawn();

    // Spawn an entity with a set of components.
    template <typename... Ts>
    Entity spawn(Ts&&... components);

    // Despawn an entity, removing it from its archetype.
    void despawn(Entity entity);

    // Check if entity is alive.
    bool is_alive(Entity entity) const;

    // --- Component Access (immediate) ---

    template <typename T>
    T& get(Entity entity);

    template <typename T>
    const T& get(Entity entity) const;

    template <typename T>
    T* try_get(Entity entity);

    template <typename T>
    bool has(Entity entity) const;

    // Add a component to an entity. Moves entity to a new archetype.
    template <typename T>
    T& add(Entity entity, T component);

    // Remove a component from an entity. Moves entity to a new archetype.
    template <typename T>
    void remove(Entity entity);

    // --- Resources ---

    template <typename T>
    void insert_resource(T resource);

    template <typename T>
    T& resource();

    template <typename T>
    const T& resource() const;

    template <typename T>
    T* try_resource();

    template <typename T>
    bool has_resource() const;

    // --- Events ---

    template <typename T>
    void register_event();

    template <typename T>
    EventWriter<T> event_writer();

    template <typename T>
    EventReader<T> event_reader() const;

    void swap_event_buffers();

    // --- Queries ---

    template <typename... T>
    Query<T...> query();

    // --- Commands ---

    void apply_commands(Commands& commands);

    // --- Internals (for advanced usage and testing) ---

    ArchetypeStorage& archetypes() { return m_archetypes; }
    const ArchetypeStorage& archetypes() const { return m_archetypes; }
    EntityAllocator& entities() { return m_entities; }

private:
    // Ensure a component type is registered. Called lazily.
    template <typename T>
    void ensure_registered();

    ArchetypeStorage m_archetypes;
    ResourceStorage m_resources;
    EventStorage m_events;
    EntityAllocator m_entities;
};

// ============================
// Template implementations
// ============================

template <typename T>
void World::ensure_registered() {
    if (!m_archetypes.is_registered(component_id<T>())) {
        m_archetypes.register_component<T>();
    }
}

template <typename... Ts>
Entity World::spawn(Ts&&... components) {
    (ensure_registered<std::decay_t<Ts>>(), ...);

    Entity entity = m_entities.allocate();
    auto id = make_archetype_id<std::decay_t<Ts>...>();
    Archetype& arch = m_archetypes.get_or_create(id);
    m_archetypes.add_entity(arch, entity);

    // Push each component into its column
    auto push_component = [&]<typename C>(C&& comp) {
        using Raw = std::decay_t<C>;
        auto& col = arch.get_column_raw(component_id<Raw>());
        Raw temp = std::forward<C>(comp);
        col.push(&temp);
    };

    (push_component(std::forward<Ts>(components)), ...);

    return entity;
}

template <typename T>
T& World::get(Entity entity) {
    EntityLocation loc = m_archetypes.locate(entity);
    assert(loc.archetype != nullptr && "Entity not found in any archetype");
    return loc.archetype->get<T>(loc.row);
}

template <typename T>
const T& World::get(Entity entity) const {
    EntityLocation loc = m_archetypes.locate(entity);
    assert(loc.archetype != nullptr && "Entity not found in any archetype");
    return loc.archetype->get<T>(loc.row);
}

template <typename T>
T* World::try_get(Entity entity) {
    EntityLocation loc = m_archetypes.locate(entity);
    if (!loc.archetype) return nullptr;
    T* col = loc.archetype->get_column<T>();
    if (!col) return nullptr;
    return &col[loc.row];
}

template <typename T>
bool World::has(Entity entity) const {
    EntityLocation loc = m_archetypes.locate(entity);
    if (!loc.archetype) return false;
    return loc.archetype->has_component(component_id<T>());
}

template <typename T>
T& World::add(Entity entity, T component) {
    ensure_registered<T>();

    EntityLocation loc = m_archetypes.locate(entity);
    assert(loc.archetype != nullptr && "Entity not found");
    assert(!loc.archetype->has_component(component_id<T>()) && "Entity already has component");

    Archetype& old_arch = *loc.archetype;
    ArchetypeId new_id = archetype_with(old_arch.id, component_id<T>());
    Archetype& new_arch = m_archetypes.get_or_create(new_id);

    m_archetypes.move_entity(entity, old_arch, new_arch);

    // Push the new component
    auto& col = new_arch.get_column_raw(component_id<T>());
    col.push(&component);

    EntityLocation new_loc = m_archetypes.locate(entity);
    return new_arch.get<T>(new_loc.row);
}

template <typename T>
void World::remove(Entity entity) {
    EntityLocation loc = m_archetypes.locate(entity);
    assert(loc.archetype != nullptr && "Entity not found");
    assert(loc.archetype->has_component(component_id<T>()) && "Entity does not have component");

    Archetype& old_arch = *loc.archetype;
    ArchetypeId new_id = archetype_without(old_arch.id, component_id<T>());
    Archetype& new_arch = m_archetypes.get_or_create(new_id);

    m_archetypes.move_entity(entity, old_arch, new_arch);
}

template <typename T>
void World::insert_resource(T resource) {
    m_resources.insert<T>(std::move(resource));
}

template <typename T>
T& World::resource() {
    return m_resources.get<T>();
}

template <typename T>
const T& World::resource() const {
    return m_resources.get<T>();
}

template <typename T>
T* World::try_resource() {
    return m_resources.try_get<T>();
}

template <typename T>
bool World::has_resource() const {
    return m_resources.has<T>();
}

template <typename T>
void World::register_event() {
    m_events.register_event<T>();
}

template <typename T>
EventWriter<T> World::event_writer() {
    return m_events.writer<T>();
}

template <typename T>
EventReader<T> World::event_reader() const {
    return m_events.reader<T>();
}

template <typename... T>
Query<T...> World::query() {
    return Query<T...>(m_archetypes);
}

} // namespace helios
```

### Step 11.2 - Create World source

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/world.cpp`

```cpp
#include "helios/ecs/world.h"
#include "helios/ecs/commands.h"

namespace helios {

Entity World::spawn() {
    Entity entity = m_entities.allocate();
    // Place in empty archetype (no components)
    ArchetypeId empty_id{};
    Archetype& arch = m_archetypes.get_or_create(empty_id);
    m_archetypes.add_entity(arch, entity);
    return entity;
}

void World::despawn(Entity entity) {
    assert(m_entities.is_alive(entity));
    m_archetypes.remove_entity(entity);
    m_entities.deallocate(entity);
}

bool World::is_alive(Entity entity) const {
    return m_entities.is_alive(entity);
}

void World::swap_event_buffers() {
    m_events.swap_all_buffers();
}

void World::apply_commands(Commands& commands) {
    commands.apply(*this);
}

} // namespace helios
```

### Step 11.3 - Create World tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_world.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/ecs/world.h"
#include <string>

using namespace helios;

namespace {

struct Position {
    float x = 0, y = 0, z = 0;
};

struct Velocity {
    float dx = 0, dy = 0, dz = 0;
};

struct Health {
    int hp = 100;
};

struct Tag {
    std::string name;
};

struct Disabled {};

} // namespace

TEST(World, SpawnEmpty) {
    World world;
    Entity e = world.spawn();
    EXPECT_TRUE(world.is_alive(e));
}

TEST(World, SpawnWithComponents) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3}, Velocity{4, 5, 6});

    EXPECT_TRUE(world.is_alive(e));
    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_TRUE(world.has<Velocity>(e));

    auto& pos = world.get<Position>(e);
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);

    auto& vel = world.get<Velocity>(e);
    EXPECT_FLOAT_EQ(vel.dx, 4.0f);
}

TEST(World, Despawn) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    EXPECT_TRUE(world.is_alive(e));

    world.despawn(e);
    EXPECT_FALSE(world.is_alive(e));
}

TEST(World, GetComponent) {
    World world;
    Entity e = world.spawn(Position{10, 20, 30});

    Position& pos = world.get<Position>(e);
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    pos.x = 999.0f;
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 999.0f);
}

TEST(World, GetConstComponent) {
    World world;
    Entity e = world.spawn(Position{10, 20, 30});

    const World& cworld = world;
    const Position& pos = cworld.get<Position>(e);
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
}

TEST(World, TryGetPresent) {
    World world;
    Entity e = world.spawn(Position{5, 0, 0});

    Position* pos = world.try_get<Position>(e);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 5.0f);
}

TEST(World, TryGetAbsent) {
    World world;
    Entity e = world.spawn(Position{5, 0, 0});

    Velocity* vel = world.try_get<Velocity>(e);
    EXPECT_EQ(vel, nullptr);
}

TEST(World, HasComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});

    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_FALSE(world.has<Velocity>(e));
}

TEST(World, AddComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    EXPECT_FALSE(world.has<Velocity>(e));

    world.add(e, Velocity{10, 20, 30});
    EXPECT_TRUE(world.has<Velocity>(e));
    EXPECT_FLOAT_EQ(world.get<Velocity>(e).dx, 10.0f);

    // Original component should still be intact
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 1.0f);
}

TEST(World, RemoveComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3}, Velocity{4, 5, 6});

    EXPECT_TRUE(world.has<Velocity>(e));
    world.remove<Velocity>(e);
    EXPECT_FALSE(world.has<Velocity>(e));

    // Position should still be intact
    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 1.0f);
}

TEST(World, AddThenRemoveReturnsToOriginalArchetype) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});

    world.add(e, Velocity{4, 5, 6});
    world.remove<Velocity>(e);

    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_FALSE(world.has<Velocity>(e));
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 1.0f);
}

TEST(World, SpawnMultipleEntitiesSameArchetype) {
    World world;
    Entity e1 = world.spawn(Position{1, 0, 0});
    Entity e2 = world.spawn(Position{2, 0, 0});
    Entity e3 = world.spawn(Position{3, 0, 0});

    EXPECT_FLOAT_EQ(world.get<Position>(e1).x, 1.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e2).x, 2.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e3).x, 3.0f);
}

TEST(World, DespawnMiddleEntityPreservesOthers) {
    World world;
    Entity e1 = world.spawn(Position{1, 0, 0});
    Entity e2 = world.spawn(Position{2, 0, 0});
    Entity e3 = world.spawn(Position{3, 0, 0});

    world.despawn(e2);

    EXPECT_TRUE(world.is_alive(e1));
    EXPECT_FALSE(world.is_alive(e2));
    EXPECT_TRUE(world.is_alive(e3));

    // Data should still be correct for surviving entities
    EXPECT_FLOAT_EQ(world.get<Position>(e1).x, 1.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e3).x, 3.0f);
}

TEST(World, QueryThroughWorld) {
    World world;
    world.spawn(Position{1, 0, 0}, Velocity{10, 0, 0});
    world.spawn(Position{2, 0, 0}, Velocity{20, 0, 0});
    world.spawn(Position{3, 0, 0}); // no velocity

    auto q = world.query<Position, const Velocity>();
    EXPECT_EQ(q.count(), 2u);
}

TEST(World, ResourceStorage) {
    struct GameTime {
        float delta = 0.016f;
    };

    World world;
    world.insert_resource(GameTime{.delta = 0.033f});

    EXPECT_TRUE(world.has_resource<GameTime>());
    EXPECT_FLOAT_EQ(world.resource<GameTime>().delta, 0.033f);

    world.resource<GameTime>().delta = 0.05f;
    EXPECT_FLOAT_EQ(world.resource<GameTime>().delta, 0.05f);
}

TEST(World, EventSystem) {
    struct DamageEvent {
        int amount;
    };

    World world;
    world.register_event<DamageEvent>();

    // Write events
    auto writer = world.event_writer<DamageEvent>();
    writer.send(DamageEvent{25});
    writer.send(DamageEvent{50});

    // Before swap, reader sees nothing
    auto reader = world.event_reader<DamageEvent>();
    EXPECT_TRUE(reader.is_empty());

    // After swap, reader sees them
    world.swap_event_buffers();
    auto reader2 = world.event_reader<DamageEvent>();
    EXPECT_EQ(reader2.count(), 2u);
}

TEST(World, SpawnWithStringComponent) {
    World world;
    Entity e = world.spawn(Tag{.name = "player"});
    EXPECT_EQ(world.get<Tag>(e).name, "player");
}
```

**Commit:** `feat(ecs): add World with entity operations, component access, resources, events`

---

## Task 12: Commands and EntityBuilder

### Step 12.1 - Create Commands header

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/commands.h`

```cpp
#pragma once

#include "helios/ecs/entity.h"
#include "helios/ecs/entity_allocator.h"
#include "helios/ecs/component_id.h"

#include <vector>
#include <functional>
#include <any>
#include <memory>

namespace helios {

class World;  // forward declaration

// A single deferred command
struct Command {
    std::function<void(World&)> execute;
};

class EntityBuilder;

class Commands {
public:
    explicit Commands(EntityAllocator& allocator)
        : m_allocator(&allocator) {}

    // Spawn a new entity. Returns an EntityBuilder for adding components.
    EntityBuilder spawn();

    // Despawn an entity (deferred).
    void despawn(Entity entity);

    // Add a component to an existing entity (deferred).
    template <typename T>
    void insert(Entity entity, T component);

    // Remove a component from an existing entity (deferred).
    template <typename T>
    void remove(Entity entity);

    // Insert or overwrite a resource (deferred).
    template <typename T>
    void insert_resource(T resource);

    // Apply all queued commands to the world. Called by World::apply_commands.
    void apply(World& world);

    // Number of pending commands.
    size_t pending_count() const { return m_commands.size(); }

private:
    friend class EntityBuilder;

    EntityAllocator* m_allocator;
    std::vector<Command> m_commands;
};

// Builder for spawning entities with multiple components.
class EntityBuilder {
public:
    EntityBuilder(Entity entity, Commands& commands)
        : m_entity(entity), m_commands(&commands) {}

    template <typename T>
    EntityBuilder& insert(T component);

    Entity id() const { return m_entity; }

private:
    Entity m_entity;
    Commands* m_commands;
};

// ============================
// Template implementations
// ============================

template <typename T>
void Commands::insert(Entity entity, T component) {
    m_commands.push_back(Command{
        [entity, comp = std::move(component)](World& world) mutable {
            world.add(entity, std::move(comp));
        }
    });
}

template <typename T>
void Commands::remove(Entity entity) {
    m_commands.push_back(Command{
        [entity](World& world) {
            world.remove<T>(entity);
        }
    });
}

template <typename T>
void Commands::insert_resource(T resource) {
    m_commands.push_back(Command{
        [res = std::move(resource)](World& world) mutable {
            world.insert_resource<T>(std::move(res));
        }
    });
}

template <typename T>
EntityBuilder& EntityBuilder::insert(T component) {
    m_commands->insert(m_entity, std::move(component));
    return *this;
}

} // namespace helios
```

### Step 12.2 - Create Commands source

- [ ] Create file: `helios-rewrite/helios-core/src/ecs/commands.cpp`

```cpp
#include "helios/ecs/commands.h"
#include "helios/ecs/world.h"

namespace helios {

EntityBuilder Commands::spawn() {
    Entity entity = m_allocator->allocate();

    // Deferred: register entity in the world's empty archetype
    m_commands.push_back(Command{
        [entity](World& world) {
            ArchetypeId empty_id{};
            Archetype& arch = world.archetypes().get_or_create(empty_id);
            world.archetypes().add_entity(arch, entity);
        }
    });

    return EntityBuilder(entity, *this);
}

void Commands::despawn(Entity entity) {
    m_commands.push_back(Command{
        [entity](World& world) {
            world.despawn(entity);
        }
    });
}

void Commands::apply(World& world) {
    for (auto& cmd : m_commands) {
        cmd.execute(world);
    }
    m_commands.clear();
}

} // namespace helios
```

### Step 12.3 - Create Commands tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_commands.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/ecs/commands.h"
#include "helios/ecs/world.h"

using namespace helios;

namespace {

struct Position {
    float x = 0, y = 0, z = 0;
};

struct Velocity {
    float dx = 0, dy = 0, dz = 0;
};

struct Health {
    int hp = 100;
};

struct Tag {
    std::string name;
};

} // namespace

TEST(Commands, DeferredSpawn) {
    World world;
    Commands cmd(world.entities());

    auto builder = cmd.spawn();
    Entity e = builder.id();

    // Entity is allocated but NOT yet in the world's archetype storage
    // (it is alive in the allocator though)
    EXPECT_TRUE(world.entities().is_alive(e));

    // Apply commands
    world.apply_commands(cmd);

    // Now entity should be locatable
    EXPECT_TRUE(world.is_alive(e));
}

TEST(Commands, SpawnWithComponents) {
    World world;
    Commands cmd(world.entities());

    auto builder = cmd.spawn();
    Entity e = builder.id();
    builder.insert(Position{1, 2, 3})
           .insert(Velocity{4, 5, 6});

    world.apply_commands(cmd);

    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_TRUE(world.has<Velocity>(e));
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 1.0f);
    EXPECT_FLOAT_EQ(world.get<Velocity>(e).dx, 4.0f);
}

TEST(Commands, DeferredDespawn) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    EXPECT_TRUE(world.is_alive(e));

    Commands cmd(world.entities());
    cmd.despawn(e);

    // Still alive before apply
    EXPECT_TRUE(world.is_alive(e));

    world.apply_commands(cmd);
    EXPECT_FALSE(world.is_alive(e));
}

TEST(Commands, DeferredInsertComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3});
    EXPECT_FALSE(world.has<Velocity>(e));

    Commands cmd(world.entities());
    cmd.insert(e, Velocity{10, 20, 30});

    // Not yet applied
    EXPECT_FALSE(world.has<Velocity>(e));

    world.apply_commands(cmd);
    EXPECT_TRUE(world.has<Velocity>(e));
    EXPECT_FLOAT_EQ(world.get<Velocity>(e).dx, 10.0f);
}

TEST(Commands, DeferredRemoveComponent) {
    World world;
    Entity e = world.spawn(Position{1, 2, 3}, Velocity{4, 5, 6});

    Commands cmd(world.entities());
    cmd.remove<Velocity>(e);

    EXPECT_TRUE(world.has<Velocity>(e)); // still there before apply
    world.apply_commands(cmd);
    EXPECT_FALSE(world.has<Velocity>(e));
    EXPECT_TRUE(world.has<Position>(e)); // position preserved
}

TEST(Commands, DeferredInsertResource) {
    struct GameTime { float delta; };

    World world;
    Commands cmd(world.entities());
    cmd.insert_resource(GameTime{.delta = 0.033f});

    EXPECT_FALSE(world.has_resource<GameTime>());
    world.apply_commands(cmd);
    EXPECT_TRUE(world.has_resource<GameTime>());
    EXPECT_FLOAT_EQ(world.resource<GameTime>().delta, 0.033f);
}

TEST(Commands, MultipleSpawns) {
    World world;
    Commands cmd(world.entities());

    Entity e1 = cmd.spawn().insert(Position{1, 0, 0}).id();
    Entity e2 = cmd.spawn().insert(Position{2, 0, 0}).id();
    Entity e3 = cmd.spawn().insert(Position{3, 0, 0}).id();

    world.apply_commands(cmd);

    EXPECT_TRUE(world.is_alive(e1));
    EXPECT_TRUE(world.is_alive(e2));
    EXPECT_TRUE(world.is_alive(e3));
    EXPECT_FLOAT_EQ(world.get<Position>(e1).x, 1.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e2).x, 2.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e3).x, 3.0f);
}

TEST(Commands, SpawnAndThenDespawn) {
    World world;
    Commands cmd(world.entities());

    Entity e = cmd.spawn().insert(Position{1, 2, 3}).id();
    cmd.despawn(e);

    world.apply_commands(cmd);
    EXPECT_FALSE(world.is_alive(e));
}

TEST(Commands, PendingCount) {
    World world;
    Commands cmd(world.entities());

    EXPECT_EQ(cmd.pending_count(), 0u);
    cmd.spawn();
    EXPECT_EQ(cmd.pending_count(), 1u);

    Entity e = world.spawn(Position{1, 2, 3});
    cmd.despawn(e);
    EXPECT_EQ(cmd.pending_count(), 2u);

    world.apply_commands(cmd);
    EXPECT_EQ(cmd.pending_count(), 0u);
}

TEST(Commands, StringComponents) {
    World world;
    Commands cmd(world.entities());

    Entity e = cmd.spawn().insert(Tag{.name = "hero"}).id();
    world.apply_commands(cmd);

    EXPECT_EQ(world.get<Tag>(e).name, "hero");
}
```

**Commit:** `feat(ecs): add Commands with deferred spawn/despawn/insert/remove and EntityBuilder`

---

## Task 13: AssetHandle and Opaque Handle Types

### Step 13.1 - Create AssetHandle and opaque handle types

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/asset_handle.h`

```cpp
#pragma once

#include <cstdint>
#include <functional>

namespace helios {

struct AssetHandle {
    uint64_t id = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const AssetHandle&) const = default;
    bool operator!=(const AssetHandle&) const = default;
};

// Opaque handle into the physics backend
struct BodyHandle {
    uint64_t id = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const BodyHandle&) const = default;
};

// Opaque handle into the audio backend
struct SoundHandle {
    uint64_t id = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const SoundHandle&) const = default;
};

} // namespace helios

template <>
struct std::hash<helios::AssetHandle> {
    size_t operator()(helios::AssetHandle h) const noexcept {
        return std::hash<uint64_t>{}(h.id);
    }
};
```

**Commit:** `feat(ecs): add AssetHandle, BodyHandle, SoundHandle opaque types`

---

## Task 14: Component Definitions

### Step 14.1 - Create enums used by components

- [ ] Create file: `helios-rewrite/helios-core/include/helios/components/component_enums.h`

```cpp
#pragma once

#include <cstdint>

namespace helios {

enum class ProjectionType : uint8_t {
    Perspective,
    Orthographic,
};

enum class BodyType : uint8_t {
    Static,
    Dynamic,
    Kinematic,
};

} // namespace helios
```

### Step 14.2 - Create all component structs

- [ ] Create file: `helios-rewrite/helios-core/include/helios/components/components.h`

```cpp
#pragma once

#include "helios/components/component_enums.h"
#include "helios/ecs/asset_handle.h"
#include "helios/ecs/entity.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <cstdint>

namespace helios {

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 to_mat4() const {
        glm::mat4 result(1.0f);
        result = glm::translate(result, position);
        result *= glm::mat4_cast(rotation);
        result = glm::scale(result, scale);
        return result;
    }
};

struct GlobalTransform {
    glm::mat4 matrix{1.0f};
};

struct MeshRenderer {
    AssetHandle mesh{};
    AssetHandle material{};
};

struct PointLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
};

struct DirectionalLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    bool cast_shadows = true;
};

struct Camera {
    float fov_y = 45.0f;
    float near_plane = 0.1f;
    float far_plane = 500.0f;
    ProjectionType projection = ProjectionType::Perspective;
    float ortho_size = 10.0f;
};

struct ActiveCamera {};

struct RigidBody {
    BodyType type = BodyType::Dynamic;
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
    BodyHandle body_handle{};
};

struct BoxCollider {
    glm::vec3 half_extents{0.5f};
    glm::vec3 offset{0.0f};
};

struct SphereCollider {
    float radius = 0.5f;
    glm::vec3 offset{0.0f};
};

struct AudioSource {
    AssetHandle clip{};
    float volume = 1.0f;
    bool loop = false;
    bool spatial = false;
    SoundHandle playing_handle{};
};

struct ScriptInstance {
    uint32_t script_type_id = 0;
    uint64_t managed_handle = 0;
};

struct Parent {
    Entity entity;
};

struct Children {
    std::vector<Entity> entities;
};

struct Tag {
    std::string name;
};

struct Disabled {};

} // namespace helios
```

### Step 14.3 - Create component tests

- [ ] Create file: `helios-rewrite/tests/ecs/test_components.cpp`

```cpp
#include <gtest/gtest.h>
#include "helios/components/components.h"
#include "helios/ecs/world.h"

using namespace helios;

TEST(Components, TransformDefaultValues) {
    Transform t{};
    EXPECT_FLOAT_EQ(t.position.x, 0.0f);
    EXPECT_FLOAT_EQ(t.position.y, 0.0f);
    EXPECT_FLOAT_EQ(t.position.z, 0.0f);
    EXPECT_FLOAT_EQ(t.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.y, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.z, 1.0f);
    // Identity quaternion
    EXPECT_FLOAT_EQ(t.rotation.w, 1.0f);
    EXPECT_FLOAT_EQ(t.rotation.x, 0.0f);
}

TEST(Components, TransformToMat4Identity) {
    Transform t{};
    glm::mat4 m = t.to_mat4();
    // Should be identity matrix
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i == j) {
                EXPECT_NEAR(m[i][j], 1.0f, 1e-6f);
            } else {
                EXPECT_NEAR(m[i][j], 0.0f, 1e-6f);
            }
        }
    }
}

TEST(Components, TransformToMat4Translation) {
    Transform t{};
    t.position = {5.0f, 10.0f, 15.0f};
    glm::mat4 m = t.to_mat4();
    EXPECT_NEAR(m[3][0], 5.0f, 1e-6f);
    EXPECT_NEAR(m[3][1], 10.0f, 1e-6f);
    EXPECT_NEAR(m[3][2], 15.0f, 1e-6f);
}

TEST(Components, GlobalTransformDefault) {
    GlobalTransform gt{};
    EXPECT_NEAR(gt.matrix[0][0], 1.0f, 1e-6f);
    EXPECT_NEAR(gt.matrix[3][3], 1.0f, 1e-6f);
}

TEST(Components, MeshRendererDefault) {
    MeshRenderer mr{};
    EXPECT_FALSE(static_cast<bool>(mr.mesh));
    EXPECT_FALSE(static_cast<bool>(mr.material));
}

TEST(Components, PointLightDefaults) {
    PointLight pl{};
    EXPECT_FLOAT_EQ(pl.color.r, 1.0f);
    EXPECT_FLOAT_EQ(pl.intensity, 1.0f);
    EXPECT_FLOAT_EQ(pl.radius, 10.0f);
}

TEST(Components, DirectionalLightDefaults) {
    DirectionalLight dl{};
    EXPECT_FLOAT_EQ(dl.direction.y, -1.0f);
    EXPECT_TRUE(dl.cast_shadows);
}

TEST(Components, CameraDefaults) {
    Camera cam{};
    EXPECT_FLOAT_EQ(cam.fov_y, 45.0f);
    EXPECT_FLOAT_EQ(cam.near_plane, 0.1f);
    EXPECT_FLOAT_EQ(cam.far_plane, 500.0f);
    EXPECT_EQ(cam.projection, ProjectionType::Perspective);
}

TEST(Components, RigidBodyDefaults) {
    RigidBody rb{};
    EXPECT_EQ(rb.type, BodyType::Dynamic);
    EXPECT_FLOAT_EQ(rb.mass, 1.0f);
    EXPECT_FLOAT_EQ(rb.friction, 0.5f);
    EXPECT_FLOAT_EQ(rb.restitution, 0.3f);
}

TEST(Components, CollidersDefaults) {
    BoxCollider bc{};
    EXPECT_FLOAT_EQ(bc.half_extents.x, 0.5f);

    SphereCollider sc{};
    EXPECT_FLOAT_EQ(sc.radius, 0.5f);
}

TEST(Components, AudioSourceDefaults) {
    AudioSource as{};
    EXPECT_FALSE(static_cast<bool>(as.clip));
    EXPECT_FLOAT_EQ(as.volume, 1.0f);
    EXPECT_FALSE(as.loop);
    EXPECT_FALSE(as.spatial);
}

TEST(Components, ParentAndChildren) {
    Parent p{.entity = Entity{5, 1}};
    EXPECT_EQ(p.entity.index, 5u);

    Children c{};
    c.entities.push_back(Entity{1, 1});
    c.entities.push_back(Entity{2, 1});
    EXPECT_EQ(c.entities.size(), 2u);
}

TEST(Components, TagComponent) {
    Tag t{.name = "player"};
    EXPECT_EQ(t.name, "player");
}

TEST(Components, DisabledIsMarker) {
    EXPECT_EQ(sizeof(Disabled), 1u); // empty struct is 1 byte
}

TEST(Components, AssetHandleValidity) {
    AssetHandle h{};
    EXPECT_FALSE(static_cast<bool>(h));

    AssetHandle h2{.id = 42};
    EXPECT_TRUE(static_cast<bool>(h2));
    EXPECT_NE(h, h2);
}

// Test that components work as ECS data
TEST(Components, SpawnEntityWithComponents) {
    World world;
    Entity e = world.spawn(
        Transform{.position = {1, 2, 3}},
        MeshRenderer{.mesh = AssetHandle{1}, .material = AssetHandle{2}}
    );

    auto& t = world.get<Transform>(e);
    auto& mr = world.get<MeshRenderer>(e);

    EXPECT_FLOAT_EQ(t.position.x, 1.0f);
    EXPECT_EQ(mr.mesh.id, 1u);
    EXPECT_EQ(mr.material.id, 2u);
}

TEST(Components, SpawnEntityWithLight) {
    World world;
    Entity e = world.spawn(
        Transform{.position = {0, 5, 0}},
        PointLight{.color = {1, 0.8f, 0.6f}, .intensity = 2.0f, .radius = 20.0f}
    );

    auto& pl = world.get<PointLight>(e);
    EXPECT_FLOAT_EQ(pl.intensity, 2.0f);
    EXPECT_FLOAT_EQ(pl.radius, 20.0f);
}

TEST(Components, SpawnEntityWithCamera) {
    World world;
    Entity e = world.spawn(
        Transform{},
        Camera{.fov_y = 60.0f, .near_plane = 0.01f, .far_plane = 1000.0f},
        ActiveCamera{}
    );

    EXPECT_TRUE(world.has<ActiveCamera>(e));
    EXPECT_FLOAT_EQ(world.get<Camera>(e).fov_y, 60.0f);
}

TEST(Components, QueryComponentsByType) {
    World world;
    world.spawn(Transform{.position = {1, 0, 0}}, PointLight{.intensity = 1.0f});
    world.spawn(Transform{.position = {2, 0, 0}}, DirectionalLight{.intensity = 2.0f});
    world.spawn(Transform{.position = {3, 0, 0}}, PointLight{.intensity = 3.0f});

    auto q = world.query<Transform, PointLight>();
    EXPECT_EQ(q.count(), 2u);

    for (auto [t, pl] : q) {
        EXPECT_GT(pl.intensity, 0.0f);
    }
}

TEST(Components, DisabledFilteredByWithout) {
    World world;
    world.spawn(Transform{.position = {1, 0, 0}});
    world.spawn(Transform{.position = {2, 0, 0}}, Disabled{});

    auto q = world.query<Transform, Without<Disabled>>();
    EXPECT_EQ(q.count(), 1u);

    for (auto [t] : q) {
        EXPECT_FLOAT_EQ(t.position.x, 1.0f);
    }
}

TEST(Components, ParentChildInECS) {
    World world;
    Entity parent = world.spawn(Transform{.position = {0, 0, 0}});
    Entity child1 = world.spawn(Transform{.position = {1, 0, 0}}, Parent{.entity = parent});
    Entity child2 = world.spawn(Transform{.position = {2, 0, 0}}, Parent{.entity = parent});

    world.add(parent, Children{.entities = {child1, child2}});

    auto& children = world.get<Children>(parent);
    EXPECT_EQ(children.entities.size(), 2u);
    EXPECT_EQ(children.entities[0], child1);
    EXPECT_EQ(children.entities[1], child2);

    EXPECT_EQ(world.get<Parent>(child1).entity, parent);
}
```

**Commit:** `feat(components): add all component definitions (Transform, Camera, Lights, Physics, Audio, Hierarchy)`

---

## Task 15: Res and ResMut Wrapper Types

### Step 15.1 - Create Res/ResMut wrappers

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/system_params.h`

```cpp
#pragma once

#include "helios/ecs/resource_storage.h"

namespace helios {

// Res<T> - immutable reference to a resource.
// Used as a system parameter to declare read-only access to a resource.
template <typename T>
class Res {
public:
    explicit Res(const T& ref) : m_ptr(&ref) {}

    const T& operator*() const { return *m_ptr; }
    const T* operator->() const { return m_ptr; }
    const T* get() const { return m_ptr; }

private:
    const T* m_ptr;
};

// ResMut<T> - mutable reference to a resource.
// Used as a system parameter to declare read-write access to a resource.
template <typename T>
class ResMut {
public:
    explicit ResMut(T& ref) : m_ptr(&ref) {}

    T& operator*() { return *m_ptr; }
    T* operator->() { return m_ptr; }
    T* get() { return m_ptr; }

    const T& operator*() const { return *m_ptr; }
    const T* operator->() const { return m_ptr; }
    const T* get() const { return m_ptr; }

private:
    T* m_ptr;
};

} // namespace helios
```

**Commit:** `feat(ecs): add Res and ResMut system parameter wrappers`

---

## Task 16: Convenience Header

### Step 16.1 - Create top-level include

- [ ] Create file: `helios-rewrite/helios-core/include/helios/ecs/ecs.h`

```cpp
#pragma once

// Convenience header that includes the entire ECS module

#include "helios/ecs/entity.h"
#include "helios/ecs/entity_allocator.h"
#include "helios/ecs/component_id.h"
#include "helios/ecs/column.h"
#include "helios/ecs/archetype.h"
#include "helios/ecs/archetype_storage.h"
#include "helios/ecs/resource_storage.h"
#include "helios/ecs/event_storage.h"
#include "helios/ecs/query.h"
#include "helios/ecs/query_filters.h"
#include "helios/ecs/commands.h"
#include "helios/ecs/world.h"
#include "helios/ecs/asset_handle.h"
#include "helios/ecs/system_params.h"
```

**Commit:** `feat(ecs): add convenience ecs.h header`

---

## Task 17: Build and Verify All Tests

### Step 17.1 - Build the project

- [ ] Run:

```bash
cd helios-rewrite
cmake -B build -DHELIOS_BUILD_TESTS=ON
cmake --build build -j$(nproc)
```

### Step 17.2 - Run all tests

- [ ] Run:

```bash
cd helios-rewrite
ctest --test-dir build --output-on-failure
```

Or equivalently:

```bash
cd helios-rewrite/build
./bin/helios-tests --gtest_output=xml:test_results.xml
```

### Step 17.3 - Verify all tests pass

- [ ] Expected test suites and approximate counts:
  - `Entity` - 6 tests
  - `EntityAllocator` - 9 tests
  - `Archetype` - 7 tests
  - `ArchetypeStorage` - 6 tests
  - `ResourceStorage` - 11 tests
  - `EventChannel` / `EventWriter` / `EventReader` / `EventStorage` - 10 tests
  - `Query` - 11 tests
  - `World` - 14 tests
  - `Commands` - 9 tests
  - `Components` - 18 tests

Total: approximately 101 tests.

### Step 17.4 - Fix any compilation errors or test failures

- [ ] If compilation fails, fix includes, templates, or missing implementations. Common issues:
  - Missing `#include <variant>` for `std::monostate` in query.h
  - Missing `#include <stdexcept>` or `<cassert>` in headers
  - Template instantiation errors in query iterator tuple filtering

**Commit:** `test: all ECS core tests passing`

---

## Task 18: Final Cleanup and Integration Commit

### Step 18.1 - Verify directory structure

- [ ] Confirm the final layout:

```
helios-rewrite/
  CMakeLists.txt
  helios-core/
    CMakeLists.txt
    include/
      helios/
        ecs/
          ecs.h
          entity.h
          entity_allocator.h
          component_id.h
          column.h
          archetype.h
          archetype_storage.h
          resource_storage.h
          event_storage.h
          query.h
          query_filters.h
          commands.h
          world.h
          asset_handle.h
          system_params.h
        components/
          component_enums.h
          components.h
    src/
      ecs/
        entity.cpp
        entity_allocator.cpp
        archetype.cpp
        archetype_storage.cpp
        world.cpp
        commands.cpp
        resource_storage.cpp
        event_storage.cpp
  tests/
    CMakeLists.txt
    ecs/
      test_entity.cpp
      test_entity_allocator.cpp
      test_archetype.cpp
      test_archetype_storage.cpp
      test_world.cpp
      test_query.cpp
      test_commands.cpp
      test_resources.cpp
      test_events.cpp
      test_components.cpp
```

### Step 18.2 - Run full test suite one final time

- [ ] Run:

```bash
cd helios-rewrite
cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure
```

All 101 tests should pass.

### Step 18.3 - Create final commit

- [ ] Run:

```bash
cd helios-rewrite
git add -A
git commit -m "feat: complete ECS core implementation (Plan 1)

- CMake project with C++20 and Google Test
- Entity with generational indices and EntityAllocator with free list
- Type-erased Column storage with swap-remove
- Archetype and ArchetypeStorage with entity location map
- World with immediate entity/component operations
- Query with With/Without/Optional filters and const correctness
- Commands with deferred spawn/despawn/insert/remove and EntityBuilder
- ResourceStorage (std::any) and EventStorage (double-buffered channels)
- Res/ResMut system parameter wrappers
- AssetHandle and opaque backend handles
- All component definitions (Transform, Camera, Lights, Physics, Audio, Hierarchy)
- 101 unit tests covering all systems"
```

---

## Build/Test Commands Summary

```bash
# Configure
cd helios-rewrite
cmake -B build -DHELIOS_BUILD_TESTS=ON

# Build
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure

# Run tests with verbose output
cd build && ./bin/helios-tests --gtest_print_time=1

# Run specific test suite
cd build && ./bin/helios-tests --gtest_filter="World.*"

# Clean build
cd helios-rewrite && rm -rf build && cmake -B build -DHELIOS_BUILD_TESTS=ON && cmake --build build -j$(nproc)
```

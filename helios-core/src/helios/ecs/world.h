#pragma once

#include "helios/ecs/archetype_storage.h"
#include "helios/ecs/component_id.h"
#include "helios/ecs/entity.h"
#include "helios/ecs/entity_allocator.h"
#include "helios/ecs/event_storage.h"
#include "helios/ecs/query.h"
#include "helios/ecs/resource_storage.h"
#include "helios/components/components.h"

#include <cassert>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace helios {

// Type trait: are all types in a parameter pack distinct?
template <typename...> struct are_all_unique : std::true_type {};
template <typename T, typename... Rest>
struct are_all_unique<T, Rest...>
    : std::bool_constant<!std::disjunction_v<std::is_same<T, Rest>...> && are_all_unique<Rest...>::value> {};
template <typename... Ts>
inline constexpr bool are_all_unique_v = are_all_unique<Ts...>::value;

// Forward declarations to break circular dependencies
class Commands;
class Scheduler;

class World {
public:
    World();
    ~World();

    // -----------------------------------------------------------------
    // Entity spawning
    // -----------------------------------------------------------------

    /// Spawn an entity with no components (placed in the empty archetype).
    Entity spawn();

    /// Spawn an entity with an initial set of components.
    /// All component types must be aggregates (no constructors, no virtual methods).
    template <typename... Ts>
        requires (Component<std::remove_cvref_t<Ts>> && ...)
    Entity spawn(Ts&&... components) {
        static_assert(are_all_unique_v<std::remove_cvref_t<Ts>...>,
            "spawn() requires all component types to be distinct");
        Entity e = m_allocator.allocate();

        // Ensure all component types are registered.
        (ensure_registered<std::remove_cvref_t<Ts>>(), ...);

        ArchetypeId id = make_archetype_id<std::remove_cvref_t<Ts>...>();
        Archetype& arch = m_archetypes.get_or_create(id);
        m_archetypes.add_entity(arch, e);

        // Push each component into its column, stamped with current tick.
        (arch.get_column<std::remove_cvref_t<Ts>>().push(&components, m_current_tick), ...);

        return e;
    }

    /// Spawn an entity and add children to it in one call.
    /// Usage: world.spawn_with_children(Tag{"Parent"}, Transform{}, [&](World& w, Entity parent) {
    ///            w.spawn_child(parent, Tag{"Child"}, Transform{});
    ///        });
    template <typename F, typename... Ts>
        requires (Component<std::remove_cvref_t<Ts>> && ...)
    Entity spawn_with_children(F&& build_children, Ts&&... components) {
        Entity parent = spawn(std::forward<Ts>(components)...);
        build_children(*this, parent);
        return parent;
    }

    /// Spawn an entity as a child of the given parent.
    /// Automatically adds Parent component and updates parent's Children.
    template <typename... Ts>
        requires (Component<std::remove_cvref_t<Ts>> && ...)
    Entity spawn_child(Entity parent, Ts&&... components) {
        Entity child = spawn(std::forward<Ts>(components)...);
        add(child, Parent{.entity = parent});
        if (auto* ch = try_get<Children>(parent)) {
            ch->entities.push_back(child);
        } else {
            add(parent, Children{.entities = {child}});
        }
        return child;
    }

    /// Despawn an entity, removing it from its archetype and deallocating it.
    /// Component destructors run automatically (RAII handles release assets).
    void despawn(Entity entity);

    /// Check whether an entity is still alive.
    bool is_alive(Entity entity) const;

    // -----------------------------------------------------------------
    // Component access
    // -----------------------------------------------------------------

    /// Get a mutable reference to a component. Asserts the entity has it.
    template <typename T>
    T& get(Entity entity) {
        auto loc = m_archetypes.locate(entity);
        assert(loc.has_value() && "World::get: entity not found");
        return loc->archetype->get<T>(loc->row);
    }

    /// Get a const reference to a component. Asserts the entity has it.
    template <typename T>
    const T& get(Entity entity) const {
        auto loc = m_archetypes.locate(entity);
        assert(loc.has_value() && "World::get: entity not found");
        return loc->archetype->template get<T>(loc->row);
    }

    /// Try to get a pointer to a component. Returns nullptr if absent.
    template <typename T>
    T* try_get(Entity entity) {
        auto loc = m_archetypes.locate(entity);
        if (!loc.has_value()) return nullptr;
        if (!loc->archetype->has_component(component_id<T>())) return nullptr;
        return &loc->archetype->get<T>(loc->row);
    }

    /// Try to get a const pointer to a component. Returns nullptr if absent.
    template <typename T>
    const T* try_get(Entity entity) const {
        auto loc = m_archetypes.locate(entity);
        if (!loc.has_value()) return nullptr;
        if (!loc->archetype->has_component(component_id<T>())) return nullptr;
        return &loc->archetype->template get<T>(loc->row);
    }

    /// Check whether an entity has a given component.
    template <typename T>
    bool has(Entity entity) const {
        auto loc = m_archetypes.locate(entity);
        if (!loc.has_value()) return false;
        return loc->archetype->has_component(component_id<T>());
    }

    // -----------------------------------------------------------------
    // Add / remove components
    // -----------------------------------------------------------------

    /// Add a component to an existing entity, moving it to a new archetype.
    template <Component T>
    void add(Entity entity, T component) {
        ensure_registered<T>();

        auto loc = m_archetypes.locate(entity);
        assert(loc.has_value() && "World::add: entity not found");
        if (!loc.has_value()) return;

        Archetype& from = *loc->archetype;

        // Already has this component -- overwrite in place and stamp tick.
        if (from.has_component(component_id<T>())) {
            from.get<T>(loc->row) = std::move(component);
            from.get_column<T>().stamp(loc->row, m_current_tick);
            return;
        }

        ArchetypeId new_id = archetype_with(from.id, component_id<T>());
        Archetype& to = m_archetypes.get_or_create(new_id);

        m_archetypes.move_entity(entity, from, to);

        // Push the new component into the destination column, stamped with current tick.
        to.get_column<T>().push(&component, m_current_tick);
    }

    /// Remove a component from an existing entity, moving it to a new archetype.
    template <typename T>
    void remove(Entity entity) {
        auto loc = m_archetypes.locate(entity);
        assert(loc.has_value() && "World::remove: entity not found");
        if (!loc.has_value()) return;

        Archetype& from = *loc->archetype;

        // Does not have this component -- nothing to do.
        if (!from.has_component(component_id<T>())) {
            return;
        }

        ArchetypeId new_id = archetype_without(from.id, component_id<T>());
        Archetype& to = m_archetypes.get_or_create(new_id);

        m_archetypes.move_entity(entity, from, to);
    }

    // -----------------------------------------------------------------
    // Resources
    // -----------------------------------------------------------------

    template <typename T>
    void insert_resource(T resource) {
        m_resources.insert<T>(std::move(resource));
    }

    template <typename T>
    T& resource() {
        return m_resources.get<T>();
    }

    template <typename T>
    const T& resource() const {
        return m_resources.get<T>();
    }

    template <typename T>
    T* try_resource() {
        return m_resources.try_get<T>();
    }

    template <typename T>
    const T* try_resource() const {
        return m_resources.try_get<T>();
    }

    template <typename T>
    bool has_resource() const {
        return m_resources.has<T>();
    }

    // -----------------------------------------------------------------
    // Events
    // -----------------------------------------------------------------

    template <typename T>
    void register_event() {
        m_events.register_event<T>();
    }

    template <typename T>
    bool has_event() const {
        return m_events.has_channel<T>();
    }

    template <typename T>
    EventWriter<T> event_writer() {
        return m_events.writer<T>();
    }

    template <typename T>
    EventReader<T> event_reader() {
        return m_events.reader<T>();
    }

    void swap_event_buffers();

    // -----------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------

    template <typename... Params>
    Query<Params...> query(uint32_t last_run_tick = 0, uint32_t current_tick = 0) {
        return Query<Params...>(m_archetypes, last_run_tick,
                                current_tick ? current_tick : m_current_tick);
    }

    // -----------------------------------------------------------------
    // Commands
    // -----------------------------------------------------------------

    void apply_commands(Commands& commands);

    /// Return a reference to the world-owned pending commands buffer.
    /// Systems that take Commands as a parameter write into this buffer.
    Commands& pending_commands();

    /// Apply all pending deferred commands and clear the buffer.
    void apply_and_clear_pending_commands();

    // -----------------------------------------------------------------
    // Change detection tick
    // -----------------------------------------------------------------

    /// Return the current world tick (monotonically increasing).
    uint32_t current_tick() const { return m_current_tick; }

    // -----------------------------------------------------------------
    // Internal access
    // -----------------------------------------------------------------

    ArchetypeStorage& archetypes() { return m_archetypes; }
    const ArchetypeStorage& archetypes() const { return m_archetypes; }

    EntityAllocator& entities() { return m_allocator; }
    const EntityAllocator& entities() const { return m_allocator; }

private:
    friend class Scheduler;
    friend struct WorldTestAccess;
    friend void propagate_transforms(World& world);

    /// Advance the world tick by one. Only the Scheduler and
    /// propagate_transforms should call this so that every system/pass
    /// sees a unique tick. Private + friend to prevent unsynchronized
    /// external mutation from arbitrary systems.
    void advance_tick() { ++m_current_tick; }

    /// Lazily register a component type with the archetype storage.
    template <typename T>
    void ensure_registered() {
        auto id = component_id<T>();
        if (m_registered_components.count(id) == 0) {
            m_archetypes.register_component<T>();
            m_registered_components.insert(id);
        }
    }

    EntityAllocator m_allocator;
    ArchetypeStorage m_archetypes;
    ResourceStorage m_resources;
    EventStorage m_events;
    std::unordered_set<ComponentId> m_registered_components;
    uint32_t m_current_tick = 1;  // 0 = "never changed"

    // Eagerly constructed in the World constructor so that parallel systems
    // can call pending_commands() without a race on lazy initialization.
    // The scheduler applies and clears this buffer after each stage.
    std::unique_ptr<Commands> m_pending_commands;
};

/// Test-only helper that exposes advance_tick() for unit tests.
/// Production systems must never use this; use the Scheduler instead.
struct WorldTestAccess {
    static void advance_tick(World& world) { world.advance_tick(); }
};

} // namespace helios

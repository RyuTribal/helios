#pragma once

#include "helios/ecs/entity.h"
#include "helios/ecs/entity_allocator.h"
#include "helios/ecs/component_id.h"

#include <functional>
#include <vector>

namespace helios {

// Forward declaration to break circular dependency.
// World is fully defined in world.h; template implementations below include it.
class World;

struct Command {
    std::function<void(World&)> execute;
};

class Commands;

/// Builder returned by Commands::spawn(). Allows chaining .insert() calls
/// to queue deferred component additions for a pre-allocated entity.
class EntityBuilder {
public:
    EntityBuilder(Entity entity, Commands& commands)
        : m_entity(entity), m_commands(&commands) {}

    /// Queue a deferred insert of the given component (must be aggregate).
    template <Component T>
    EntityBuilder& insert(T component);

    /// Return the pre-allocated entity id.
    Entity id() const { return m_entity; }

private:
    Entity m_entity;
    Commands* m_commands;
};

class Commands {
public:
    explicit Commands(EntityAllocator& allocator)
        : m_allocator(&allocator) {}

    // Non-copyable: systems must take Commands& (reference) so that deferred
    // operations accumulate in the world-owned buffer, not a temporary copy.
    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;
    Commands(Commands&&) = default;
    Commands& operator=(Commands&&) = default;

    /// Allocate an entity immediately, queue deferred spawn into empty archetype.
    /// Returns an EntityBuilder for chaining .insert() calls.
    EntityBuilder spawn();

    /// Queue a deferred despawn.
    void despawn(Entity entity);

    /// Queue a deferred component addition (must be aggregate).
    template <Component T>
    void insert(Entity entity, T component);

    /// Queue a deferred component removal.
    template <typename T>
    void remove(Entity entity);

    /// Queue a deferred resource insertion.
    template <typename T>
    void insert_resource(T resource);

    /// Execute all queued commands against the world, then clear.
    void apply(World& world);

    /// Number of pending commands.
    size_t pending_count() const { return m_commands.size(); }

private:
    EntityAllocator* m_allocator;
    std::vector<Command> m_commands;

    // Grant access so template implementations can push to m_commands.
    template <typename T>
    friend class detail_commands_access;

    // Template implementations need to push commands.
    void push_command(Command cmd) { m_commands.push_back(std::move(cmd)); }
};

} // namespace helios

// -------------------------------------------------------------------------
// Template implementations -- require the full World definition.
// -------------------------------------------------------------------------
#include "helios/ecs/world.h"

namespace helios {

template <Component T>
void Commands::insert(Entity entity, T component) {
    push_command(Command{
        [entity, comp = std::move(component)](World& world) mutable {
            world.add<T>(entity, std::move(comp));
        }
    });
}

template <typename T>
void Commands::remove(Entity entity) {
    push_command(Command{
        [entity](World& world) {
            world.remove<T>(entity);
        }
    });
}

template <typename T>
void Commands::insert_resource(T resource) {
    push_command(Command{
        [res = std::move(resource)](World& world) mutable {
            world.insert_resource<T>(std::move(res));
        }
    });
}

template <Component T>
EntityBuilder& EntityBuilder::insert(T component) {
    m_commands->insert<T>(m_entity, std::move(component));
    return *this;
}

} // namespace helios

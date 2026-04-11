#pragma once
#include "helios/ecs/archetype.h"
#include "helios/ecs/component_id.h"
#include "helios/ecs/column.h"
#include "helios/ecs/entity.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <cassert>

namespace helios {

struct EntityLocation {
    Archetype* archetype = nullptr;
    size_t row = 0;
};

class ArchetypeStorage {
public:
    ArchetypeStorage() = default;

    /// Register a component type so its column factory is known.
    template <typename T>
    void register_component() {
        m_column_factories[component_id<T>()] = [] { return Column::create<T>(); };
    }

    /// Find or create an archetype for the given id.
    Archetype& get_or_create(const ArchetypeId& id);

    /// Add an entity to an archetype (no component data -- caller pushes columns).
    /// Returns the row index.
    size_t add_entity(Archetype& archetype, Entity entity);

    /// Swap-remove an entity from its current archetype. Updates location map.
    void remove_entity(Entity entity);

    /// Move an entity from one archetype to another.
    /// Shared component data is moved; columns only in the destination are
    /// left short (caller pushes the new component afterward).
    void move_entity(Entity entity, Archetype& from, Archetype& to);

    /// Look up where an entity lives. Returns nullopt if not tracked.
    std::optional<EntityLocation> locate(Entity entity) const;

    /// Check whether an entity is tracked.
    bool contains(Entity entity) const;

    /// Explicitly set (or overwrite) the location of an entity.
    void set_location(Entity entity, Archetype* archetype, size_t row);

    /// Iterate over every archetype.
    void for_each_archetype(const std::function<void(Archetype&)>& callback);

    /// Iterate over every archetype (const).
    void for_each_archetype(const std::function<void(const Archetype&)>& callback) const;

    /// Iterate over archetypes that contain all of the Required component types.
    template <typename... Required>
    void for_each_matching(const std::function<void(Archetype&)>& callback) {
        for (auto& [id, archetype] : m_archetypes) {
            if (archetype->has_all<Required...>()) {
                callback(*archetype);
            }
        }
    }

private:
    /// Column factories keyed by ComponentId.
    std::unordered_map<ComponentId, std::function<Column()>> m_column_factories;

    /// All archetypes, keyed by their sorted ArchetypeId.
    std::unordered_map<ArchetypeId, std::unique_ptr<Archetype>> m_archetypes;

    /// Entity -> location mapping.
    std::unordered_map<Entity, EntityLocation> m_entity_locations;
};

} // namespace helios

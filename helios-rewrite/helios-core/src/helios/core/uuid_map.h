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

#include "helios/ecs/world.h"
#include "helios/ecs/commands.h"
#include "helios/core/engine_log_channels.h"

namespace helios {

World::World() = default;
World::~World() = default;

Entity World::spawn() {
    Entity e = m_allocator.allocate();
    ArchetypeId empty_id;
    Archetype& arch = m_archetypes.get_or_create(empty_id);
    m_archetypes.add_entity(arch, e);
    HELIOS_LOG(ECS, Trace, "Spawned entity {{index={}, gen={}}}", e.index, e.generation);
    return e;
}

void World::despawn(Entity entity) {
    if (!m_allocator.is_alive(entity)) return;
    HELIOS_LOG(ECS, Trace, "Despawning entity {{index={}, gen={}}}", entity.index, entity.generation);

    m_archetypes.remove_entity(entity);
    m_allocator.deallocate(entity);
}

bool World::is_alive(Entity entity) const {
    return m_allocator.is_alive(entity);
}

void World::swap_event_buffers() {
    m_events.swap_all_buffers();
}

void World::apply_commands(Commands& commands) {
    commands.apply(*this);
}

Commands& World::pending_commands() {
    if (!m_pending_commands) {
        m_pending_commands = std::make_unique<Commands>(m_allocator);
    }
    return *m_pending_commands;
}

void World::apply_and_clear_pending_commands() {
    if (m_pending_commands && m_pending_commands->pending_count() > 0) {
        m_pending_commands->apply(*this);
    }
}

} // namespace helios

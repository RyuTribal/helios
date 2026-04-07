#include "helios/ecs/world.h"
#include "helios/ecs/commands.h"

namespace helios {

Entity World::spawn() {
    Entity e = m_allocator.allocate();
    ArchetypeId empty_id;
    Archetype& arch = m_archetypes.get_or_create(empty_id);
    m_archetypes.add_entity(arch, e);
    return e;
}

void World::despawn(Entity entity) {
    if (!m_allocator.is_alive(entity)) return;
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

} // namespace helios

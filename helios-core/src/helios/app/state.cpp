#include "helios/app/state.h"
#include "helios/ecs/world.h"

namespace helios {

Entity StateBase::spawn_tracked(World& world) {
    Entity e = world.spawn();
    m_tracked_entities.push_back(e);
    return e;
}

void StateBase::despawn_tracked(World& world) {
    for (Entity e : m_tracked_entities) {
        if (world.is_alive(e)) {
            world.despawn(e);
        }
    }
    m_tracked_entities.clear();
}

} // namespace helios

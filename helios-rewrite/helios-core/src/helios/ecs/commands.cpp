#include "helios/ecs/commands.h"
#include "helios/ecs/world.h"
#include "helios/core/engine_log_channels.h"

namespace helios {

EntityBuilder Commands::spawn() {
    Entity e = m_allocator->allocate();
    Entity captured = e;
    m_commands.push_back(Command{
        [captured](World& world) {
            // Place the entity into the empty archetype so it is tracked.
            ArchetypeId empty_id;
            Archetype& arch = world.archetypes().get_or_create(empty_id);
            world.archetypes().add_entity(arch, captured);
        }
    });
    return EntityBuilder(e, *this);
}

void Commands::despawn(Entity entity) {
    m_commands.push_back(Command{
        [entity](World& world) {
            world.despawn(entity);
        }
    });
}

void Commands::apply(World& world) {
    HELIOS_LOG(ECS, Trace, "Applying {} deferred command(s)", m_commands.size());
    for (auto& cmd : m_commands) {
        cmd.execute(world);
    }
    m_commands.clear();
}

} // namespace helios

#pragma once

#include <cstdint>
#include <unordered_map>

namespace helios {
class World;

/// World resource: tracks which script entities were alive last frame.
/// Used by script_destroy_system to detect despawns and call OnDestroy.
struct ScriptAliveTracker {
    std::unordered_map<uint64_t, uint64_t> prev_alive; // entity_raw -> managed_handle
};

/// ECS system: executes OnUpdate for all scripted entities, grouped by type.
void script_execution_system(World& world);

/// ECS system: creates managed instances for new ScriptInstance entities.
void script_create_system(World& world);

/// ECS system: destroys managed instances for despawned entities.
void script_destroy_system(World& world);

/// ECS system: reads ContactEvent and dispatches OnCollisionEnter to scripts.
void script_collision_dispatch_system(World& world);

} // namespace helios

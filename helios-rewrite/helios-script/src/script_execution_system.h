// helios-script/src/script_execution_system.h
#pragma once

namespace helios {
class World;

/// ECS system: executes OnUpdate for all scripted entities, grouped by type.
/// Registered in Schedule::Update by ScriptingPlugin.
void script_execution_system(World& world);

/// ECS system: creates managed instances for new ScriptInstance entities.
/// Runs in Schedule::Update, before script_execution_system.
void script_create_system(World& world);

/// ECS system: destroys managed instances for despawned entities.
/// Runs in Schedule::PostUpdate.
void script_destroy_system(World& world);

} // namespace helios

// helios-physics/src/interface/physics_factory.h
//
// Factory function that creates the best available PhysicsWorld backend.
// This header only exposes the base interface; backend selection happens
// inside the .cpp that the library compiles (where Jolt headers are available).
#pragma once

#include <memory>
#include "interface/physics_world.h"

namespace helios::physics {

/// Create a PhysicsWorld using the best available backend.
/// Returns JoltPhysicsWorld when HELIOS_HAS_JOLT, otherwise StubPhysicsWorld.
std::unique_ptr<PhysicsWorld> create_physics_world(const PhysicsConfig& config = {});

} // namespace helios::physics

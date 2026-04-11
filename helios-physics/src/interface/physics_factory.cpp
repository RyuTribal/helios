#include "interface/physics_factory.h"
#include "jolt/jolt_physics_world.h"

namespace helios::physics {

std::unique_ptr<PhysicsWorld> create_physics_world(const PhysicsConfig& config) {
    return std::make_unique<JoltPhysicsWorld>(config);
}

} // namespace helios::physics

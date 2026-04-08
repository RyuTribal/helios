// helios-physics/src/interface/physics_factory.cpp

#include "interface/physics_factory.h"

#if HELIOS_HAS_JOLT
#include "jolt/jolt_physics_world.h"
#else
#include "stub/stub_physics_world.h"
#endif

namespace helios::physics {

std::unique_ptr<PhysicsWorld> create_physics_world(const PhysicsConfig& config) {
#if HELIOS_HAS_JOLT
    return std::make_unique<JoltPhysicsWorld>(config);
#else
    return std::make_unique<StubPhysicsWorld>(config);
#endif
}

} // namespace helios::physics

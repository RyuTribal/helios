// helios-core/src/helios/ecs/schedule.h
#pragma once

#include <cstdint>

namespace helios {

enum class Schedule : uint8_t {
    Startup,     // runs once at app launch
    PreUpdate,   // input polling, event processing
    Update,      // game logic
    FixedUpdate, // physics (fixed timestep, ticks N times per frame)
    PostUpdate,  // transform propagation, hierarchy, cleanup
    PreRender,   // render extraction, editor UI
    COUNT        // sentinel -- always last
};

} // namespace helios

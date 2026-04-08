// helios-core/src/helios/window/window_events.h
#pragma once
#include "helios/window/window_types.h"
#include <cstdint>

namespace helios {

struct WindowResized {
    WindowId  window_id = InvalidWindowId;
    uint32_t  width     = 0;
    uint32_t  height    = 0;
};

struct WindowClosed {
    WindowId  window_id = InvalidWindowId;
};

} // namespace helios

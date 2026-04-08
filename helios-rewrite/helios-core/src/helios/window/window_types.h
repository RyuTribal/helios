// helios-core/src/helios/window/window_types.h
#pragma once
#include <cstdint>
#include <string>

namespace helios {

// Strongly-typed window identifier.
using WindowId = uint32_t;
inline constexpr WindowId InvalidWindowId = 0;

struct WindowDesc {
    std::string title      = "Helios";
    uint32_t    width      = 1280;
    uint32_t    height     = 720;
    bool        vsync      = true;
    bool        fullscreen = false;
    bool        resizable  = true;
};

} // namespace helios

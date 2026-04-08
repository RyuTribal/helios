// helios-core/src/helios/window/window_plugin.h
#pragma once
#include "helios/window/window_types.h"
#include "helios/window/window_events.h"
#include "helios/window/windows.h"
#include "helios/ecs/system_params.h"
#include "helios/ecs/event_storage.h"

namespace helios {

class App; // forward declaration

struct WindowPlugin {
    WindowDesc primary_window = {
        .title  = "Helios",
        .width  = 1280,
        .height = 720,
        .vsync  = true,
    };

    void build(App& app);
};

// System function: polls all windows, emits WindowResized / WindowClosed events.
// Registered in Schedule::PreUpdate by WindowPlugin.
void poll_window_events(ResMut<Windows> windows,
                        EventWriter<WindowResized> resize_writer,
                        EventWriter<WindowClosed>  close_writer);

} // namespace helios

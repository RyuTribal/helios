// helios-core/src/helios/window/window_plugin.cpp
#include "helios/window/window_plugin.h"
#include "helios/window/window.h"
#include "helios/window/windows.h"
#include "helios/window/window_events.h"
#include "helios/ecs/app.h"
#include "helios/core/log_macros.h"

#include <chrono>

// Reuse the Window log channel (defined in window.cpp).
HELIOS_DECLARE_LOG_CHANNEL(Window);

namespace helios {

void WindowPlugin::build(App& app) {
    HELIOS_LOG(Window, Info, "Building WindowPlugin");

    // Create the primary window.
    Window primary(primary_window);
    Windows windows(std::move(primary));

    // Insert resources.
    app.insert_resource<Windows>(std::move(windows));

    // Register events.
    app.add_event<WindowResized>();
    app.add_event<WindowClosed>();

    // Register system.
    app.add_system(Schedule::PreUpdate, poll_window_events, "poll_window_events");

    HELIOS_LOG(Window, Info, "WindowPlugin built: primary window \"{}\" ({}x{})",
               primary_window.title, primary_window.width, primary_window.height);
}

void poll_window_events(ResMut<Windows> windows,
                        EventWriter<WindowResized> resize_writer,
                        EventWriter<WindowClosed>  close_writer)
{
    // Poll GLFW events for all windows.
    windows->poll_all();

    // Track recent resizes to suppress spurious Wayland close events.
    // Some compositors fire close callbacks during fullscreen/workspace
    // transitions. We ignore close requests within 1 second of a resize.
    static auto last_resize_time = std::chrono::steady_clock::time_point{};
    bool had_resize = false;

    // Emit resize events.
    for (auto& [id, window] : *windows) {
        const auto& cb = window.callback_data();
        if (cb.resized) {
            resize_writer.send(WindowResized{
                .window_id = id,
                .width     = cb.new_width,
                .height    = cb.new_height,
            });
            had_resize = true;
        }
    }

    if (had_resize) {
        last_resize_time = std::chrono::steady_clock::now();
    }

    // Check for closing windows.
    auto closing = windows->closing_windows();
    for (auto id : closing) {
        // Suppress close events within 1 second of a resize (Wayland workaround)
        auto now = std::chrono::steady_clock::now();
        auto since_resize = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_resize_time).count();
        if (since_resize < 1000) {
            HELIOS_LOG(Window, Debug, "Ignoring spurious close event ({}ms after resize)", since_resize);
            continue;
        }

        close_writer.send(WindowClosed{ .window_id = id });

        if (windows->should_app_exit_on_close(id)) {
            windows->request_quit();
        }

        if (windows->count() > 1 || !windows->should_app_exit_on_close(id)) {
            windows->destroy(id);
        }
    }

}

} // namespace helios

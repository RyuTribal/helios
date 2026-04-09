// helios-core/src/helios/window/window_plugin.cpp
#include "helios/window/window_plugin.h"
#include "helios/window/window.h"
#include "helios/window/windows.h"
#include "helios/window/window_events.h"
#include "helios/ecs/app.h"
#include "helios/core/log_macros.h"

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
    // Reset close flags before polling so we only see fresh events.
    for (auto& [id, window] : *windows) {
        window.reset_close_flag();
    }

    // Poll GLFW events for all windows.
    windows->poll_all();

    // Emit resize events FIRST (needed to detect same-poll close+resize).
    for (auto& [id, window] : *windows) {
        const auto& cb = window.callback_data();
        if (cb.resized) {
            resize_writer.send(WindowResized{
                .window_id = id,
                .width     = cb.new_width,
                .height    = cb.new_height,
            });
        }
    }

    // Check for closing windows. If a close arrives in the SAME poll as
    // a resize, it's a spurious Hyprland event (the compositor fires close
    // during fullscreen/workspace transitions alongside configure events).
    auto closing = windows->closing_windows();
    for (auto id : closing) {
        const auto& cb = windows->get(id).callback_data();
        if (cb.resized) {
            HELIOS_LOG(Window, Debug,
                "Ignoring spurious close on window {} (arrived with resize)", id);
            // Reset the flag so it doesn't persist to next frame
            windows->get(id).reset_close_flag();
            continue;
        }

        HELIOS_LOG(Window, Info, "Window {} close requested", id);
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

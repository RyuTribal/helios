#pragma once
#include "helios/window/window.h"
#include "helios/window/window_types.h"
#include <unordered_map>

namespace helios {

/// Policy that controls what happens when windows are closed.
enum class WindowClosePolicy {
    /// Closing the primary window exits the app. Secondary windows just close.
    /// This is the default — suitable for games.
    PrimaryExitsApp,

    /// App stays alive as long as ANY window is open.
    /// Suitable for multi-window editors. When the last window closes, app exits.
    LastWindowExitsApp,

    /// Closing any window never exits the app. Manual quit() required.
    /// Suitable for tools that manage their own lifecycle.
    NeverExit,
};

class Windows {
public:
    // Default constructor creates an empty manager (used by HeadlessPlugin).
    Windows() = default;

    // Construct with a primary window already created.
    explicit Windows(Window primary_window);

    ~Windows() = default;

    // Move-only (Window is move-only, so the map is move-only).
    Windows(Windows&&) noexcept = default;
    Windows& operator=(Windows&&) noexcept = default;
    Windows(const Windows&) = delete;
    Windows& operator=(const Windows&) = delete;

    // ---- Window lifecycle ----

    // Create a new window and return its ID.
    WindowId create(const WindowDesc& desc);

    // Destroy a window by ID.
    void destroy(WindowId id);

    // Get a window by ID. Asserts if not found.
    Window& get(WindowId id);
    const Window& get(WindowId id) const;

    // ---- Primary window management ----

    // Get the primary window. Asserts if no primary exists (headless mode).
    Window& primary();
    const Window& primary() const;
    WindowId primary_id() const;
    bool has_primary() const;

    // Switch primary to a different window. The old primary becomes secondary.
    // Asserts if the new id doesn't exist.
    void set_primary(WindowId new_primary);

    // ---- Close policy ----

    WindowClosePolicy close_policy() const { return m_close_policy; }
    void set_close_policy(WindowClosePolicy policy) { m_close_policy = policy; }

    // Check if the app should exit based on the close policy and which
    // window was closed. Called by poll_window_events system after handling
    // WindowClosed events.
    // Returns true if the app should quit.
    bool should_app_exit_on_close(WindowId closed_id) const;

    // ---- Polling ----

    // Poll events for all windows (calls glfwPollEvents once, then clears
    // per-window callback data).
    void poll_all();

    // Collect IDs of windows that should_close (GLFW flag set).
    std::vector<WindowId> closing_windows() const;

    // ---- Iteration ----

    auto begin()       { return m_windows.begin(); }
    auto end()         { return m_windows.end(); }
    auto begin() const { return m_windows.begin(); }
    auto end()   const { return m_windows.end(); }

    // Number of open windows.
    size_t count() const { return m_windows.size(); }

    // App quit request (set by poll_window_events based on close policy).
    void request_quit() { m_quit_requested = true; }
    bool quit_requested() const { return m_quit_requested; }

private:
    std::unordered_map<WindowId, Window> m_windows;
    WindowId m_primary = InvalidWindowId;
    WindowId m_next_id = 1;
    WindowClosePolicy m_close_policy = WindowClosePolicy::PrimaryExitsApp;
    bool m_quit_requested = false;
};

} // namespace helios

// helios-core/src/helios/window/windows.h
#pragma once
#include "helios/window/window.h"
#include "helios/window/window_types.h"
#include <unordered_map>

namespace helios {

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

    // Create a new window and return its ID.
    WindowId create(const WindowDesc& desc);

    // Destroy a window by ID. Cannot destroy the primary window.
    void destroy(WindowId id);

    // Get a window by ID. Asserts if not found.
    Window& get(WindowId id);
    const Window& get(WindowId id) const;

    // Get the primary window. Asserts if no primary exists (headless mode).
    Window& primary();
    const Window& primary() const;
    WindowId primary_id() const;
    bool has_primary() const;

    // Poll events for all windows (calls glfwPollEvents once, then clears
    // per-window callback data).
    void poll_all();

    // Iteration over all windows.
    auto begin()       { return m_windows.begin(); }
    auto end()         { return m_windows.end(); }
    auto begin() const { return m_windows.begin(); }
    auto end()   const { return m_windows.end(); }

    // Number of open windows.
    size_t count() const { return m_windows.size(); }

private:
    std::unordered_map<WindowId, Window> m_windows;
    WindowId m_primary = InvalidWindowId;
    WindowId m_next_id = 1;
};

} // namespace helios

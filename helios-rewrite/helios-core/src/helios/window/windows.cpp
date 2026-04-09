// helios-core/src/helios/window/windows.cpp
#include "helios/window/windows.h"
#include "helios/core/assert.h"
#include "helios/core/log_macros.h"

#include <GLFW/glfw3.h>

// Reuse the Window log channel (defined in window.cpp).
HELIOS_DECLARE_LOG_CHANNEL(Window);

namespace helios {

Windows::Windows(Window primary_window) {
    WindowId id = m_next_id++;
    m_windows.emplace(id, std::move(primary_window));
    m_primary = id;
    HELIOS_LOG(Window, Debug, "Windows manager created with primary window id={}", id);
}

WindowId Windows::create(const WindowDesc& desc) {
    WindowId id = m_next_id++;
    m_windows.emplace(id, Window(desc));
    HELIOS_LOG(Window, Info, "Created window id={} \"{}\"", id, desc.title);
    return id;
}

void Windows::destroy(WindowId id) {
    auto erased = m_windows.erase(id);
    HELIOS_ASSERT(erased > 0, "Window not found for destroy");

    // If we just destroyed the primary, clear the primary id
    if (id == m_primary) {
        m_primary = InvalidWindowId;
        HELIOS_LOG(Window, Info, "Destroyed primary window id={}", id);

        // Auto-promote another window to primary if one exists
        if (!m_windows.empty()) {
            m_primary = m_windows.begin()->first;
            HELIOS_LOG(Window, Info, "Auto-promoted window id={} to primary", m_primary);
        }
    } else {
        HELIOS_LOG(Window, Info, "Destroyed secondary window id={}", id);
    }
}

Window& Windows::get(WindowId id) {
    auto it = m_windows.find(id);
    HELIOS_ASSERT(it != m_windows.end(), "Window not found");
    return it->second;
}

const Window& Windows::get(WindowId id) const {
    auto it = m_windows.find(id);
    HELIOS_ASSERT(it != m_windows.end(), "Window not found");
    return it->second;
}

Window& Windows::primary() {
    HELIOS_ASSERT(m_primary != InvalidWindowId, "No primary window (headless mode?)");
    return get(m_primary);
}

const Window& Windows::primary() const {
    HELIOS_ASSERT(m_primary != InvalidWindowId, "No primary window (headless mode?)");
    return get(m_primary);
}

WindowId Windows::primary_id() const {
    return m_primary;
}

bool Windows::has_primary() const {
    return m_primary != InvalidWindowId;
}

void Windows::set_primary(WindowId new_primary) {
    HELIOS_ASSERT(m_windows.count(new_primary) > 0, "Window not found for set_primary");
    WindowId old = m_primary;
    m_primary = new_primary;
    HELIOS_LOG(Window, Info, "Switched primary window: id={} → id={}", old, new_primary);
}

bool Windows::should_app_exit_on_close(WindowId closed_id) const {
    switch (m_close_policy) {
        case WindowClosePolicy::PrimaryExitsApp:
            return closed_id == m_primary;

        case WindowClosePolicy::LastWindowExitsApp:
            // After this window is destroyed, will there be any left?
            // (The window hasn't been removed yet at the time this is called)
            return m_windows.size() <= 1;

        case WindowClosePolicy::NeverExit:
            return false;
    }
    return false;
}

std::vector<WindowId> Windows::closing_windows() const {
    std::vector<WindowId> result;
    for (const auto& [id, window] : m_windows) {
        if (window.callback_data().close_requested) {
            result.push_back(id);
        }
    }
    return result;
}

void Windows::poll_all() {
    glfwPollEvents();
}

} // namespace helios

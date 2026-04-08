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
    HELIOS_ASSERT(id != m_primary, "Cannot destroy the primary window");
    auto erased = m_windows.erase(id);
    HELIOS_ASSERT(erased > 0, "Window not found for destroy");
    HELIOS_LOG(Window, Info, "Destroyed window id={}", id);
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

void Windows::poll_all() {
    // glfwPollEvents is a global call -- it dispatches callbacks for ALL windows.
    // We call it once here, then each window's callback data has already been
    // populated via the per-window user pointer.
    glfwPollEvents();
}

} // namespace helios

// helios-core/src/helios/window/window.h
#pragma once
#include "helios/window/window_types.h"
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace helios {

// Callback payload accumulated by GLFW callbacks between poll_events() calls.
// The input system reads this each frame.
struct WindowCallbackData {
    // Keyboard
    struct KeyEvent { int key; int action; };
    std::vector<KeyEvent> key_events;

    // Mouse buttons
    struct MouseButtonEvent { int button; int action; };
    std::vector<MouseButtonEvent> mouse_button_events;

    // Mouse position (set continuously by cursor callback)
    double mouse_x = 0.0;
    double mouse_y = 0.0;
    bool   mouse_moved = false;

    // Scroll
    double scroll_x = 0.0;
    double scroll_y = 0.0;

    // Resize
    bool     resized = false;
    uint32_t new_width  = 0;
    uint32_t new_height = 0;

    // Close
    bool close_requested = false;

    void clear() {
        key_events.clear();
        mouse_button_events.clear();
        mouse_moved = false;
        scroll_x = 0.0;
        scroll_y = 0.0;
        resized = false;
        close_requested = false;
    }
};

class Window {
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    // Move-only.
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    uint32_t width() const;
    uint32_t height() const;
    bool should_close() const;
    const std::string& title() const;

    /// Cursor mode: Normal shows the cursor, Captured hides and locks it
    /// (with raw mouse motion when available). Useful for orbit/FPS cameras.
    enum class CursorMode { Normal, Captured };
    void set_cursor_mode(CursorMode mode);
    CursorMode cursor_mode() const;

    // Returns the per-frame callback data accumulated since last clear.
    const WindowCallbackData& callback_data() const;

    // Clear accumulated callback data (called after all systems read it).
    void clear_callback_data();

    // Reset the GLFW should_close flag (called before polling to detect fresh events only).
    void reset_close_flag();

    // Opaque handle for RHI / Vulkan surface creation.
    // Returns GLFWwindow* cast to void*.
    void* native_handle() const;

private:
    struct Impl;                       // defined in window.cpp
    std::unique_ptr<Impl> m_impl;
};

} // namespace helios

#pragma once
#include "helios/input/key_codes.h"
#include <glm/glm.hpp>
#include <array>

namespace helios {

struct RawInput {
    // ---- Query API ----

    // Keyboard
    bool key_pressed(KeyCode key) const;
    bool key_just_pressed(KeyCode key) const;
    bool key_just_released(KeyCode key) const;

    // Mouse
    glm::vec2 mouse_position() const;
    glm::vec2 mouse_delta() const;
    float scroll_delta() const;
    bool mouse_button_pressed(MouseButton btn) const;
    bool mouse_button_just_pressed(MouseButton btn) const;
    bool mouse_button_just_released(MouseButton btn) const;

    // Gamepad (stub -- returns zero/false until gamepad support is added)
    float gamepad_axis(int pad, GamepadAxis axis) const;
    bool  gamepad_button(int pad, GamepadButton btn) const;

    // ---- Internal state (updated by update_raw_input system) ----

    // Keyboard: indexed by GLFW key code (matches KeyCode values).
    // Size 512 covers all GLFW key codes (GLFW_KEY_LAST = 348, with margin).
    static constexpr int KeyArraySize = 512;
    std::array<bool, KeyArraySize> m_keys_current{};
    std::array<bool, KeyArraySize> m_keys_previous{};

    // Mouse position
    glm::vec2 m_mouse_pos{0.0f, 0.0f};
    glm::vec2 m_mouse_pos_prev{0.0f, 0.0f};

    // Scroll (accumulated per frame, reset each frame)
    float m_scroll{0.0f};

    // Mouse buttons: indexed by MouseButton value. Max 8 buttons.
    static constexpr int MouseButtonCount = static_cast<int>(MouseButton::_Count);
    std::array<bool, MouseButtonCount> m_mouse_buttons_current{};
    std::array<bool, MouseButtonCount> m_mouse_buttons_previous{};

    // Called at the start of each frame by the input system
    // to snapshot previous state before processing new events.
    void begin_frame();
};

} // namespace helios

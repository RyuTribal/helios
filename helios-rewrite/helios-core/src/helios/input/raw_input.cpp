// helios-core/src/helios/input/raw_input.cpp
#include "helios/input/raw_input.h"
#include "helios/core/log_macros.h"

HELIOS_DEFINE_LOG_CHANNEL(Input);

namespace helios {

// ---- Keyboard ----

bool RawInput::key_pressed(KeyCode key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= KeyArraySize) return false;
    return m_keys_current[static_cast<size_t>(idx)];
}

bool RawInput::key_just_pressed(KeyCode key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= KeyArraySize) return false;
    return m_keys_current[static_cast<size_t>(idx)] && !m_keys_previous[static_cast<size_t>(idx)];
}

bool RawInput::key_just_released(KeyCode key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= KeyArraySize) return false;
    return !m_keys_current[static_cast<size_t>(idx)] && m_keys_previous[static_cast<size_t>(idx)];
}

// ---- Mouse ----

glm::vec2 RawInput::mouse_position() const {
    return m_mouse_pos;
}

glm::vec2 RawInput::mouse_delta() const {
    return m_mouse_pos - m_mouse_pos_prev;
}

float RawInput::scroll_delta() const {
    return m_scroll;
}

bool RawInput::mouse_button_pressed(MouseButton btn) const {
    int idx = static_cast<int>(btn);
    if (idx < 0 || idx >= MouseButtonCount) return false;
    return m_mouse_buttons_current[static_cast<size_t>(idx)];
}

bool RawInput::mouse_button_just_pressed(MouseButton btn) const {
    int idx = static_cast<int>(btn);
    if (idx < 0 || idx >= MouseButtonCount) return false;
    return m_mouse_buttons_current[static_cast<size_t>(idx)] && !m_mouse_buttons_previous[static_cast<size_t>(idx)];
}

bool RawInput::mouse_button_just_released(MouseButton btn) const {
    int idx = static_cast<int>(btn);
    if (idx < 0 || idx >= MouseButtonCount) return false;
    return !m_mouse_buttons_current[static_cast<size_t>(idx)] && m_mouse_buttons_previous[static_cast<size_t>(idx)];
}

// ---- Gamepad (stub) ----

float RawInput::gamepad_axis(int /*pad*/, GamepadAxis /*axis*/) const {
    return 0.0f;
}

bool RawInput::gamepad_button(int /*pad*/, GamepadButton /*btn*/) const {
    return false;
}

// ---- Frame management ----

void RawInput::begin_frame() {
    m_keys_previous          = m_keys_current;
    m_mouse_pos_prev         = m_mouse_pos;
    m_mouse_buttons_previous = m_mouse_buttons_current;
    m_scroll                 = 0.0f;
    HELIOS_LOG_TRACE(Input, "RawInput::begin_frame");
}

} // namespace helios

// helios-core/src/helios/input/input_map.cpp
#include "helios/input/input_map.h"
#include "helios/input/raw_input.h"
#include "helios/core/assert.h"
#include "helios/core/log_macros.h"

HELIOS_DECLARE_LOG_CHANNEL(Input);

namespace helios {

// ---- Binding definition ----

InputMap& InputMap::action(const std::string& name, KeyCode key) {
    HELIOS_ASSERT(!name.empty(), "Action name must not be empty");
    m_actions[name].keys.push_back(key);
    return *this;
}

InputMap& InputMap::action(const std::string& name, MouseButton btn) {
    HELIOS_ASSERT(!name.empty(), "Action name must not be empty");
    m_actions[name].mouse_buttons.push_back(btn);
    return *this;
}

InputMap& InputMap::action(const std::string& name, GamepadButton btn) {
    HELIOS_ASSERT(!name.empty(), "Action name must not be empty");
    m_actions[name].gamepad_buttons.push_back(btn);
    return *this;
}

InputMap& InputMap::axis(const std::string& name, KeyCode positive, KeyCode negative) {
    HELIOS_ASSERT(!name.empty(), "Axis name must not be empty");
    m_axes[name] = AxisBinding{ .positive = positive, .negative = negative };
    return *this;
}

InputMap& InputMap::axis(const std::string& name, GamepadAxis stick) {
    HELIOS_ASSERT(!name.empty(), "Axis name must not be empty");
    m_axes[name] = AxisBinding{ .gamepad_axis = stick };
    return *this;
}

// ---- Query ----

bool InputMap::pressed(const std::string& action_name) const {
    if (!m_raw) return false;
    auto it = m_actions.find(action_name);
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    for (auto key : binding.keys) {
        if (m_raw->key_pressed(key)) return true;
    }
    for (auto btn : binding.mouse_buttons) {
        if (m_raw->mouse_button_pressed(btn)) return true;
    }
    for (auto btn : binding.gamepad_buttons) {
        if (m_raw->gamepad_button(0, btn)) return true;
    }
    return false;
}

bool InputMap::just_pressed(const std::string& action_name) const {
    if (!m_raw) return false;
    auto it = m_actions.find(action_name);
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    for (auto key : binding.keys) {
        if (m_raw->key_just_pressed(key)) return true;
    }
    for (auto btn : binding.mouse_buttons) {
        if (m_raw->mouse_button_just_pressed(btn)) return true;
    }
    // Gamepad just_pressed not yet implemented (needs previous frame state
    // for gamepad buttons, which will come with full gamepad support).
    return false;
}

bool InputMap::just_released(const std::string& action_name) const {
    if (!m_raw) return false;
    auto it = m_actions.find(action_name);
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    for (auto key : binding.keys) {
        if (m_raw->key_just_released(key)) return true;
    }
    for (auto btn : binding.mouse_buttons) {
        if (m_raw->mouse_button_just_released(btn)) return true;
    }
    return false;
}

float InputMap::axis_value(const std::string& axis_name) const {
    if (!m_raw) return 0.0f;
    auto it = m_axes.find(axis_name);
    if (it == m_axes.end()) return 0.0f;

    const auto& binding = it->second;

    // Keyboard axis: positive key adds +1, negative key adds -1.
    float value = 0.0f;
    if (binding.positive != KeyCode::Unknown && m_raw->key_pressed(binding.positive)) {
        value += 1.0f;
    }
    if (binding.negative != KeyCode::Unknown && m_raw->key_pressed(binding.negative)) {
        value -= 1.0f;
    }

    // Gamepad axis override (if bound and non-zero, use it instead).
    if (binding.gamepad_axis != GamepadAxis::None) {
        float analog = m_raw->gamepad_axis(0, binding.gamepad_axis);
        if (analog != 0.0f) {
            value = analog;
        }
    }

    return value;
}

// ---- Internal ----

void InputMap::set_raw_input(const RawInput* raw) {
    HELIOS_ASSERT(raw != nullptr, "InputMap::set_raw_input called with null pointer");
    m_raw = raw;
    HELIOS_LOG_TRACE(Input, "InputMap::set_raw_input updated");
}

} // namespace helios

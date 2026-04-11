#pragma once
#include "helios/input/key_codes.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

struct RawInput; // forward declaration

class InputMap {
public:
    InputMap() = default;

    // ---- Define bindings (builder pattern, returns *this for chaining) ----

    InputMap& action(const std::string& name, KeyCode key);
    InputMap& action(const std::string& name, MouseButton btn);
    InputMap& action(const std::string& name, GamepadButton btn);

    InputMap& axis(const std::string& name, KeyCode positive, KeyCode negative);
    InputMap& axis(const std::string& name, GamepadAxis stick);

    // ---- Query (call in game systems) ----

    // Returns true if any binding for this action is currently pressed.
    bool pressed(const std::string& action_name) const;

    // Returns true on the frame the action was first pressed.
    bool just_pressed(const std::string& action_name) const;

    // Returns true on the frame the action was released.
    bool just_released(const std::string& action_name) const;

    // Returns -1.0 to 1.0 for keyboard axes, or raw analog value for gamepad.
    float axis_value(const std::string& axis_name) const;

    // ---- Internal (set by update_action_map system) ----
    void set_raw_input(const RawInput* raw);

private:
    struct ActionBinding {
        std::vector<KeyCode>       keys;
        std::vector<MouseButton>   mouse_buttons;
        std::vector<GamepadButton> gamepad_buttons;
    };

    struct AxisBinding {
        KeyCode     positive     = KeyCode::Unknown;
        KeyCode     negative     = KeyCode::Unknown;
        GamepadAxis gamepad_axis = GamepadAxis::None;
    };

    std::unordered_map<std::string, ActionBinding> m_actions;
    std::unordered_map<std::string, AxisBinding>   m_axes;
    const RawInput* m_raw = nullptr;
};

} // namespace helios

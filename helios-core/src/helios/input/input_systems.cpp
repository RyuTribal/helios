#include "helios/input/input_systems.h"
#include "helios/input/raw_input.h"
#include "helios/input/input_map.h"
#include "helios/window/windows.h"
#include "helios/window/window.h"
#include "helios/ecs/system_params.h"
#include "helios/core/assert.h"
#include "helios/core/log_macros.h"

#include <GLFW/glfw3.h>

HELIOS_DECLARE_LOG_CHANNEL(Input);

namespace helios {

void update_raw_input(ResMut<RawInput> input, ResMut<Windows> windows) {
    // Snapshot previous frame state before applying new events.
    input->begin_frame();

    // InputPlugin requires WindowPlugin, which guarantees a primary window
    // exists.  Assert once rather than branching every frame.
    HELIOS_ASSERT(windows->has_primary(),
                  "update_raw_input requires a primary window "
                  "(InputPlugin depends on WindowPlugin)");

    const Window& primary = windows->primary();
    const WindowCallbackData& cb = primary.callback_data();

    for (const auto& ev : cb.key_events) {
        int key = ev.key;
        if (key >= 0 && key < RawInput::KeyArraySize) {
            if (ev.action == GLFW_PRESS || ev.action == GLFW_REPEAT) {
                input->m_keys_current[static_cast<size_t>(key)] = true;
            } else if (ev.action == GLFW_RELEASE) {
                input->m_keys_current[static_cast<size_t>(key)] = false;
            }
        }
    }

    for (const auto& ev : cb.mouse_button_events) {
        int btn = ev.button;
        if (btn >= 0 && btn < RawInput::MouseButtonCount) {
            if (ev.action == GLFW_PRESS) {
                input->m_mouse_buttons_current[static_cast<size_t>(btn)] = true;
            } else if (ev.action == GLFW_RELEASE) {
                input->m_mouse_buttons_current[static_cast<size_t>(btn)] = false;
            }
        }
    }

    if (cb.mouse_moved) {
        input->m_mouse_pos = glm::vec2(
            static_cast<float>(cb.mouse_x),
            static_cast<float>(cb.mouse_y)
        );
    }

    // Accumulate scroll delta (already reset in begin_frame).
    input->m_scroll = static_cast<float>(cb.scroll_y);

    HELIOS_LOG_TRACE(Input, "update_raw_input: {} key events, {} mouse button events",
                     cb.key_events.size(), cb.mouse_button_events.size());

    // Prevents stale events (especially scroll) from persisting across frames.
    windows->primary().clear_callback_data();
}

void update_action_map(ResMut<InputMap> map, Res<RawInput> input) {
    map->set_raw_input(&(*input));
}

} // namespace helios

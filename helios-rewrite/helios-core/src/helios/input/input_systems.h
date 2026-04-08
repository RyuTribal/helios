// helios-core/src/helios/input/input_systems.h
#pragma once
#include "helios/ecs/system_params.h"

namespace helios {

// Forward declarations for resource types.
class Windows;
struct RawInput;
class InputMap;

// Updates RawInput from the primary window's GLFW callback data.
// Must run after poll_window_events.
void update_raw_input(ResMut<RawInput> input, Res<Windows> windows);

// Updates InputMap's internal pointer to RawInput so action queries work.
// Must run after update_raw_input.
void update_action_map(ResMut<InputMap> map, Res<RawInput> input);

} // namespace helios

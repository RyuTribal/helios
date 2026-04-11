# RawInput

The `RawInput` struct provides a direct way to query the current state of keyboard and mouse input.

## Overview

The `RawInput` resource is updated every frame by the input system. It captures snapshots of key and button states, allowing systems to check for presses, releases, and continuous holds.

## Methods

### Keyboard State

| Method | Description |
| :--- | :--- |
| `key_pressed(key)` | Checks if the given key is currently held down. |
| `key_just_pressed(key)` | Checks if the key was first pressed during the current frame. |
| `key_just_released(key)` | Checks if the key was released during the current frame. |

### Mouse State

| Method | Description |
| :--- | :--- |
| `mouse_position()` | Returns the current cursor position as a `glm::vec2`. |
| `mouse_delta()` | Returns the movement delta since the previous frame. |
| `scroll_delta()` | Returns the scroll wheel delta for the current frame. |
| `mouse_button_pressed(btn)` | Checks if a mouse button is held down. |
| `mouse_button_just_pressed(btn)`| Checks if a mouse button was just pressed. |
| `mouse_button_just_released(btn)`| Checks if a mouse button was just released. |

## Usage Example

```cpp
// Within a system...
const auto& input = world.get_resource<RawInput>();

if (input.key_just_pressed(KeyCode::Space)) {
    // Perform jump
}

if (input.mouse_button_pressed(MouseButton::Left)) {
    // Perform fire
}
```

## Related Pages
- [InputMap](input_map.md)
- [Window](../window/window.md)

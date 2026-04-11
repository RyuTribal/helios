# InputMap

The `InputMap` class allows developers to define high-level actions and axes, decoupling gameplay logic from specific keys or mouse buttons.

## Overview

Instead of checking for `KeyCode::W`, systems can check for a `"MoveForward"` action. This allows for easy remapping and supports multiple input devices (keyboard, mouse, gamepad).

## Methods

### Binding (Builder Pattern)

| Method | Description |
| :--- | :--- |
| `action(name, key)` | Binds a key to an action. |
| `action(name, button)` | Binds a mouse or gamepad button to an action. |
| `axis(name, pos, neg)` | Binds two keys to a 1D axis (-1.0 to 1.0). |
| `axis(name, stick)` | Binds a gamepad stick to an axis. |

### Querying

| Method | Description |
| :--- | :--- |
| `pressed(name)` | Returns `true` if any binding for this action is currently pressed. |
| `just_pressed(name)` | Returns `true` on the frame the action was first pressed. |
| `just_released(name)` | Returns `true` on the frame the action was released. |
| `axis_value(name)` | Returns the value of the named axis. |

## Usage Example

```cpp
// 1. Configure the map (e.g., during app startup)
InputMap map;
map.action("Jump", KeyCode::Space)
   .action("Jump", GamepadButton::South)
   .axis("Vertical", KeyCode::W, KeyCode::S);

// 2. Query in gameplay logic
if (map.just_pressed("Jump")) {
    player.jump();
}

float vertical = map.axis_value("Vertical"); // 1.0 if W pressed, -1.0 if S pressed
```

### Remapping Support

`InputMap` stores multiple bindings per action. This means an action like `"Fire"` can be bound to both the Left Mouse Button and a Gamepad Trigger simultaneously.

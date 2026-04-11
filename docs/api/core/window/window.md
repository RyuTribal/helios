# Window

The `Window` class represents a single OS window, providing properties for size and title, as well as handling input events.

## Overview

A `Window` is created through the `Windows` manager and provides a bridge between the OS and the Helios engine. It manages a GLFW window and accumulates input events for the engine's input system to process.

## Methods

### Properties

| Method | Description |
| :--- | :--- |
| `width()` | Returns the current width of the window. |
| `height()` | Returns the current height of the window. |
| `title()` | Returns the title of the window. |
| `should_close()` | Checks if a close request has been made. |
| `native_handle()` | Returns the underlying GLFW window handle (cast to `void*`). |

### Cursor Management

| Method | Description |
| :--- | :--- |
| `set_cursor_mode(mode)` | Sets the cursor mode: `Normal` or `Captured`. |
| `cursor_mode()` | Gets the current cursor mode. |
| `warp_cursor_to_grab()` | Warps the cursor back to the grab point (used for orbit/FPS cameras). |

## Input and Callbacks

The `Window` accumulates raw input events (keyboard, mouse, scroll, resize) in a `WindowCallbackData` structure.

- `callback_data()`: Returns the accumulated event data since the last clear.
- `clear_callback_data()`: Clears the accumulated events (called by the input system).

### WindowCallbackData

A struct containing:
- `key_events`: Vector of key actions.
- `mouse_button_events`: Vector of mouse button actions.
- `mouse_x`, `mouse_y`: Current cursor position.
- `scroll_x`, `scroll_y`: Cumulative scroll delta.
- `resized`: Boolean flag for window resize.
- `close_requested`: Boolean flag for close request.

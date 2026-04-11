# Windows

The `Windows` manager tracks all open windows, handles primary window access, and manages the application's close policy.

## Overview

The `Windows` manager is a world resource that provides access to all windows. It tracks a "primary" window and defines what happens when windows are closed.

## Methods

### Lifecycle Management

| Method | Description |
| :--- | :--- |
| `create(desc)` | Create a new window with the given description. Returns a `WindowId`. |
| `destroy(id)` | Destroy a window by its ID. |
| `get(id)` | Get a reference to a window by its ID. |

### Primary Window Access

| Method | Description |
| :--- | :--- |
| `primary()` | Get a reference to the primary window. |
| `primary_id()` | Get the `WindowId` of the primary window. |
| `has_primary()` | Checks if a primary window exists (returns `false` in headless mode). |
| `set_primary(id)` | Changes the primary window to the given ID. |

### Close Policy

| Method | Description |
| :--- | :--- |
| `set_close_policy(policy)` | Sets the behavior for window close requests. |
| `close_policy()` | Returns the current close policy. |
| `request_quit()` | Explicitly request the application to quit. |
| `quit_requested()` | Checks if a quit request has been made. |

## Close Policies

The `WindowClosePolicy` enum defines how window closures affect the application's lifecycle:

- `PrimaryExitsApp`: Closing the primary window exits the app. Secondary windows just close. (Default for games)
- `LastWindowExitsApp`: The app exits when the last window is closed. (Suitable for editors)
- `NeverExit`: Closing a window never exits the app automatically. Manual `quit()` required. (Suitable for specialized tools)

## Related Pages
- [Window](window.md)

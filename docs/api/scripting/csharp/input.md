# Input

The `Input` class is a static helper for checking the state of the keyboard and mouse.

## Methods

| Method | Description |
| :--- | :--- |
| `bool IsKeyPressed(int)` | Check if a key is currently held down. |
| `bool IsKeyJustPressed(int)` | Check if a key was pressed during this frame. |
| `bool IsMouseButtonPressed(int)` | Check if a mouse button is currently held down. |
| `void SetCursorMode(int)` | Set the cursor mode (0: Normal, 1: Captured/Hidden). |
| `int GetCursorMode()` | Get the current cursor mode. |

## Properties

| Property | Type | Description |
| :--- | :--- | :--- |
| `MousePosition` | `Vector2` | Current mouse screen coordinates. |
| `MouseDelta` | `Vector2` | Mouse movement since the last frame. |
| `ScrollDelta` | `float` | Scroll wheel movement since the last frame. |

## Usage

```csharp
public override void OnUpdate(float delta) {
    if (Input.IsKeyJustPressed(71)) { // 'G'
        Log.Info("G was pressed this frame!");
    }
    
    if (Input.IsMouseButtonPressed(0)) { // Left click
        var mousePos = Input.MousePosition;
        Log.Info($"Mouse clicked at: {mousePos.X}, {mousePos.Y}");
    }
}
```

### Note on Keycodes
Helios uses standard GLFW keycodes. For common keys, the keycode typically matches the ASCII value for the uppercase character (e.g., 'A' is 65, 'S' is 83).

## Related Pages

- [ScriptBehaviour](script_behaviour.md)
- [Input Map API](../../core/input/input_map.md)

using System.Numerics;

namespace Helios;

public static class Input
{
    public static bool IsKeyPressed(KeyCode key) => NativeAPI.IsKeyPressed((int)key);
    public static bool IsMouseButtonPressed(MouseButton button) => NativeAPI.IsMouseButtonPressed((int)button);
    public static Vector2 MousePosition => NativeAPI.GetMousePosition();
}

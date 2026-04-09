using System.Numerics;

namespace Helios;

public static class Input
{
    public static bool IsKeyPressed(int keycode) => NativeAPI.IsKeyPressed(keycode);
    public static bool IsMouseButtonPressed(int button) => NativeAPI.IsMouseButtonPressed(button);
    public static Vector2 MousePosition => NativeAPI.GetMousePosition();
}

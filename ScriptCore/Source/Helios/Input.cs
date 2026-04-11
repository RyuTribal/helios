using System.Numerics;

namespace Helios;

public static class Input
{
    public static bool IsKeyPressed(int keycode) => NativeAPI.IsKeyPressed(keycode);
    public static bool IsKeyJustPressed(int keycode) => NativeAPI.IsKeyJustPressed(keycode);
    public static bool IsMouseButtonPressed(int button) => NativeAPI.IsMouseButtonPressed(button);
    public static Vector2 MousePosition => NativeAPI.GetMousePosition();
    public static Vector2 MouseDelta => NativeAPI.GetMouseDelta();
    public static float ScrollDelta => NativeAPI.GetScrollDelta();

    /// <summary>Set cursor mode: 0 = normal, 1 = captured/hidden.</summary>
    public static void SetCursorMode(int mode) => NativeAPI.SetCursorMode(mode);

    /// <summary>Get cursor mode: 0 = normal, 1 = captured/hidden.</summary>
    public static int GetCursorMode() => NativeAPI.GetCursorMode();
}

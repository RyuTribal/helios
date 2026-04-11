namespace Helios;

/// <summary>
/// Logging helpers that forward to the C++ engine logging system.
/// </summary>
public static class Log
{
    public static void Trace(string message) => NativeAPI.Log(0, message);
    public static void Info(string message) => NativeAPI.Log(1, message);
    public static void Warn(string message) => NativeAPI.Log(2, message);
    public static void Error(string message) => NativeAPI.Log(3, message);
}

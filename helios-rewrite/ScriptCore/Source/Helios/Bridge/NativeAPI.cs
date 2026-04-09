using System.Numerics;
using System.Text;
using Helios.Bridge;

namespace Helios;

/// <summary>
/// Static accessor for NativeEngineAPI function pointers.
/// All methods pass WorldContext as the first argument to every native call.
/// </summary>
public static unsafe class NativeAPI
{
    private static NativeEngineAPI* Api;

    internal static void Init(NativeEngineAPI* api)
    {
        Api = api;
    }

    // -- Logging --------------------------------------------------------------

    internal static void Log(int level, string message)
    {
        var bytes = Encoding.UTF8.GetBytes(message + '\0');
        fixed (byte* ptr = bytes)
            Api->Log(Api->WorldContext, level, ptr);
    }

    // -- Entity ---------------------------------------------------------------

    internal static ulong Spawn()
        => Api->Spawn(Api->WorldContext);

    internal static void Despawn(ulong entityId)
        => Api->Despawn(Api->WorldContext, entityId);

    internal static bool IsAlive(ulong entityId)
        => Api->IsAlive(Api->WorldContext, entityId);

    // -- Component (generic) --------------------------------------------------

    internal static bool HasComponent(ulong entityId, string componentName)
    {
        var bytes = Encoding.UTF8.GetBytes(componentName + '\0');
        fixed (byte* ptr = bytes)
            return Api->HasComponent(Api->WorldContext, entityId, ptr);
    }

    // -- Transform shortcuts --------------------------------------------------

    internal static Vector3 TransformGetTranslation(ulong id)
    {
        Vector3 v;
        Api->TransformGetTranslation(Api->WorldContext, id, (float*)&v);
        return v;
    }

    internal static void TransformSetTranslation(ulong id, Vector3 value)
        => Api->TransformSetTranslation(Api->WorldContext, id, (float*)&value);

    internal static Vector3 TransformGetRotation(ulong id)
    {
        Vector3 v;
        Api->TransformGetRotation(Api->WorldContext, id, (float*)&v);
        return v;
    }

    internal static void TransformSetRotation(ulong id, Vector3 value)
        => Api->TransformSetRotation(Api->WorldContext, id, (float*)&value);

    internal static Vector3 TransformGetScale(ulong id)
    {
        Vector3 v;
        Api->TransformGetScale(Api->WorldContext, id, (float*)&v);
        return v;
    }

    internal static void TransformSetScale(ulong id, Vector3 value)
        => Api->TransformSetScale(Api->WorldContext, id, (float*)&value);

    // -- Input ----------------------------------------------------------------

    internal static bool IsKeyPressed(int keycode)
        => Api->IsKeyPressed(Api->WorldContext, keycode);

    internal static bool IsMouseButtonPressed(int button)
        => Api->IsMouseButtonPressed(Api->WorldContext, button);

    internal static Vector2 GetMousePosition()
    {
        float x, y;
        Api->GetMousePosition(Api->WorldContext, &x, &y);
        return new Vector2(x, y);
    }
}

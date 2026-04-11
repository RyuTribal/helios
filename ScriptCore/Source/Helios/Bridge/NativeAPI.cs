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
        => Api->IsAlive(Api->WorldContext, entityId) != 0;

    // -- Component (generic) --------------------------------------------------

    internal static bool HasComponent(ulong entityId, string componentName)
    {
        var bytes = Encoding.UTF8.GetBytes(componentName + '\0');
        fixed (byte* ptr = bytes)
            return Api->HasComponent(Api->WorldContext, entityId, ptr) != 0;
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
        => Api->IsKeyPressed(Api->WorldContext, keycode) != 0;

    internal static bool IsMouseButtonPressed(int button)
        => Api->IsMouseButtonPressed(Api->WorldContext, button) != 0;

    internal static Vector2 GetMousePosition()
    {
        float x, y;
        Api->GetMousePosition(Api->WorldContext, &x, &y);
        return new Vector2(x, y);
    }

    // -- Physics --------------------------------------------------------------

    internal static void PhysicsApplyForce(ulong entityId, Vector3 force)
        => Api->PhysicsApplyForce(Api->WorldContext, entityId, (float*)&force);

    internal static void PhysicsApplyImpulse(ulong entityId, Vector3 impulse)
        => Api->PhysicsApplyImpulse(Api->WorldContext, entityId, (float*)&impulse);

    internal static void PhysicsApplyTorque(ulong entityId, Vector3 torque)
        => Api->PhysicsApplyTorque(Api->WorldContext, entityId, (float*)&torque);

    internal static void PhysicsSetLinearVelocity(ulong entityId, Vector3 velocity)
        => Api->PhysicsSetLinearVelocity(Api->WorldContext, entityId, (float*)&velocity);

    internal static Vector3 PhysicsGetLinearVelocity(ulong entityId)
    {
        Vector3 v;
        Api->PhysicsGetLinearVelocity(Api->WorldContext, entityId, (float*)&v);
        return v;
    }

    internal static void PhysicsSetAngularVelocity(ulong entityId, Vector3 velocity)
        => Api->PhysicsSetAngularVelocity(Api->WorldContext, entityId, (float*)&velocity);

    internal static Vector3 PhysicsGetAngularVelocity(ulong entityId)
    {
        Vector3 v;
        Api->PhysicsGetAngularVelocity(Api->WorldContext, entityId, (float*)&v);
        return v;
    }

    // -- Mouse delta / scroll -------------------------------------------------

    internal static Vector2 GetMouseDelta()
    {
        float dx, dy;
        Api->GetMouseDelta(Api->WorldContext, &dx, &dy);
        return new Vector2(dx, dy);
    }

    internal static float GetScrollDelta()
        => Api->GetScrollDelta(Api->WorldContext);

    // -- Cursor mode ----------------------------------------------------------

    internal static void SetCursorMode(int mode)
        => Api->SetCursorMode(Api->WorldContext, mode);

    internal static int GetCursorMode()
        => Api->GetCursorMode(Api->WorldContext);

    // -- Physics teleport -----------------------------------------------------

    internal static void PhysicsTeleport(ulong entityId, Vector3 position, Vector4 rotation)
    {
        Api->PhysicsTeleport(Api->WorldContext, entityId, (float*)&position, (float*)&rotation);
    }

    // -- Tag name -------------------------------------------------------------

    internal static string GetTagName(ulong entityId)
    {
        const int bufferSize = 256;
        byte* buffer = stackalloc byte[bufferSize];
        Api->GetTagName(Api->WorldContext, entityId, buffer, bufferSize);
        int len = 0;
        while (len < bufferSize && buffer[len] != 0) len++;
        return Encoding.UTF8.GetString(buffer, len);
    }

    // -- Time -----------------------------------------------------------------

    internal static float GetDeltaTime()
        => Api->GetDeltaTime(Api->WorldContext);

    // -- Input: single-press --------------------------------------------------

    internal static bool IsKeyJustPressed(int keycode)
        => Api->IsKeyJustPressed(Api->WorldContext, keycode) != 0;

    // -- Transform: look-at ---------------------------------------------------

    internal static void TransformLookAt(ulong entityId, Vector3 target, Vector3 up)
    {
        Api->TransformLookAt(Api->WorldContext, entityId, (float*)&target, (float*)&up);
    }

    // -- Assets ---------------------------------------------------------------

    internal static ulong AssetLoad(string path)
    {
        var bytes = Encoding.UTF8.GetBytes(path + '\0');
        fixed (byte* ptr = bytes)
            return Api->AssetLoad(Api->WorldContext, ptr);
    }

    internal static bool AssetIsLoaded(ulong handle)
        => Api->AssetIsLoaded(Api->WorldContext, handle) != 0;

    // -- Audio ----------------------------------------------------------------

    internal static uint AudioGetSoundId(string name)
    {
        var bytes = Encoding.UTF8.GetBytes(name + '\0');
        fixed (byte* ptr = bytes)
            return Api->AudioGetSoundId(Api->WorldContext, ptr);
    }

    internal static void AudioPlaySoundAt(uint soundId, float x, float y, float z)
        => Api->AudioPlaySoundAt(Api->WorldContext, soundId, x, y, z);

    internal static void AudioPlayFile(string path, float x, float y, float z,
                                        float volume, bool loop)
    {
        var bytes = Encoding.UTF8.GetBytes(path + '\0');
        fixed (byte* ptr = bytes)
            Api->AudioPlayFile(Api->WorldContext, ptr, x, y, z, volume, loop ? (byte)1 : (byte)0);
    }

    internal static void AudioPlayHandle(ulong handle, float x, float y, float z,
                                          float volume, bool loop)
        => Api->AudioPlayHandle(Api->WorldContext, handle, x, y, z, volume, loop ? (byte)1 : (byte)0);

    // -- Scene ----------------------------------------------------------------

    internal static void SceneLoad(string path)
    {
        var bytes = Encoding.UTF8.GetBytes(path + '\0');
        fixed (byte* ptr = bytes)
            Api->SceneLoad(Api->WorldContext, ptr);
    }

    internal static void SceneInstantiate(string path)
    {
        var bytes = Encoding.UTF8.GetBytes(path + '\0');
        fixed (byte* ptr = bytes)
            Api->SceneInstantiate(Api->WorldContext, ptr);
    }

    internal static bool SceneIsReady(string path)
    {
        var bytes = Encoding.UTF8.GetBytes(path + '\0');
        fixed (byte* ptr = bytes)
            return Api->SceneIsReady(Api->WorldContext, ptr) != 0;
    }
}

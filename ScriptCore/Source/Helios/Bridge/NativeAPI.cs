using System.Numerics;
using System.Text;
using Helios.Bridge;

namespace Helios;

public static unsafe class NativeAPI
{
    private static NativeEngineAPI* Api;

    internal static void Init(NativeEngineAPI* api)
    {
        Api = api;
    }

    // ── Input (3) ──────────────────────────────────────────────

    internal static bool IsKeyPressed(int keycode)
        => Api->IsKeyPressed(keycode);

    internal static bool IsMouseButtonPressed(int button)
        => Api->IsMouseButtonPressed(button);

    internal static Vector2 GetMousePosition()
    {
        float x, y;
        Api->GetMousePosition(&x, &y);
        return new Vector2(x, y);
    }

    // ── Entity (2) ─────────────────────────────────────────────

    internal static bool EntityHasComponent(ulong entityId, string componentName)
    {
        var bytes = Encoding.UTF8.GetBytes(componentName + '\0');
        fixed (byte* ptr = bytes)
            return Api->EntityHasComponent(entityId, ptr);
    }

    internal static void EntityDestroy(ulong entityId)
        => Api->EntityDestroy(entityId);

    // ── Transform (6) ──────────────────────────────────────────

    internal static Vector3 TransformGetTranslation(ulong id)
    {
        Vector3 v;
        Api->TransformGetTranslation(id, (float*)&v);
        return v;
    }

    internal static void TransformSetTranslation(ulong id, Vector3 value)
        => Api->TransformSetTranslation(id, (float*)&value);

    internal static Vector3 TransformGetRotation(ulong id)
    {
        Vector3 v;
        Api->TransformGetRotation(id, (float*)&v);
        return v;
    }

    internal static void TransformSetRotation(ulong id, Vector3 value)
        => Api->TransformSetRotation(id, (float*)&value);

    internal static Vector3 TransformGetScale(ulong id)
    {
        Vector3 v;
        Api->TransformGetScale(id, (float*)&v);
        return v;
    }

    internal static void TransformSetScale(ulong id, Vector3 value)
        => Api->TransformSetScale(id, (float*)&value);

    // ── Camera (8) ─────────────────────────────────────────────

    internal static void CameraRotateAroundEntity(ulong id, Vector2 rotation, float speed, bool inverse)
        => Api->CameraRotateAroundEntity(id, (float*)&rotation, speed, inverse);

    internal static void CameraRotate(ulong id, Vector2 rotation, float speed, bool inverse)
        => Api->CameraRotate(id, (float*)&rotation, speed, inverse);

    internal static Vector3 CameraGetForwardDirection(ulong id)
    {
        Vector3 v;
        Api->CameraGetForwardDirection(id, (float*)&v);
        return v;
    }

    internal static Vector3 CameraGetRightDirection(ulong id)
    {
        Vector3 v;
        Api->CameraGetRightDirection(id, (float*)&v);
        return v;
    }

    internal static Vector3 CameraGetPosition(ulong id)
    {
        Vector3 v;
        Api->CameraGetPosition(id, (float*)&v);
        return v;
    }

    internal static Vector3 CameraGetRotation(ulong id)
    {
        Vector3 v;
        Api->CameraGetRotation(id, (float*)&v);
        return v;
    }

    internal static void CameraSetPosition(ulong id, Vector3 value)
        => Api->CameraSetPosition(id, (float*)&value);

    internal static void CameraSetRotation(ulong id, Vector3 value)
        => Api->CameraSetRotation(id, (float*)&value);

    // ── Sounds (2) ─────────────────────────────────────────────

    internal static void SoundsPlayGlobal(ulong id, int index)
        => Api->SoundsPlayGlobal(id, index);

    internal static void SoundsPlayLocal(ulong id, int index)
        => Api->SoundsPlayLocal(id, index);

    // ── Box Collider (7) ───────────────────────────────────────

    internal static Vector3 BoxColliderGetLinearVelocity(ulong id)
    {
        Vector3 v;
        Api->BoxColliderGetLinearVelocity(id, (float*)&v);
        return v;
    }

    internal static void BoxColliderSetLinearVelocity(ulong id, Vector3 value)
        => Api->BoxColliderSetLinearVelocity(id, (float*)&value);

    internal static void BoxColliderAddLinearVelocity(ulong id, Vector3 value)
        => Api->BoxColliderAddLinearVelocity(id, (float*)&value);

    internal static void BoxColliderAddAngularVelocity(ulong id, Vector3 value)
        => Api->BoxColliderAddAngularVelocity(id, (float*)&value);

    internal static void BoxColliderAddImpulse(ulong id, Vector3 value)
        => Api->BoxColliderAddImpulse(id, (float*)&value);

    internal static void BoxColliderAddAngularImpulse(ulong id, Vector3 value)
        => Api->BoxColliderAddAngularImpulse(id, (float*)&value);

    internal static void BoxColliderAddLinearAngularImpulse(ulong id, Vector3 linear, Vector3 angular)
        => Api->BoxColliderAddLinearAngularImpulse(id, (float*)&linear, (float*)&angular);

    // ── Sphere Collider (7) ────────────────────────────────────

    internal static Vector3 SphereColliderGetLinearVelocity(ulong id)
    {
        Vector3 v;
        Api->SphereColliderGetLinearVelocity(id, (float*)&v);
        return v;
    }

    internal static void SphereColliderSetLinearVelocity(ulong id, Vector3 value)
        => Api->SphereColliderSetLinearVelocity(id, (float*)&value);

    internal static void SphereColliderAddLinearVelocity(ulong id, Vector3 value)
        => Api->SphereColliderAddLinearVelocity(id, (float*)&value);

    internal static void SphereColliderAddAngularVelocity(ulong id, Vector3 value)
        => Api->SphereColliderAddAngularVelocity(id, (float*)&value);

    internal static void SphereColliderAddImpulse(ulong id, Vector3 value)
        => Api->SphereColliderAddImpulse(id, (float*)&value);

    internal static void SphereColliderAddAngularImpulse(ulong id, Vector3 value)
        => Api->SphereColliderAddAngularImpulse(id, (float*)&value);

    internal static void SphereColliderAddLinearAngularImpulse(ulong id, Vector3 linear, Vector3 angular)
        => Api->SphereColliderAddLinearAngularImpulse(id, (float*)&linear, (float*)&angular);

    // ── Character Controller (11) ──────────────────────────────

    internal static Vector3 CharControllerGetLinearVelocity(ulong id)
    {
        Vector3 v;
        Api->CharControllerGetLinearVelocity(id, (float*)&v);
        return v;
    }

    internal static void CharControllerSetLinearVelocity(ulong id, Vector3 value)
        => Api->CharControllerSetLinearVelocity(id, (float*)&value);

    internal static void CharControllerAddLinearVelocity(ulong id, Vector3 value)
        => Api->CharControllerAddLinearVelocity(id, (float*)&value);

    internal static void CharControllerAddAngularVelocity(ulong id, Vector3 value)
        => Api->CharControllerAddAngularVelocity(id, (float*)&value);

    internal static void CharControllerAddImpulse(ulong id, Vector3 value)
        => Api->CharControllerAddImpulse(id, (float*)&value);

    internal static void CharControllerAddAngularImpulse(ulong id, Vector3 value)
        => Api->CharControllerAddAngularImpulse(id, (float*)&value);

    internal static void CharControllerAddLinearAngularImpulse(ulong id, Vector3 linear, Vector3 angular)
        => Api->CharControllerAddLinearAngularImpulse(id, (float*)&linear, (float*)&angular);

    internal static bool CharControllerIsGrounded(ulong id)
        => Api->CharControllerIsGrounded(id);

    internal static Vector3 CharControllerGetRotation(ulong id)
    {
        Vector3 v;
        Api->CharControllerGetRotation(id, (float*)&v);
        return v;
    }

    internal static void CharControllerSetRotation(ulong id, Vector3 value)
        => Api->CharControllerSetRotation(id, (float*)&value);

    internal static void CharControllerRotate(ulong id, Vector3 value)
        => Api->CharControllerRotate(id, (float*)&value);
}

using System.Runtime.InteropServices;

namespace Helios.Bridge;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeEngineAPI
{
    // Input (3)
    public delegate* unmanaged<int, bool> IsKeyPressed;
    public delegate* unmanaged<int, bool> IsMouseButtonPressed;
    public delegate* unmanaged<float*, float*, void> GetMousePosition;

    // Entity (2)
    public delegate* unmanaged<ulong, byte*, bool> EntityHasComponent;
    public delegate* unmanaged<ulong, void> EntityDestroy;

    // Transform (6)
    public delegate* unmanaged<ulong, float*, void> TransformGetTranslation;
    public delegate* unmanaged<ulong, float*, void> TransformSetTranslation;
    public delegate* unmanaged<ulong, float*, void> TransformGetRotation;
    public delegate* unmanaged<ulong, float*, void> TransformSetRotation;
    public delegate* unmanaged<ulong, float*, void> TransformGetScale;
    public delegate* unmanaged<ulong, float*, void> TransformSetScale;

    // Camera (8)
    public delegate* unmanaged<ulong, float*, float, bool, void> CameraRotateAroundEntity;
    public delegate* unmanaged<ulong, float*, float, bool, void> CameraRotate;
    public delegate* unmanaged<ulong, float*, void> CameraGetForwardDirection;
    public delegate* unmanaged<ulong, float*, void> CameraGetRightDirection;
    public delegate* unmanaged<ulong, float*, void> CameraGetPosition;
    public delegate* unmanaged<ulong, float*, void> CameraGetRotation;
    public delegate* unmanaged<ulong, float*, void> CameraSetPosition;
    public delegate* unmanaged<ulong, float*, void> CameraSetRotation;

    // Sounds (2)
    public delegate* unmanaged<ulong, int, void> SoundsPlayGlobal;
    public delegate* unmanaged<ulong, int, void> SoundsPlayLocal;

    // Box Collider (7)
    public delegate* unmanaged<ulong, float*, void> BoxColliderGetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderSetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddAngularVelocity;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddImpulse;
    public delegate* unmanaged<ulong, float*, void> BoxColliderAddAngularImpulse;
    public delegate* unmanaged<ulong, float*, float*, void> BoxColliderAddLinearAngularImpulse;

    // Sphere Collider (7)
    public delegate* unmanaged<ulong, float*, void> SphereColliderGetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderSetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddAngularVelocity;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddImpulse;
    public delegate* unmanaged<ulong, float*, void> SphereColliderAddAngularImpulse;
    public delegate* unmanaged<ulong, float*, float*, void> SphereColliderAddLinearAngularImpulse;

    // Character Controller (11)
    public delegate* unmanaged<ulong, float*, void> CharControllerGetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerSetLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddLinearVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddAngularVelocity;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddImpulse;
    public delegate* unmanaged<ulong, float*, void> CharControllerAddAngularImpulse;
    public delegate* unmanaged<ulong, float*, float*, void> CharControllerAddLinearAngularImpulse;
    public delegate* unmanaged<ulong, bool> CharControllerIsGrounded;
    public delegate* unmanaged<ulong, float*, void> CharControllerGetRotation;
    public delegate* unmanaged<ulong, float*, void> CharControllerSetRotation;
    public delegate* unmanaged<ulong, float*, void> CharControllerRotate;
}

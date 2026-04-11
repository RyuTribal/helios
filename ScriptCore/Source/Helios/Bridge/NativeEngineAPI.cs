using System.Runtime.InteropServices;

namespace Helios.Bridge;

/// <summary>
/// Function pointers received from C++ at initialization.
/// Layout must exactly match the C++ NativeEngineAPI struct.
/// Every callback receives world_context (void*) as its first argument.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeEngineAPI
{
    // World context (opaque pointer passed back on every call)
    public void* WorldContext;

    // Logging
    public delegate* unmanaged<void*, int, byte*, void> Log;

    // Entity
    public delegate* unmanaged<void*, ulong> Spawn;
    public delegate* unmanaged<void*, ulong, void> Despawn;
    public delegate* unmanaged<void*, ulong, byte> IsAlive;

    // Component (generic)
    public delegate* unmanaged<void*, ulong, byte*, byte> HasComponent;

    // Transform shortcuts
    public delegate* unmanaged<void*, ulong, float*, void> TransformGetTranslation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformSetTranslation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformGetRotation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformSetRotation;
    public delegate* unmanaged<void*, ulong, float*, void> TransformGetScale;
    public delegate* unmanaged<void*, ulong, float*, void> TransformSetScale;

    // Input
    public delegate* unmanaged<void*, int, byte> IsKeyPressed;
    public delegate* unmanaged<void*, int, byte> IsMouseButtonPressed;
    public delegate* unmanaged<void*, float*, float*, void> GetMousePosition;

    // Physics
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsApplyForce;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsApplyImpulse;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsApplyTorque;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsSetLinearVelocity;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsGetLinearVelocity;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsSetAngularVelocity;
    public delegate* unmanaged<void*, ulong, float*, void> PhysicsGetAngularVelocity;

    // Mouse delta / scroll
    public delegate* unmanaged<void*, float*, float*, void> GetMouseDelta;
    public delegate* unmanaged<void*, float> GetScrollDelta;

    // Cursor mode (0 = normal, 1 = captured)
    public delegate* unmanaged<void*, int, void> SetCursorMode;
    public delegate* unmanaged<void*, int> GetCursorMode;

    // Physics teleport (set_transform + zero velocity)
    public delegate* unmanaged<void*, ulong, float*, float*, void> PhysicsTeleport;

    // Tag name lookup
    public delegate* unmanaged<void*, ulong, byte*, int, void> GetTagName;

    // Time
    public delegate* unmanaged<void*, float> GetDeltaTime;

    // Input: single-press detection
    public delegate* unmanaged<void*, int, byte> IsKeyJustPressed;

    // Transform: look-at (computes rotation to face target)
    public delegate* unmanaged<void*, ulong, float*, float*, void> TransformLookAt;

    // Assets
    public delegate* unmanaged<void*, byte*, ulong> AssetLoad;
    public delegate* unmanaged<void*, ulong, byte> AssetIsLoaded;

    // Audio
    public delegate* unmanaged<void*, byte*, uint> AudioGetSoundId;
    public delegate* unmanaged<void*, uint, float, float, float, void> AudioPlaySoundAt;
    public delegate* unmanaged<void*, byte*, float, float, float, float, byte, void> AudioPlayFile;
    public delegate* unmanaged<void*, ulong, float, float, float, float, byte, void> AudioPlayHandle;

    // Scene
    public delegate* unmanaged<void*, byte*, void> SceneLoad;
    public delegate* unmanaged<void*, byte*, void> SceneInstantiate;
    public delegate* unmanaged<void*, byte*, byte> SceneIsReady;
}

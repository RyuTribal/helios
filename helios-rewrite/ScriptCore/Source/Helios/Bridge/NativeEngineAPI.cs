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
}

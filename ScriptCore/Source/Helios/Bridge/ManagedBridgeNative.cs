using System.Runtime.InteropServices;

namespace Helios.Bridge;

/// <summary>
/// Function pointers that C++ receives from C# during initialization.
/// Layout must exactly match the C++ ManagedBridge struct.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ManagedBridgeNative
{
    // Assembly management
    public delegate* unmanaged<byte*, void> LoadAppAssembly;
    public delegate* unmanaged<void> UnloadAppAssembly;

    // Class discovery
    public delegate* unmanaged<int> GetEntityClassCount;
    public delegate* unmanaged<int, byte*, int, void> GetEntityClassName;
    public delegate* unmanaged<byte*, byte> EntityClassExists;
    public delegate* unmanaged<byte*, uint> GetScriptTypeId;

    // Instance lifecycle
    public delegate* unmanaged<byte*, ulong, ulong> CreateInstance;
    public delegate* unmanaged<ulong, void> InvokeOnCreate;
    public delegate* unmanaged<uint, ulong*, ulong*, int, float, void> InvokeOnUpdateBatch;
    public delegate* unmanaged<ulong, ulong, void> InvokeOnDestroy;
    public delegate* unmanaged<void> DestroyAllInstances;

    // Method invocation (generic)
    public delegate* unmanaged<ulong, byte*, void*, int, void> InvokeMethod;

    // Collision callbacks
    public delegate* unmanaged<ulong, ulong, float, float, float, float, float, float, float, void> InvokeOnCollision;
}

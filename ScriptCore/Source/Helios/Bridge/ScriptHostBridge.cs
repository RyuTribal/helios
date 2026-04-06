using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Helios.Bridge;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct ManagedBridgeNative
{
    public delegate* unmanaged<byte*, void> LoadAppAssembly;
    public delegate* unmanaged<void> UnloadAppAssembly;
    public delegate* unmanaged<int> GetEntityClassCount;
    public delegate* unmanaged<int, byte*, int, void> GetEntityClassName;
    public delegate* unmanaged<byte*, bool> EntityClassExists;
    public delegate* unmanaged<byte*, ulong, bool> CreateInstance;
    public delegate* unmanaged<ulong, void> DestroyInstance;
    public delegate* unmanaged<ulong, void> InvokeOnCreate;
    public delegate* unmanaged<ulong, float, void> InvokeOnUpdate;
    public delegate* unmanaged<void> DestroyAllInstances;
    public delegate* unmanaged<ulong, byte*, bool> EntityHasComponent;
}

public static unsafe class ScriptHostBridge
{
    private static ScriptAssemblyLoadContext? s_ScriptALC;
    private static Assembly? s_AppAssembly;
    private static List<Type> s_EntityClasses = new();
    private static Dictionary<ulong, Entity> s_Instances = new();
    private static Dictionary<Type, MethodInfo?> s_OnCreateCache = new();
    private static Dictionary<Type, MethodInfo?> s_OnUpdateCache = new();

    [UnmanagedCallersOnly]
    public static void Initialize(NativeEngineAPI* nativeApi, ManagedBridgeNative* outBridge)
    {
        NativeAPI.Init(nativeApi);

        outBridge->LoadAppAssembly = &BridgeLoadAppAssembly;
        outBridge->UnloadAppAssembly = &BridgeUnloadAppAssembly;
        outBridge->GetEntityClassCount = &BridgeGetEntityClassCount;
        outBridge->GetEntityClassName = &BridgeGetEntityClassName;
        outBridge->EntityClassExists = &BridgeEntityClassExists;
        outBridge->CreateInstance = &BridgeCreateInstance;
        outBridge->DestroyInstance = &BridgeDestroyInstance;
        outBridge->InvokeOnCreate = &BridgeInvokeOnCreate;
        outBridge->InvokeOnUpdate = &BridgeInvokeOnUpdate;
        outBridge->DestroyAllInstances = &BridgeDestroyAllInstances;
        outBridge->EntityHasComponent = &BridgeEntityHasComponent;
    }

    // ── Helper: UTF-8 byte* to string ──────────────────────────

    private static string PtrToString(byte* ptr)
    {
        if (ptr == null) return string.Empty;
        int len = 0;
        while (ptr[len] != 0) len++;
        return Encoding.UTF8.GetString(ptr, len);
    }

    // ── Bridge methods ─────────────────────────────────────────

    [UnmanagedCallersOnly]
    private static void BridgeLoadAppAssembly(byte* pathPtr)
    {
        string path = PtrToString(pathPtr);

        // Unload previous if any
        if (s_ScriptALC != null)
        {
            s_Instances.Clear();
            s_OnCreateCache.Clear();
            s_OnUpdateCache.Clear();
            s_EntityClasses.Clear();
            s_AppAssembly = null;
            s_ScriptALC.Unload();
        }

        s_ScriptALC = new ScriptAssemblyLoadContext();

        // Load from stream to avoid file locking
        using var stream = File.OpenRead(path);
        s_AppAssembly = s_ScriptALC.LoadFromStream(stream);

        // Scan for Entity subclasses
        s_EntityClasses.Clear();
        foreach (var type in s_AppAssembly.GetTypes())
        {
            if (type.IsSubclassOf(typeof(Entity)) && !type.IsAbstract)
                s_EntityClasses.Add(type);
        }
    }

    [UnmanagedCallersOnly]
    private static void BridgeUnloadAppAssembly()
    {
        s_Instances.Clear();
        s_OnCreateCache.Clear();
        s_OnUpdateCache.Clear();
        s_EntityClasses.Clear();
        s_AppAssembly = null;

        if (s_ScriptALC != null)
        {
            s_ScriptALC.Unload();
            s_ScriptALC = null;
        }
    }

    [UnmanagedCallersOnly]
    private static int BridgeGetEntityClassCount()
    {
        return s_EntityClasses.Count;
    }

    [UnmanagedCallersOnly]
    private static void BridgeGetEntityClassName(int index, byte* buffer, int bufferSize)
    {
        if (index < 0 || index >= s_EntityClasses.Count) return;

        string name = s_EntityClasses[index].FullName ?? s_EntityClasses[index].Name;
        int byteCount = Encoding.UTF8.GetBytes(name, new Span<byte>(buffer, bufferSize - 1));
        buffer[byteCount] = 0; // null terminator
    }

    [UnmanagedCallersOnly]
    private static bool BridgeEntityClassExists(byte* fullNamePtr)
    {
        string fullName = PtrToString(fullNamePtr);
        foreach (var type in s_EntityClasses)
        {
            if (type.FullName == fullName || type.Name == fullName)
                return true;
        }
        return false;
    }

    [UnmanagedCallersOnly]
    private static bool BridgeCreateInstance(byte* classNamePtr, ulong entityId)
    {
        string className = PtrToString(classNamePtr);

        Type? targetType = null;
        foreach (var type in s_EntityClasses)
        {
            if (type.FullName == className || type.Name == className)
            {
                targetType = type;
                break;
            }
        }

        if (targetType == null) return false;

        var instance = (Entity?)Activator.CreateInstance(targetType);
        if (instance == null) return false;

        // Set the entity ID (public mutable field)
        instance.ID = entityId;

        s_Instances[entityId] = instance;
        return true;
    }

    [UnmanagedCallersOnly]
    private static void BridgeDestroyInstance(ulong entityId)
    {
        s_Instances.Remove(entityId);
    }

    [UnmanagedCallersOnly]
    private static void BridgeInvokeOnCreate(ulong entityId)
    {
        if (!s_Instances.TryGetValue(entityId, out var instance)) return;

        var type = instance.GetType();
        if (!s_OnCreateCache.TryGetValue(type, out var method))
        {
            method = type.GetMethod("OnCreate",
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            s_OnCreateCache[type] = method;
        }

        method?.Invoke(instance, null);
    }

    [UnmanagedCallersOnly]
    private static void BridgeInvokeOnUpdate(ulong entityId, float deltaTime)
    {
        if (!s_Instances.TryGetValue(entityId, out var instance)) return;

        var type = instance.GetType();
        if (!s_OnUpdateCache.TryGetValue(type, out var method))
        {
            method = type.GetMethod("OnUpdate",
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
                null, new[] { typeof(float) }, null);
            s_OnUpdateCache[type] = method;
        }

        method?.Invoke(instance, new object[] { deltaTime });
    }

    [UnmanagedCallersOnly]
    private static void BridgeDestroyAllInstances()
    {
        s_Instances.Clear();
    }

    [UnmanagedCallersOnly]
    private static bool BridgeEntityHasComponent(ulong entityId, byte* componentNamePtr)
    {
        string componentName = PtrToString(componentNamePtr);
        return NativeAPI.EntityHasComponent(entityId, componentName);
    }
}

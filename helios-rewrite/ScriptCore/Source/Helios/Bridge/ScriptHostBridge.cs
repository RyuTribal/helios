using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Helios.Bridge;

public static unsafe class ScriptHostBridge
{
    private static ScriptAssemblyLoadContext? s_ScriptALC;
    private static Assembly? s_AppAssembly;
    private static List<Type> s_ScriptClasses = new();
    private static Dictionary<ulong, ScriptBehaviour> s_Instances = new();

    [UnmanagedCallersOnly]
    public static void Initialize(NativeEngineAPI* nativeApi, ManagedBridgeNative* outBridge)
    {
        NativeAPI.Init(nativeApi);

        outBridge->LoadAppAssembly = &BridgeLoadAppAssembly;
        outBridge->UnloadAppAssembly = &BridgeUnloadAppAssembly;
        outBridge->GetEntityClassCount = &BridgeGetEntityClassCount;
        outBridge->GetEntityClassName = &BridgeGetEntityClassName;
        outBridge->EntityClassExists = &BridgeEntityClassExists;
        outBridge->GetScriptTypeId = &BridgeGetScriptTypeId;
        outBridge->CreateInstance = &BridgeCreateInstance;
        outBridge->InvokeOnCreate = &BridgeInvokeOnCreate;
        outBridge->InvokeOnUpdateBatch = &BridgeInvokeOnUpdateBatch;
        outBridge->InvokeOnDestroy = &BridgeInvokeOnDestroy;
        outBridge->DestroyAllInstances = &BridgeDestroyAllInstances;
        outBridge->InvokeMethod = &BridgeInvokeMethod;
        outBridge->InvokeOnCollision = &BridgeInvokeOnCollision;
    }

    // -- Helper: UTF-8 byte* to string ----------------------------------------

    private static string PtrToString(byte* ptr)
    {
        if (ptr == null) return string.Empty;
        int len = 0;
        while (ptr[len] != 0) len++;
        return Encoding.UTF8.GetString(ptr, len);
    }

    // -- Bridge methods -------------------------------------------------------

    [UnmanagedCallersOnly]
    private static void BridgeLoadAppAssembly(byte* pathPtr)
    {
        string path = PtrToString(pathPtr);

        if (!File.Exists(path))
        {
            Console.Error.WriteLine($"[ScriptBridge] App assembly not found: {path}");
            return;
        }

        // Unload previous if any
        if (s_ScriptALC != null)
        {
            s_Instances.Clear();
            s_ScriptClasses.Clear();
            s_AppAssembly = null;
            s_ScriptALC.Unload();
        }

        s_ScriptALC = new ScriptAssemblyLoadContext();

        // Load from stream to avoid file locking
        using var stream = File.OpenRead(path);
        s_AppAssembly = s_ScriptALC.LoadFromStream(stream);

        // Scan for ScriptBehaviour subclasses
        s_ScriptClasses.Clear();
        Type[] types;
        try
        {
            types = s_AppAssembly.GetTypes();
        }
        catch (ReflectionTypeLoadException ex)
        {
            types = ex.Types.Where(t => t != null).ToArray()!;
            foreach (var err in ex.LoaderExceptions)
                Console.Error.WriteLine($"[ScriptBridge] Type load warning: {err?.Message}");
        }

        foreach (var type in types)
        {
            if (type.IsSubclassOf(typeof(ScriptBehaviour)) && !type.IsAbstract)
                s_ScriptClasses.Add(type);
        }
    }

    [UnmanagedCallersOnly]
    private static void BridgeUnloadAppAssembly()
    {
        s_Instances.Clear();
        s_ScriptClasses.Clear();
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
        return s_ScriptClasses.Count;
    }

    [UnmanagedCallersOnly]
    private static void BridgeGetEntityClassName(int index, byte* buffer, int bufferSize)
    {
        if (index < 0 || index >= s_ScriptClasses.Count) return;

        string name = s_ScriptClasses[index].FullName ?? s_ScriptClasses[index].Name;
        int byteCount = Encoding.UTF8.GetBytes(name, new Span<byte>(buffer, bufferSize - 1));
        buffer[byteCount] = 0; // null terminator
    }

    [UnmanagedCallersOnly]
    private static byte BridgeEntityClassExists(byte* fullNamePtr)
    {
        string fullName = PtrToString(fullNamePtr);
        foreach (var type in s_ScriptClasses)
        {
            if (type.FullName == fullName || type.Name == fullName)
                return 1;
        }
        return 0;
    }

    [UnmanagedCallersOnly]
    private static uint BridgeGetScriptTypeId(byte* fullNamePtr)
    {
        string fullName = PtrToString(fullNamePtr);
        // FNV-1a hash
        uint hash = 2166136261u;
        foreach (char c in fullName)
        {
            hash ^= c;
            hash *= 16777619u;
        }
        return hash;
    }

    [UnmanagedCallersOnly]
    private static ulong BridgeCreateInstance(byte* classNamePtr, ulong entityId)
    {
        string className = PtrToString(classNamePtr);

        Type? targetType = null;
        foreach (var type in s_ScriptClasses)
        {
            if (type.FullName == className || type.Name == className)
            {
                targetType = type;
                break;
            }
        }

        if (targetType == null) return 0;

        var instance = (ScriptBehaviour?)Activator.CreateInstance(targetType);
        if (instance == null) return 0;

        // Set the entity ID
        instance.EntityId = entityId;

        // Store and return handle (we use entityId as the key)
        s_Instances[entityId] = instance;

        // Return a non-zero handle (entityId itself works as the opaque handle)
        return entityId;
    }

    [UnmanagedCallersOnly]
    private static void BridgeInvokeOnCreate(ulong entityId)
    {
        if (!s_Instances.TryGetValue(entityId, out var instance)) return;
        instance.OnCreate();
    }

    [UnmanagedCallersOnly]
    private static void BridgeInvokeOnUpdateBatch(
        uint scriptTypeId,
        ulong* entityIds,
        ulong* managedHandles,
        int count,
        float deltaTime)
    {
        for (int i = 0; i < count; i++)
        {
            ulong entityId = entityIds[i];
            if (s_Instances.TryGetValue(entityId, out var instance))
            {
                instance.OnUpdate(deltaTime);
            }
        }
    }

    [UnmanagedCallersOnly]
    private static void BridgeInvokeOnDestroy(ulong entityId, ulong managedHandle)
    {
        if (s_Instances.TryGetValue(entityId, out var instance))
        {
            instance.OnDestroy();
            s_Instances.Remove(entityId);
        }
    }

    [UnmanagedCallersOnly]
    private static void BridgeDestroyAllInstances()
    {
        s_Instances.Clear();
    }

    [UnmanagedCallersOnly]
    private static void BridgeInvokeMethod(
        ulong entityId, byte* methodNamePtr,
        void* args, int argsSizeBytes)
    {
        if (!s_Instances.TryGetValue(entityId, out var instance)) return;
        string methodName = PtrToString(methodNamePtr);

        // For collision exit, args is a uint64 (other entity ID)
        if (methodName == "OnCollisionExit" && argsSizeBytes >= 8)
            instance.OnCollisionExit(*(ulong*)args);
    }

    [UnmanagedCallersOnly]
    private static void BridgeInvokeOnCollision(
        ulong entityId, ulong otherEntityId,
        float px, float py, float pz,
        float nx, float ny, float nz,
        float impulse)
    {
        if (!s_Instances.TryGetValue(entityId, out var instance)) return;
        instance.OnCollisionEnter(
            otherEntityId,
            new Vector3(px, py, pz),
            new Vector3(nx, ny, nz),
            impulse);
    }
}

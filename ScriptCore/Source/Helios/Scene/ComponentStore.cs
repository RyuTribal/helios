using System;
using System.Collections.Generic;

namespace Helios;

/// <summary>
/// Managed-side storage for C#-defined components.
/// Engine components (TransformComponent, etc.) live in C++ ECS and are
/// accessed through the native bridge. User-defined components live here.
/// </summary>
internal static class ComponentStore
{
    // entityId → (componentType → component instance)
    private static readonly Dictionary<ulong, Dictionary<Type, Component>> s_Store = new();

    internal static void Add(ulong entityId, Component component)
    {
        if (!s_Store.TryGetValue(entityId, out var components))
        {
            components = new Dictionary<Type, Component>();
            s_Store[entityId] = components;
        }
        components[component.GetType()] = component;
    }

    internal static T? Get<T>(ulong entityId) where T : Component
    {
        if (s_Store.TryGetValue(entityId, out var components) &&
            components.TryGetValue(typeof(T), out var comp))
        {
            return (T)comp;
        }
        return null;
    }

    internal static bool Has<T>(ulong entityId) where T : Component
    {
        return s_Store.TryGetValue(entityId, out var components) &&
               components.ContainsKey(typeof(T));
    }

    internal static bool Remove<T>(ulong entityId) where T : Component
    {
        if (s_Store.TryGetValue(entityId, out var components))
        {
            return components.Remove(typeof(T));
        }
        return false;
    }

    /// <summary>Remove all components for a despawned entity.</summary>
    internal static void RemoveAll(ulong entityId)
    {
        s_Store.Remove(entityId);
    }
}

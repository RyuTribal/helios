using System;
using System.Numerics;

namespace Helios;

/// <summary>
/// Entity wrapper. Provides component access (both native C++ components
/// and managed C#-defined components), physics, and utility methods.
/// ScriptBehaviour extends this so user scripts inherit everything.
/// </summary>
public class Entity
{
    public Entity() { ID = 0; }
    public Entity(ulong id) { ID = id; }

    public ulong ID;

    public void Destroy() => NativeAPI.Despawn(ID);

    // -- Component access -----------------------------------------------------

    /// <summary>
    /// Check if the entity has a component. Checks managed store first,
    /// then native bridge for engine components.
    /// </summary>
    public bool HasComponent<T>() where T : Component, new()
    {
        if (ComponentStore.Has<T>(ID)) return true;
        return NativeAPI.HasComponent(ID, typeof(T).Name);
    }

    /// <summary>
    /// Get a component. Returns managed C# component if it exists,
    /// otherwise checks native bridge for engine components (TransformComponent, etc.).
    /// </summary>
    public T? GetComponent<T>() where T : Component, new()
    {
        // Check managed store first
        var managed = ComponentStore.Get<T>(ID);
        if (managed != null) return managed;

        // Fall back to native bridge (engine components)
        if (!NativeAPI.HasComponent(ID, typeof(T).Name)) return null;
        return new T() { Entity = this };
    }

    /// <summary>Add a C#-defined component to this entity.</summary>
    /// <exception cref="InvalidOperationException">
    /// Thrown if T is a native engine component (marked with [NativeComponent]).
    /// </exception>
    public T AddComponent<T>(T component) where T : Component
    {
        if (Attribute.IsDefined(typeof(T), typeof(NativeComponentAttribute)))
            throw new InvalidOperationException(
                $"Cannot add native engine component '{typeof(T).Name}' from C#. " +
                $"Native components are managed by the C++ ECS.");

        component.Entity = this;
        ComponentStore.Add(ID, component);
        return component;
    }

    /// <summary>Remove a C#-defined component from this entity.</summary>
    public bool RemoveComponent<T>() where T : Component
    {
        return ComponentStore.Remove<T>(ID);
    }

    public bool IsAlive() => NativeAPI.IsAlive(ID);

    // -- Physics --------------------------------------------------------------

    public void ApplyForce(Vector3 force) => NativeAPI.PhysicsApplyForce(ID, force);
    public void ApplyImpulse(Vector3 impulse) => NativeAPI.PhysicsApplyImpulse(ID, impulse);
    public void ApplyTorque(Vector3 torque) => NativeAPI.PhysicsApplyTorque(ID, torque);

    public Vector3 LinearVelocity
    {
        get => NativeAPI.PhysicsGetLinearVelocity(ID);
        set => NativeAPI.PhysicsSetLinearVelocity(ID, value);
    }

    public Vector3 AngularVelocity
    {
        get => NativeAPI.PhysicsGetAngularVelocity(ID);
        set => NativeAPI.PhysicsSetAngularVelocity(ID, value);
    }

    /// <summary>
    /// Teleport the physics body to a new position and rotation, zeroing velocity.
    /// rotation is a quaternion as (x, y, z, w).
    /// </summary>
    public void PhysicsTeleport(Vector3 position, Vector4 rotation)
        => NativeAPI.PhysicsTeleport(ID, position, rotation);

    /// <summary>The Tag component's name string, or empty if no Tag.</summary>
    public string TagName => NativeAPI.GetTagName(ID);

    /// <summary>
    /// Set the entity's rotation so it faces the target position.
    /// Uses glm::lookAt internally — matches the C++ camera convention.
    /// </summary>
    public void LookAt(Vector3 target, Vector3 up)
        => NativeAPI.TransformLookAt(ID, target, up);

    /// <summary>Seconds since last frame (convenience shortcut).</summary>
    public static float DeltaTime => NativeAPI.GetDeltaTime();
}

using System;
using System.Numerics;

namespace Helios;

/// <summary>
/// Lightweight entity wrapper. Provides component access via NativeEngineAPI.
/// This is NOT the base class for user scripts (that is ScriptBehaviour).
/// </summary>
public class Entity
{
    public Entity() { ID = 0; }
    public Entity(ulong id) { ID = id; }

    public ulong ID;

    public void Destroy() => NativeAPI.Despawn(ID);

    public bool HasComponent<T>() where T : Component, new()
        => NativeAPI.HasComponent(ID, typeof(T).Name);

    public T? GetComponent<T>() where T : Component, new()
    {
        if (!HasComponent<T>()) return null;
        return new T() { Entity = this };
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
}

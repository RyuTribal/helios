using System;
using System.Numerics;
using Helios;

namespace SandboxScripts;

/// <summary>
/// Simple script that rotates the helmet entity each frame using physics torque.
/// Demonstrates the ScriptBehaviour lifecycle, physics API, and collision callbacks.
/// </summary>
public class HelmetRotator : ScriptBehaviour
{
    private float _totalTime;
    private float _rotationSpeed = 1.0f; // radians per second

    public override void OnCreate()
    {
        Log.Info($"HelmetRotator created on entity {EntityId}");
    }

    public override void OnUpdate(float delta)
    {
        _totalTime += delta;

        // Apply torque around Y axis for physics-driven rotation
        Entity.ApplyTorque(new Vector3(0, _rotationSpeed, 0));
    }

    public override void OnDestroy()
    {
        Log.Info($"HelmetRotator destroyed on entity {EntityId}");
    }

    public override void OnCollisionEnter(ulong otherEntityId, Vector3 point, Vector3 normal, float impulse)
    {
        Log.Info($"Helmet collided with entity {otherEntityId} at {point}, impulse={impulse}");
    }
}

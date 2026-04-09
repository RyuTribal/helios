using System;
using System.Numerics;
using Helios;

namespace SandboxScripts;

/// <summary>
/// Simple script that rotates the helmet entity each frame.
/// Demonstrates the ScriptBehaviour lifecycle and Transform access.
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

        var transform = Entity.GetComponent<TransformComponent>();
        if (transform == null) return;

        // Rotate around Y axis
        var rotation = transform.Rotation;
        rotation.Y += _rotationSpeed * delta;
        transform.Rotation = rotation;
    }

    public override void OnDestroy()
    {
        Log.Info($"HelmetRotator destroyed on entity {EntityId}");
    }
}

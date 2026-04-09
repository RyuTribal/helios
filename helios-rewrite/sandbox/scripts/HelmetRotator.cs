using System;
using System.Numerics;
using Helios;

namespace SandboxScripts;

/// <summary>
/// Helmet script: constant Y-axis rotation via physics angular velocity,
/// plus R (teleport reset) and E (upward force) controls.
/// Also logs collision events.
/// </summary>
public class HelmetRotator : ScriptBehaviour
{
    private float _totalTime;
    private float _rotationSpeed = 1.0f; // radians per second

    // Reset pose: position and rotation (quaternion as xyzw)
    private static readonly Vector3 ResetPosition = new(0f, 3f, 0f);
    private static readonly Vector4 ResetRotation; // computed in static ctor

    static HelmetRotator()
    {
        // glm::quat(glm::vec3(radians(90), radians(180), 0))
        // Euler XYZ -> quaternion
        float rx = MathF.PI / 2f;   // 90 degrees
        float ry = MathF.PI;        // 180 degrees
        float rz = 0f;
        var q = Quaternion.CreateFromYawPitchRoll(ry, rx, rz);
        ResetRotation = new Vector4(q.X, q.Y, q.Z, q.W);
    }

    public override void OnCreate()
    {
        Log.Info($"HelmetRotator created on entity {EntityId}");
    }

    public override void OnUpdate(float delta)
    {
        _totalTime += delta;

        // Constant Y-axis spin
        Entity.AngularVelocity = new Vector3(0, _rotationSpeed, 0);

        // R: teleport helmet back to start position (single press)
        if (Input.IsKeyJustPressed(82)) // KeyCode.R = 82
        {
            Entity.PhysicsTeleport(ResetPosition, ResetRotation);
        }

        // E: apply upward force (held)
        if (Input.IsKeyPressed(69)) // KeyCode.E = 69
        {
            Entity.ApplyForce(new Vector3(0, 30, 0));
        }
    }

    public override void OnDestroy()
    {
        Log.Info($"HelmetRotator destroyed on entity {EntityId}");
    }

    public override void OnCollisionEnter(ulong otherEntityId, Vector3 point, Vector3 normal, float impulse)
    {
        Log.Info($"Helmet collided with entity {otherEntityId} at {point}, impulse={impulse:F2}");
    }
}

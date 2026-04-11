using System;
using System.Numerics;
using Helios;

namespace SandboxScripts;

/// <summary>
/// Helmet script: constant Y-axis rotation via physics angular velocity,
/// plus R (teleport reset) and E (upward force) controls.
/// Plays bounce sound on collision.
/// </summary>
public class HelmetRotator : ScriptBehaviour
{
    private float _totalTime;
    private float _rotationSpeed = 1.0f;
    private uint _bounceSoundId;

    private static readonly Vector3 ResetPosition = new(0f, 3f, 0f);
    private static readonly Vector4 ResetRotation;

    static HelmetRotator()
    {
        float rx = MathF.PI / 2f;
        float ry = MathF.PI;
        float rz = 0f;
        var q = Quaternion.CreateFromYawPitchRoll(ry, rx, rz);
        ResetRotation = new Vector4(q.X, q.Y, q.Z, q.W);
    }

    public override void OnCreate()
    {
        Log.Info($"HelmetRotator created on entity {ID}");
        _bounceSoundId = Audio.GetSoundId("bounce");
    }

    public override void OnUpdate(float delta)
    {
        _totalTime += delta;

        AngularVelocity = new Vector3(0, _rotationSpeed, 0);

        if (IsKeyJustPressed(82)) // R
        {
            PhysicsTeleport(ResetPosition, ResetRotation);
        }

        if (IsKeyPressed(69)) // E
        {
            ApplyForce(new Vector3(0, 30, 0));
        }
    }

    public override void OnDestroy()
    {
        Log.Info($"HelmetRotator destroyed on entity {ID}");
    }

    public override void OnCollisionEnter(ulong otherEntityId, Vector3 point, Vector3 normal, float impulse)
    {
        if (_bounceSoundId != 0)
            Audio.PlayAt(_bounceSoundId, point);
    }
}

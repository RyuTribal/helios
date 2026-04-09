using System;
using System.Numerics;
using Helios;

namespace SandboxScripts;

/// <summary>
/// Side camera that mirrors the OrbitCamera but offset 90 degrees around Y.
/// Attach to the side camera entity.
/// </summary>
public class SideCamera : ScriptBehaviour
{
    // Shared orbit state (duplicated from OrbitCamera — in a real game
    // you'd use a shared resource or ECS query, not duplicate state)
    private float _yaw;
    private float _pitch;
    private float _distance = 3.0f;
    private float _sensitivity = 0.003f;
    private float _zoomSpeed = 0.3f;
    private bool _panning;

    private static readonly float MaxPitch = MathF.PI / 2f - 0.017f;
    private static readonly float YawOffset = MathF.PI / 2f; // 90 degrees

    public override void OnUpdate(float delta)
    {
        bool rmb = Input.IsMouseButtonPressed(1);

        if (rmb && !_panning) _panning = true;
        if (!rmb && _panning) _panning = false;

        if (_panning)
        {
            var mouseDelta = Input.MouseDelta;
            _yaw += mouseDelta.X * _sensitivity;
            _pitch -= mouseDelta.Y * _sensitivity;
            _pitch = Math.Clamp(_pitch, -MaxPitch, MaxPitch);
        }

        float scroll = Math.Clamp(Input.ScrollDelta, -1f, 1f);
        if (scroll != 0f)
        {
            _distance *= 1f - scroll * _zoomSpeed;
            _distance = Math.Clamp(_distance, 0.2f, 30f);
        }

        // Same spherical coords as main camera but with 90° yaw offset
        float offsetYaw = _yaw + YawOffset;
        float cosP = MathF.Cos(_pitch);
        var pos = new Vector3(
            _distance * cosP * MathF.Sin(offsetYaw),
            _distance * MathF.Sin(_pitch),
            _distance * cosP * MathF.Cos(offsetYaw)
        );

        var transform = Entity.GetComponent<TransformComponent>();
        if (transform != null)
        {
            transform.Translation = pos;
            transform.Rotation = new Vector3(-_pitch, MathF.PI + offsetYaw, 0f);
        }
    }
}

using System;
using System.Numerics;
using Helios;

namespace SandboxScripts;

/// <summary>
/// Orbit camera controlled by right-mouse-button drag and scroll wheel.
/// Attach to the main camera entity. Computes position from spherical
/// coordinates around the origin and sets a look-at rotation.
/// </summary>
public class OrbitCamera : ScriptBehaviour
{
    private float _yaw;
    private float _pitch;
    private float _distance = 3.0f;
    private float _sensitivity = 0.003f;
    private float _zoomSpeed = 0.3f;
    private bool _panning;

    private static readonly float MaxPitch = MathF.PI / 2f - 0.017f; // ~89 degrees

    public override void OnCreate()
    {
        Log.Info($"OrbitCamera created on entity {ID}");
    }

    public override void OnUpdate(float delta)
    {
        bool rmb = IsMouseButtonPressed(1);

        if (rmb && !_panning)
        {
            _panning = true;
            SetCursorMode(1);
        }
        if (!rmb && _panning)
        {
            _panning = false;
            SetCursorMode(0);
        }

        if (_panning)
        {
            var mouseDelta = GetMouseDelta();
            _yaw += mouseDelta.X * _sensitivity;
            _pitch -= mouseDelta.Y * _sensitivity;
            _pitch = Math.Clamp(_pitch, -MaxPitch, MaxPitch);
        }

        float scroll = Math.Clamp(GetScrollDelta(), -1f, 1f);
        if (scroll != 0f)
        {
            _distance *= 1f - scroll * _zoomSpeed;
            _distance = Math.Clamp(_distance, 0.2f, 30f);
        }

        float cosP = MathF.Cos(_pitch);
        var pos = new Vector3(
            _distance * cosP * MathF.Sin(_yaw),
            _distance * MathF.Sin(_pitch),
            _distance * cosP * MathF.Cos(_yaw)
        );

        var transform = GetComponent<TransformComponent>();
        if (transform != null)
        {
            transform.Translation = pos;
            LookAt(Vector3.Zero, Vector3.UnitY);
        }
    }

    public override void OnDestroy()
    {
        Log.Info($"OrbitCamera destroyed on entity {ID}");
    }
}

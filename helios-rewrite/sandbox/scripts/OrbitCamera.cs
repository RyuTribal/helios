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
        Log.Info($"OrbitCamera created on entity {EntityId}");
    }

    public override void OnUpdate(float delta)
    {
        bool rmb = Input.IsMouseButtonPressed(1); // right button

        // Start panning
        if (rmb && !_panning)
        {
            _panning = true;
            Input.SetCursorMode(1); // captured
        }
        // Stop panning
        if (!rmb && _panning)
        {
            _panning = false;
            Input.SetCursorMode(0); // normal
        }

        // Apply mouse delta while panning
        if (_panning)
        {
            var mouseDelta = Input.MouseDelta;
            _yaw += mouseDelta.X * _sensitivity;
            _pitch -= mouseDelta.Y * _sensitivity;
            _pitch = Math.Clamp(_pitch, -MaxPitch, MaxPitch);
        }

        // Scroll zoom -- proportional, clamped per tick
        float scroll = Math.Clamp(Input.ScrollDelta, -1f, 1f);
        if (scroll != 0f)
        {
            _distance *= 1f - scroll * _zoomSpeed;
            _distance = Math.Clamp(_distance, 0.2f, 30f);
        }

        // Spherical to Cartesian
        float cosP = MathF.Cos(_pitch);
        var pos = new Vector3(
            _distance * cosP * MathF.Sin(_yaw),
            _distance * MathF.Sin(_pitch),
            _distance * cosP * MathF.Cos(_yaw)
        );

        // Apply to transform
        var transform = Entity.GetComponent<TransformComponent>();
        if (transform != null)
        {
            transform.Translation = pos;

            // Look-at rotation matching the old C++ code:
            // glm::lookAt(pos, target, up) → quat_cast → conjugate
            // The conjugate of the lookAt quaternion gives the Transform rotation
            // that makes the camera face the target.
            //
            // We use spherical yaw/pitch directly since our camera IS the
            // spherical coordinate system — yaw rotates around Y, pitch around X.
            // The camera should face -Z in local space (OpenGL convention),
            // so we rotate by (pi + yaw) around Y, then pitch around X.
            transform.Rotation = new Vector3(-_pitch, MathF.PI + _yaw, 0f);
        }
    }

    public override void OnDestroy()
    {
        Log.Info($"OrbitCamera destroyed on entity {EntityId}");
    }
}

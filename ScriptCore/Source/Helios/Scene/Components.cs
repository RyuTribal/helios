using System;
using System.Numerics;

namespace Helios;

public abstract class Component
{
    public Entity Entity { get; internal set; }
}

public class TransformComponent : Component
{
    public Vector3 Translation
    {
        get => NativeAPI.TransformGetTranslation(Entity.ID);
        set => NativeAPI.TransformSetTranslation(Entity.ID, value);
    }

    public Vector3 Rotation
    {
        get => NativeAPI.TransformGetRotation(Entity.ID);
        set => NativeAPI.TransformSetRotation(Entity.ID, value);
    }

    public Vector3 Scale
    {
        get => NativeAPI.TransformGetScale(Entity.ID);
        set => NativeAPI.TransformSetScale(Entity.ID, value);
    }
}

public class CameraComponent : Component
{
    // Use this for 3:rd person
    public void RotateAroundEntity(Vector2 rotation, float speed, bool inverse_controls)
    {
        NativeAPI.CameraRotateAroundEntity(Entity.ID, rotation, speed, inverse_controls);
    }

    // Use this for 1:st person
    public void Rotate(Vector2 rotation, float speed, bool inverse_controls)
    {
        NativeAPI.CameraRotate(Entity.ID, rotation, speed, inverse_controls);
    }

    public Vector3 GetForwardDirection()
    {
        return NativeAPI.CameraGetForwardDirection(Entity.ID);
    }

    public Vector3 GetUpRightForwardDirection()
    {
        Vector3 direction = NativeAPI.CameraGetForwardDirection(Entity.ID);
        direction.Y = 0;
        direction = Vector3.Normalize(direction);
        return direction;
    }

    public Vector3 GetUpRightRightDirection()
    {
        Vector3 direction = NativeAPI.CameraGetRightDirection(Entity.ID);
        direction.Y = 0;
        direction = Vector3.Normalize(direction);
        return direction;
    }

    public Vector3 GetRightDirection()
    {
        return NativeAPI.CameraGetRightDirection(Entity.ID);
    }

    public Vector3 Position
    {
        get => NativeAPI.CameraGetPosition(Entity.ID);
        set => NativeAPI.CameraSetPosition(Entity.ID, value);
    }

    public Vector3 Rotation
    {
        get => NativeAPI.CameraGetRotation(Entity.ID);
        set => NativeAPI.CameraSetRotation(Entity.ID, value);
    }
}

public class BoxColliderComponent : Component
{
    public Vector3 LinearVelocity
    {
        get => NativeAPI.BoxColliderGetLinearVelocity(Entity.ID);
        set => NativeAPI.BoxColliderSetLinearVelocity(Entity.ID, value);
    }

    public void AddLinearVelocity(Vector3 velocity)
    {
        NativeAPI.BoxColliderAddLinearVelocity(Entity.ID, velocity);
    }

    public void AddAngularVelocity(Vector3 velocity)
    {
        NativeAPI.BoxColliderAddAngularVelocity(Entity.ID, velocity);
    }

    public void AddImpulse(Vector3 impulse)
    {
        NativeAPI.BoxColliderAddImpulse(Entity.ID, impulse);
    }

    public void AddAngularImpulse(Vector3 impulse)
    {
        NativeAPI.BoxColliderAddAngularImpulse(Entity.ID, impulse);
    }

    public void AddLinearAndAngularImpulse(Vector3 linear_impulse, Vector3 angular_impulse)
    {
        NativeAPI.BoxColliderAddLinearAngularImpulse(Entity.ID, linear_impulse, angular_impulse);
    }
}

public class GlobalSoundsComponent : Component
{
    public void PlaySoundAtIndex(int index)
    {
        NativeAPI.SoundsPlayGlobal(Entity.ID, index);
    }
}

public class LocalSoundsComponent : Component
{
    public void PlaySoundAtIndex(int index)
    {
        NativeAPI.SoundsPlayLocal(Entity.ID, index);
    }
}

public class SphereColliderComponent : Component
{
    public Vector3 LinearVelocity
    {
        get => NativeAPI.SphereColliderGetLinearVelocity(Entity.ID);
        set => NativeAPI.SphereColliderSetLinearVelocity(Entity.ID, value);
    }

    public void AddLinearVelocity(Vector3 velocity)
    {
        NativeAPI.SphereColliderAddLinearVelocity(Entity.ID, velocity);
    }

    public void AddAngularVelocity(Vector3 velocity)
    {
        NativeAPI.SphereColliderAddAngularVelocity(Entity.ID, velocity);
    }

    public void AddImpulse(Vector3 impulse)
    {
        NativeAPI.SphereColliderAddImpulse(Entity.ID, impulse);
    }

    public void AddAngularImpulse(Vector3 impulse)
    {
        NativeAPI.SphereColliderAddAngularImpulse(Entity.ID, impulse);
    }

    public void AddLinearAndAngularImpulse(Vector3 linear_impulse, Vector3 angular_impulse)
    {
        NativeAPI.SphereColliderAddLinearAngularImpulse(Entity.ID, linear_impulse, angular_impulse);
    }
}

public class CharacterControllerComponent : Component
{
    public Vector3 LinearVelocity
    {
        get => NativeAPI.CharControllerGetLinearVelocity(Entity.ID);
        set => NativeAPI.CharControllerSetLinearVelocity(Entity.ID, value);
    }

    public void AddLinearVelocity(Vector3 velocity)
    {
        NativeAPI.CharControllerAddLinearVelocity(Entity.ID, velocity);
    }

    public void AddAngularVelocity(Vector3 velocity)
    {
        NativeAPI.CharControllerAddAngularVelocity(Entity.ID, velocity);
    }

    public void AddImpulse(Vector3 impulse)
    {
        NativeAPI.CharControllerAddImpulse(Entity.ID, impulse);
    }

    public void AddAngularImpulse(Vector3 impulse)
    {
        NativeAPI.CharControllerAddAngularImpulse(Entity.ID, impulse);
    }

    public void AddLinearAndAngularImpulse(Vector3 linear_impulse, Vector3 angular_impulse)
    {
        NativeAPI.CharControllerAddLinearAngularImpulse(Entity.ID, linear_impulse, angular_impulse);
    }

    public bool IsCharacterGrounded()
    {
        return NativeAPI.CharControllerIsGrounded(Entity.ID);
    }

    public Vector3 GetRotation()
    {
        return NativeAPI.CharControllerGetRotation(Entity.ID);
    }

    public Quaternion GetRotationQuaternion()
    {
        Vector3 rotation = GetRotation();

        // Convert degrees to radians if necessary
        float yaw = rotation.Y * (float)Math.PI / 180.0f;
        float pitch = rotation.X * (float)Math.PI / 180.0f;
        float roll = rotation.Z * (float)Math.PI / 180.0f;

        // Calculate trigonometric functions
        float cy = (float)Math.Cos(yaw * 0.5f);
        float sy = (float)Math.Sin(yaw * 0.5f);
        float cp = (float)Math.Cos(pitch * 0.5f);
        float sp = (float)Math.Sin(pitch * 0.5f);
        float cr = (float)Math.Cos(roll * 0.5f);
        float sr = (float)Math.Sin(roll * 0.5f);

        // Create quaternion
        Quaternion q = new Quaternion();
        q.W = cr * cp * cy + sr * sp * sy;
        q.X = sr * cp * cy - cr * sp * sy;
        q.Y = cr * sp * cy + sr * cp * sy;
        q.Z = cr * cp * sy - sr * sp * cy;

        return q;
    }

    public void SetRotation(Vector3 rotation)
    {
        NativeAPI.CharControllerSetRotation(Entity.ID, rotation);
    }

    public void Rotate(Vector3 rotation)
    {
        NativeAPI.CharControllerRotate(Entity.ID, rotation);
    }

    public void SetRotation(Quaternion rotation)
    {
        Vector3 euler;

        // Roll (X-axis rotation)
        double sinr_cosp = 2 * (rotation.W * rotation.X + rotation.Y * rotation.Z);
        double cosr_cosp = 1 - 2 * (rotation.X * rotation.X + rotation.Y * rotation.Y);
        euler.X = (float)Math.Atan2(sinr_cosp, cosr_cosp);

        // Pitch (Y-axis rotation)
        double sinp = 2 * (rotation.W * rotation.Y - rotation.Z * rotation.X);
        if (Math.Abs(sinp) >= 1)
            euler.Y = (float)Math.Abs(Math.PI / 2) * Math.Sign(sinp);
        else
            euler.Y = (float)Math.Asin(sinp);

        // Yaw (Z-axis rotation)
        double siny_cosp = 2 * (rotation.W * rotation.Z + rotation.X * rotation.Y);
        double cosy_cosp = 1 - 2 * (rotation.Y * rotation.Y + rotation.Z * rotation.Z);
        euler.Z = (float)Math.Atan2(siny_cosp, cosy_cosp);

        // Convert radians to degrees if your system uses degrees
        euler.X = euler.X * 180 / (float)Math.PI;
        euler.Y = euler.Y * 180 / (float)Math.PI;
        euler.Z = euler.Z * 180 / (float)Math.PI;

        SetRotation(euler);
    }
}

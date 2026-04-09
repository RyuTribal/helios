using System;
using System.Numerics;

namespace Helios;

public abstract class Component
{
    public Entity Entity { get; internal set; } = null!;
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

using System;

namespace Helios;

public class Entity
{
    protected Entity() { ID = 0; }
    internal Entity(ulong id) { ID = id; }

    public ulong ID;

    public void Destroy() => NativeAPI.EntityDestroy(ID);
    public void DestroyEntity(ulong id) => NativeAPI.EntityDestroy(id);

    public bool HasComponent<T>() where T : Component, new()
        => NativeAPI.EntityHasComponent(ID, typeof(T).Name);

    public T? GetComponent<T>() where T : Component, new()
    {
        if (!HasComponent<T>()) return null;
        return new T() { Entity = this };
    }
}

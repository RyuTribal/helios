using System;

namespace Helios;

/// <summary>
/// Base class for all user scripts. Attach to entities via ScriptInstance component.
/// Subclass this and override OnCreate/OnUpdate/OnDestroy.
/// Access the entity via Entity property.
/// </summary>
public abstract class ScriptBehaviour
{
    /// <summary>Raw entity ID (generation + index packed into uint64).</summary>
    public ulong EntityId { get; internal set; }

    /// <summary>Entity wrapper for convenient component access.</summary>
    public Entity Entity => new Entity(EntityId);

    /// <summary>Called once when the script instance is created.</summary>
    public virtual void OnCreate() { }

    /// <summary>Called every frame during Schedule.Update.</summary>
    public virtual void OnUpdate(float delta) { }

    /// <summary>Called when the entity is despawned or the script is removed.</summary>
    public virtual void OnDestroy() { }

    /// <summary>Called when this entity collides with another.</summary>
    public virtual void OnCollisionEnter(ulong otherEntityId) { }

    /// <summary>Called every frame while colliding with another entity.</summary>
    public virtual void OnCollisionStay(ulong otherEntityId) { }

    /// <summary>Called when this entity stops colliding with another.</summary>
    public virtual void OnCollisionExit(ulong otherEntityId) { }
}

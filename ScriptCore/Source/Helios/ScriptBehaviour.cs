using System;
using System.Numerics;

namespace Helios;

/// <summary>
/// Base class for all user scripts. Extends Entity so scripts have direct
/// access to entity methods (GetComponent, ApplyForce, LookAt, etc.)
/// plus input, audio, and logging as instance methods.
/// </summary>
public abstract class ScriptBehaviour : Entity
{
    // -- Lifecycle callbacks --------------------------------------------------

    /// <summary>Called once when the script instance is created.</summary>
    public virtual void OnCreate() { }

    /// <summary>Called every frame during Schedule.Update.</summary>
    public virtual void OnUpdate(float delta) { }

    /// <summary>Called when the entity is despawned or the script is removed.</summary>
    public virtual void OnDestroy() { }

    /// <summary>Called when this entity collides with another.</summary>
    public virtual void OnCollisionEnter(ulong otherEntityId, Vector3 contactPoint, Vector3 normal, float impulse) { }

    /// <summary>Called when this entity stops colliding with another.</summary>
    public virtual void OnCollisionExit(ulong otherEntityId) { }

    // -- Input (instance methods for clean script API) -----------------------

    protected bool IsKeyPressed(int keycode) => Input.IsKeyPressed(keycode);
    protected bool IsKeyJustPressed(int keycode) => Input.IsKeyJustPressed(keycode);
    protected bool IsMouseButtonPressed(int button) => Input.IsMouseButtonPressed(button);
    protected Vector2 GetMousePosition() => Input.MousePosition;
    protected Vector2 GetMouseDelta() => Input.MouseDelta;
    protected float GetScrollDelta() => Input.ScrollDelta;
    protected void SetCursorMode(int mode) => Input.SetCursorMode(mode);
    protected int GetCursorMode() => Input.GetCursorMode();
}

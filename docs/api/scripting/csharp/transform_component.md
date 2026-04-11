# TransformComponent

The `TransformComponent` is a native component that manages an entity's position, rotation, and scale.

## Properties

| Property | Type | Description |
| :--- | :--- | :--- |
| `Translation` | `Vector3` | The entity's world-space position. |
| `Rotation` | `Vector3` | The entity's rotation in radians (Euler angles). |
| `Scale` | `Vector3` | The entity's world-space scale. |

## Usage

```csharp
public override void OnUpdate(float delta) {
    var transform = GetComponent<TransformComponent>();
    if (transform != null) {
        // Move the entity forward (assuming Z is forward)
        transform.Translation += new Vector3(0, 0, delta * 5f);
        
        // Rotate the entity slowly around Y-axis
        transform.Rotation += new Vector3(0, delta, 0);
    }
}
```

### Note on Radians
All rotation values in the Helios scripting API use radians rather than degrees. To convert from degrees to radians, multiply by `(float)(Math.PI / 180.0)`.

## Related Pages

- [Entity](entity.md)
- [ScriptBehaviour](script_behaviour.md)

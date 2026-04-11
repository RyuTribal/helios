# Scene

The `Scene` class is a static helper for managing the world's scene state.

## Methods

| Method | Description |
| :--- | :--- |
| `void Load(string)` | Replace the current world with a new scene. |
| `void Instantiate(string)` | Add a sub-scene to the currently active world. |
| `bool IsReady(string)` | Returns true when a scene and its assets are fully pre-loaded. |

## Usage

Scene loading in Helios is typically performed asynchronously. To avoid framerate hitches, you can check if a scene is ready before switching to it.

```csharp
public override void OnUpdate(float delta) {
    if (IsKeyPressed(76)) { // 'L'
        if (Scene.IsReady("Scenes/Level_2.hvescn")) {
            Scene.Load("Scenes/Level_2.hvescn");
        }
    }
}
```

### Scene File Paths
All scene paths are relative to the project's asset root.
- `Scenes/Main_Menu.hvescn`
- `Maps/Desert_Oasis.hvescn`

## Related Pages

- [Entity](entity.md)
- [Scene Manager API](../../core/scene/scene_manager.md)

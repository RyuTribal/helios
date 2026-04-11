# Editor

## Overview

The Helios editor (`helios-editor`) is an ImGui-based application that links
all engine libraries. It provides scene editing, asset management, and
Play/Stop mode for testing.

## Setup

```cpp
#include <helios/editor/editor_plugin.h>

app.add_plugin(helios::editor::EditorPlugin{
    .config = {.project_path = "path/to/MyGame.hveproject"}
});
```

The `EditorPlugin` registers all editor systems, panels, and resources.
The editor application's `main.cpp` sets up the full plugin stack (renderer,
physics, audio, scripting, editor) and calls `app.run()`.

## Panels

### Hierarchy

Displays all entities in the scene as a tree. Drag-and-drop to reparent.
Right-click context menu to create/delete entities and add components.

### Inspector

Shows all components on the selected entity. Each component type has a
custom widget (Transform shows position/rotation/scale sliders, MeshRenderer
shows mesh/material path fields, etc.).

### Viewport

Renders the scene through the editor camera. Supports:
- **Orbit camera**: middle-mouse drag to rotate, Shift+middle to pan, scroll
  to zoom.
- **Gizmo overlay**: translate/rotate/scale handles on the selected entity.
- **Mouse picking**: click to select entities (ray-AABB intersection).

### Content Browser

File browser rooted at the project's asset directory. Displays files and
folders with type-appropriate icons. Double-click to:
- Open `.hvescn` scene files.
- Import source assets (`.glb`, `.png`, `.wav`) into Helios binary format.

### Console

Displays engine log output. Supports filtering by log level (Trace, Debug,
Info, Warn, Error) and by log channel (Core, Renderer, Physics, Script,
etc.). Clear button to flush the log.

### Scene Settings

Edit scene-level properties:
- Skybox cubemap path.
- Ambient light color and intensity.
- Clear color.

### Stats

Real-time performance overlay:
- FPS and frame time.
- Entity count.
- Draw call count.
- GPU memory usage.

## Play / Stop Mode

### Edit mode

- Physics is paused (`PhysicsWorld` pointer is null).
- Scripts do not execute (`ScriptExecutionState.running = false`).
- The editor camera is active (`EditorOnly + ActiveCamera`).
- Entities can be freely created, moved, and deleted.

### Entering Play mode

1. The editor serializes the entire scene to a YAML string
   (`scene_snapshot` in `EditorState`).
2. A fresh `PhysicsWorld` is created and bodies are rebuilt from ECS state.
3. `ScriptExecutionState.running` is set to `true`.
4. `script_create_system` creates managed instances for all
   `ScriptInstance` entities.
5. The first game camera with `ActiveCamera` takes over rendering.

### Stopping Play mode

1. All script instances are destroyed (`invoke_destroy`).
2. `ScriptExecutionState.running` is set to `false`.
3. All non-editor entities are despawned.
4. The scene is restored from the saved YAML snapshot.
5. The `PhysicsWorld` is reset to null.
6. The editor camera regains `ActiveCamera`.

This guarantees Edit-mode state is fully preserved across play sessions.

## Scene System

### SceneRoot

Every loaded scene has a `SceneRoot` entity at the top of its hierarchy:

```cpp
struct SceneRoot {
    std::string scene_name;   // "Main_Scene"
    std::string scene_path;   // "Scenes/Main_Scene.hvescn"
};
```

All scene entities are children (direct or nested) of the `SceneRoot`.

### Save / Load

```cpp
// Save current scene
serializer.save(world, "Scenes/Main_Scene.hvescn");

// Save specific entities
serializer.save_entities(world, root_entities, path);

// Load scene
Entity scene_root = serializer.load_scene(world, "Scenes/Main_Scene.hvescn");
```

### SceneEditState

Tracks which scene is being edited and which have unsaved changes:

```cpp
struct SceneEditState {
    Entity active_scene;
    std::unordered_set<uint32_t> dirty_scenes;
};
```

## Camera Preview

When a `Camera` entity is selected in the Inspector, the editor renders a
small preview in a dedicated render target:

- Target ID: `camera_preview_target_id` (default 9999).
- Resolution: 320x180 (configurable in `EditorState`).
- Displayed as an ImGui texture in the Inspector panel below the Camera
  component.

## Gizmos

The gizmo system (`gizmo_system`) renders interactive handles on the selected
entity. It uses ImGuizmo for rendering and hit testing.

### Operations

| Operation | Key | Description |
|---|---|---|
| Translate | `W` | Move along axes |
| Rotate | `E` | Rotate around axes |
| Scale | `R` | Scale along axes |

### Space

Toggle between **Local** and **World** space with the gizmo space setting in
`EditorState`.

### Snapping

Hold `Ctrl` to snap:
- Translate: 0.5 unit increments (configurable).
- Rotate: 45 degree increments (configurable).
- Scale: 0.1 increments (configurable).

## Mouse Picking

Click in the viewport to select entities. The implementation:

1. Compute a ray from the mouse position through the editor camera.
2. Test the ray against the AABB of every `MeshRenderer` entity.
3. Select the closest hit.

The AABB is derived from the mesh asset's vertex bounds transformed by
`GlobalTransform`.

## Project System

### .hveproject files

A project is defined by a `.hveproject` file (YAML) in the project root:

```yaml
name: MyGame
asset_path: Assets
asset_registry_file: AssetRegistry.hvereg
starting_scene: 12345          # packed AssetHandle of the starting scene
script_assembly: bin/GameScripts.dll
renderer:
  anti_aliasing: None
  present_mode: Fifo
```

### Project structure

```
MyGame/
  MyGame.hveproject
  AssetRegistry.hvereg
  Assets/
    Scenes/
      Main_Scene.hvescn
    Meshes/
      Hero.hvemesh
    Textures/
      Hero_Albedo.hvetex
    Scripts/
      PlayerController.cs
  bin/
    GameScripts.dll
```

### Asset Registry

The `AssetRegistry.hvereg` file tracks all imported assets:

```cpp
struct AssetRegistryEntry {
    uint64_t handle;
    std::string file_path;
    AssetType type;  // Scene, MeshSource, Texture, CubeMap, Audio, Script, Material
};
```

### Creating a project

```cpp
auto project = Project::create_new(directory, "MyGame", build_state, pool);
project.save();
```

### Applying to world

```cpp
project.apply_to_world(world);
// Sets AssetServer root, loads starting scene, applies render settings
```

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `W` | Translate gizmo |
| `E` | Rotate gizmo |
| `R` | Scale gizmo |
| `Ctrl+S` | Save scene |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Delete` | Delete selected entity |
| `Ctrl+D` | Duplicate selected entity |
| `F` | Focus camera on selected entity |
| Play button | Enter Play mode |
| Stop button | Exit Play mode, restore scene |

## Undo / Redo

The `EditorCommands` system provides type-safe undo/redo:

```cpp
struct EditorCommands {
    template<typename T>
    void set(Entity entity, World& world, const T& new_value,
             const std::string& description);
    bool undo(World& world);
    bool redo(World& world);
    bool can_undo() const;
    bool can_redo() const;
};
```

Each `set()` call captures the old value and pushes an undo entry. The undo
stack is truncated when a new edit is made after undoing.

## Editor Camera

The editor uses an orbit camera controller:

```cpp
struct EditorCameraController {
    float yaw, pitch;
    float distance;              // orbit distance
    float pan_speed, rotate_speed, zoom_speed;
    float min_distance, max_distance;
    glm::vec3 focal_point;
    bool cursor_captured;
};
```

- **Orbit**: Middle mouse drag rotates around the focal point.
- **Pan**: Shift + middle mouse drag moves the focal point.
- **Zoom**: Scroll wheel adjusts orbit distance.

The editor camera entity has the `EditorOnly` tag, which excludes it from
serialization, physics, and script systems.

## Script Build Integration

The editor can build C# scripts via `dotnet build`, tracked by the
`ScriptBuildState` resource:

```cpp
struct ScriptBuildState {
    enum class Status { Idle, Building, Success, Failed };
    std::atomic<Status> status;
    std::string output;
};
```

Builds run asynchronously on the shared thread pool. The editor UI shows
build status and output in the Console panel.

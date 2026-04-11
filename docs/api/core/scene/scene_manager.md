# SceneManager

The `SceneManager` handles the lifecycle of game scenes, allowing developers to group entities and manage their loading, spawning, and unloading as a single unit.

## Overview

A scene is a collection of entity blueprints. The `SceneManager` allows for both programmatic scene construction and (in Phase 3) file-based scene management. It also coordinates with the `AssetServer` to ensure all assets required by a scene are preloaded before it is spawned.

## Methods

### Lifecycle Management

| Method | Description |
| :--- | :--- |
| `create(name)` | Create a new empty scene with the given name. Returns a `SceneHandle`. |
| `unload(handle)`| Free all scene data (blueprints, asset handles, etc.). |
| `is_valid(handle)`| Checks if the scene handle is currently valid. |

### Blueprint Construction

| Method | Description |
| :--- | :--- |
| `add_entity(handle, name, comps...)`| Add an entity blueprint with the given components. |
| `add_asset(handle, path)`| Register a path for preloading. |

### Spawning and Asset Loading

| Method | Description |
| :--- | :--- |
| `preload(handle, server)` | Begin asynchronous preloading of all registered assets. |
| `is_preloaded(handle, server)`| Checks if all preloaded assets are ready. |
| `spawn(handle, world, server)` | Instantiates all entity blueprints into the world. |
| `despawn(handle, world)`| Removes all spawned entities belonging to this scene. |

## Usage Example

```cpp
// 1. Define a scene
auto scene = scenes.create("MainMenu");
scenes.add_entity(scene, "MainCamera", Camera{...}, Transform{...});
scenes.add_entity(scene, "UI", UIRoot{...});
scenes.add_asset(scene, "textures/ui_atlas.png");

// 2. Preload assets (e.g., during loading screen)
scenes.preload(scene, asset_server);

// 3. Spawn once assets are ready
if (scenes.is_preloaded(scene, asset_server)) {
    scenes.spawn(scene, world, asset_server);
}
```

## Scene Data

The `SceneManager` stores information about each scene in an internal `SceneData` structure. Each spawned entity is tagged with a `SceneTag` component, allowing the manager to track and despawn them as needed.

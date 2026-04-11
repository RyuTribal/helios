# Scenes

## Overview

Helios uses a **YAML-based scene serialization** system. Scenes are stored as `.hvescn` files which describe a hierarchy of entities and their associated components. This allows for both entire levels and reusable "prefabs" (sub-scenes) to be defined in a human-readable format.

## The .hvescn Structure

A scene file defines a root `scene` node containing a list of `entities`. Each entity has a `name` and a `components` map.

```yaml
scene:
  entities:
    - name: Main Camera
      components:
        Transform:
          position: [0, 5, 10]
          rotation: [-0.2, 0, 0, 0.98]
        Camera:
          fov: 60.0
          near: 0.1
          far: 1000.0

    - name: Player
      components:
        Transform:
          position: [0, 1, 0]
        RigidBody:
          body_type: Dynamic
          mass: 1.0
        BoxCollider:
          half_extents: [0.5, 0.5, 0.5]
```

## SceneRoot Component

When a scene is loaded, Helios typically creates a **SceneRoot** entity. This entity serves as the parent for all top-level entities in that scene, allowing you to manage the entire scene as a single hierarchy (e.g., for moving, disabling, or despawning).

```cpp
struct SceneRoot {
    std::string scene_name;
    std::string scene_path; // e.g., "Scenes/Level1.hvescn"
};
```

## Loading and Instantiating

Helios distinguishes between **Loading** a scene (replacing the current world state) and **Instantiating** a scene (adding it to the current world).

### SceneManager vs SceneSerializer

- **SceneSerializer**: A low-level utility for reading/writing YAML files to the ECS `World`.
- **SceneManager**: A high-level system that manages scene lifetimes, preloading, and instantiation.

### Loading a Scene (Replace)

Loading a scene usually involves clearing the current world (except for persistent or editor-only entities) and then deserializing the new scene.

```cpp
// C++ Example
void load_level(World& world, const std::string& path) {
    // Clear existing entities
    world.clear(); 
    
    SceneSerializer serializer;
    // Register required components before loading
    // serializer.register_component<Transform>("Transform", ...);
    
    serializer.load_scene(world, path);
}
```

### Instantiating a Scene (Additive)

Instantiating a scene (or "prefab") adds its entities to the existing world, often parented under a new `SceneRoot`.

```cpp
// C++ Example: Instantiating an enemy prefab
Entity spawn_enemy(World& world, const std::string& prefab_path, glm::vec3 position) {
    SceneSerializer serializer;
    // Note: load_scene creates a SceneRoot and parents loaded entities under it
    Entity root = serializer.load_scene(world, prefab_path);
    
    if (auto* t = world.try_get<Transform>(root)) {
        t->position = position;
    }
    return root;
}
```

## Serialization Lifecycle

1. **Registration**: Component types must be registered with the `SceneSerializer` so it knows how to map YAML keys to C++ types.
2. **Serialization**: The `save` method iterates through entities, calling the registered serialization functions for each component.
3. **Deserialization**: The `load` method parses the YAML, creates entities, and adds components using the registered deserialization functions.

The `ScenePlugin` automatically registers all built-in Helios components (Transform, MeshRenderer, RigidBody, etc.) during engine initialization.

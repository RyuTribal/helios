# SceneSerializer

The `SceneSerializer` is responsible for converting a `World` of entities and components to and from a YAML representation.

## Overview

The serializer allows for full scene persistence, including:
- **Component Registration**: Each component type must be registered with the serializer to be saved or loaded.
- **Entity Hierarchies**: Automatically handles entity parenting and children.
- **Asset Handles**: Correctly serializes handles to external assets.

## Methods

### Configuration

| Method | Description |
| :--- | :--- |
| `register_component<T>(name, ser, deser)` | Register a component for serialization with its name and conversion functions. |

### Saving

| Method | Description |
| :--- | :--- |
| `save(world, path)` | Save all entities in the world to a YAML file. |
| `save_entities(world, roots, path)`| Save a specific set of entities and their children. |
| `save_to_string(world)` | Serialize the entire world to a YAML string. |

### Loading

| Method | Description |
| :--- | :--- |
| `load(world, path)` | Load entities from a YAML file into the world. |
| `load_scene(world, path)` | Load a full scene, creating a new `SceneRoot` for orphans. |
| `load_from_string(world, yaml)` | Load entities from an in-memory YAML string. |

## Usage Example

```cpp
// 1. Configure the serializer
SceneSerializer serializer;
serializer.register_component<Transform>(
    "Transform",
    serialize_yaml_transform,
    deserialize_yaml_transform
);
serializer.register_component<MeshRenderer>(
    "MeshRenderer",
    serialize_yaml_mesh_renderer,
    deserialize_yaml_mesh_renderer
);

// 2. Save current world
serializer.save(world, "assets/scenes/my_scene.yaml");

// 3. Load later
serializer.load(world, "assets/scenes/my_scene.yaml");
```

## Serialization Example (YAML)

The resulting YAML file typically looks like this:

```yaml
Entities:
  - Entity: 123456789
    Components:
      - Transform:
          Translation: [0, 10, 0]
          Rotation: [0, 0, 0, 1]
          Scale: [1, 1, 1]
      - MeshRenderer:
          Mesh: "assets/meshes/cube.hlasset"
          Material: "assets/materials/red.hlasset"
```

## Related Pages
- [SceneManager](scene_manager.md)
- [World](../ecs/world.md)

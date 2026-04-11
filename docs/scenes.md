# Scenes

## Overview

Scenes in Helios are YAML files (`.hvescn`) that describe a hierarchy of
entities and their components. The `SceneSerializer` handles reading and
writing. At runtime, scenes are managed through the `SceneManager` or loaded
directly via the serializer.

## Scene Format

Scene files are YAML. Each entity is a mapping with a `tag` (name), optional
`parent` reference, and a list of component blocks:

```yaml
- entity: 1
  tag: Player
  Transform:
    position: [0, 1, 0]
    rotation: [0, 0, 0, 1]
    scale: [1, 1, 1]
  MeshRenderer:
    mesh_path: Meshes/Hero.hvemesh
    material_path: Materials/Hero.hvemat
    flags: 3
    sort_order: 0
  RigidBody:
    body_type: Dynamic
    mass: 80.0
    friction: 0.6
    restitution: 0.1
    flags: 1

- entity: 2
  tag: Weapon
  parent: 1
  Transform:
    position: [0.5, 0, 0]
    rotation: [0, 0, 0, 1]
    scale: [1, 1, 1]
  MeshRenderer:
    mesh_path: Meshes/Sword.hvemesh
```

### Entity numbering

Entity indices in the file are local identifiers used only to resolve parent
references during deserialization. They do not correspond to runtime ECS
entity IDs.

### SceneRoot entity

When loading a full scene (via `load_scene`), the serializer creates or
finds a `SceneRoot` entity and parents all top-level entities under it:

```yaml
- entity: 0
  tag: Main_Scene
  SceneRoot:
    scene_name: Main_Scene
    scene_path: Scenes/Main_Scene.hvescn
```

## SceneSerializer

### Registration

Component types must be registered before save/load. Each registration
provides a name string and serialize/deserialize callbacks:

```cpp
SceneSerializer serializer;

serializer.register_component<Transform>("Transform",
    [](YAML::Emitter& out, const Transform& t) {
        out << YAML::Key << "position" << YAML::Value << t.position;
        out << YAML::Key << "rotation" << YAML::Value << t.rotation;
        out << YAML::Key << "scale"    << YAML::Value << t.scale;
    },
    [](const YAML::Node& node) -> Transform {
        Transform t;
        t.position = node["position"].as<glm::vec3>();
        t.rotation = node["rotation"].as<glm::quat>();
        t.scale    = node["scale"].as<glm::vec3>();
        return t;
    }
);
```

Or with raw serialize/deserialize function objects:

```cpp
serializer.register_component("MyComponent",
    // SerializeFn: (World&, Entity, Emitter&) -> bool
    [](const World& world, Entity entity, YAML::Emitter& out) -> bool {
        auto* comp = world.try_get<MyComponent>(entity);
        if (!comp) return false;
        out << YAML::Key << "value" << YAML::Value << comp->value;
        return true;
    },
    // DeserializeFn: (World&, Entity, Node&) -> void
    [](World& world, Entity entity, const YAML::Node& node) {
        MyComponent c;
        c.value = node["value"].as<int>();
        world.add(entity, std::move(c));
    }
);
```

### Save

```cpp
// Save all entities to a file
serializer.save(world, "Scenes/Main_Scene.hvescn");

// Save specific root entities (and their children recursively)
serializer.save_entities(world, {player_entity, environment_entity}, path);

// Save to an in-memory string (for editor Play/Stop snapshot)
std::string yaml = serializer.save_to_string(world);
```

### Load

```cpp
// Load entities into the world (raw -- no SceneRoot wrapping)
serializer.load(world, "Scenes/Main_Scene.hvescn");

// Full scene load: creates SceneRoot, parents orphans under it
Entity scene_root = serializer.load_scene(world, "Scenes/Main_Scene.hvescn");

// Load from an in-memory string
serializer.load_from_string(world, yaml_string);

// Full restore from string (used by editor Play/Stop)
Entity root = serializer.load_scene_from_string(world, yaml_string);
```

## SceneRoot Component

```cpp
struct SceneRoot {
    std::string scene_name;   // "Main_Scene"
    std::string scene_path;   // "Scenes/Main_Scene.hvescn" (relative to asset root)
};
```

The `SceneRoot` entity is the top of the scene hierarchy. All entities in the
scene are children (direct or nested) of it. When loading via `load_scene()`,
orphan entities (those without a `Parent` component) are automatically
parented under the `SceneRoot`.

An empty `scene_path` means the scene has never been saved to disk.

## Hierarchy Serialization

Parent-child relationships are serialized via `parent` references in YAML.
During save, each entity's `Parent` component (if present) is written as the
parent entity's local index. During load, the deserializer resolves these
references and calls `set_parent()` to rebuild the hierarchy.

```yaml
- entity: 1
  tag: Car

- entity: 2
  tag: Wheel_FL
  parent: 1

- entity: 3
  tag: Wheel_FR
  parent: 1
```

## EditorOnly Filtering

Entities with the `EditorOnly` component are skipped during serialization.
This includes:
- The editor camera entity.
- Debug visualization entities.
- Any entity marked as editor-internal.

The serializer checks for `EditorOnly` before writing each entity and skips
it entirely.

Similarly, the physics and scripting systems skip `EditorOnly` entities.

## Scene Switching at Runtime

### From C++ systems

Use the `SceneSerializer` directly:

```cpp
void switch_scene(World& world) {
    // Despawn all current entities (except EditorOnly)
    auto q = world.query<const Tag, Without<EditorOnly>>();
    for (auto [entity, tag] : q.with_entity()) {
        world.despawn(entity);
    }

    // Load the new scene
    SceneSerializer serializer;
    // ... register components ...
    serializer.load_scene(world, "Scenes/Level_2.hvescn");
}
```

### From C# scripts

```csharp
// Replace the current scene
Scene.Load("Scenes/Level_2.hvescn");

// Add a sub-scene alongside the current one
Scene.Instantiate("Scenes/enemy_wave.hvescn");

// Preload + switch (non-blocking)
if (Scene.IsReady("Scenes/Level_2.hvescn"))
    Scene.Load("Scenes/Level_2.hvescn");
```

`Scene.IsReady()` parses the scene file on first call and kicks off async
asset loads for all referenced meshes, textures, and audio. It returns `true`
once everything is loaded.

## SceneManager

For programmatic scene building (not YAML files), use the `SceneManager`:

```cpp
SceneManager scenes;
auto h = scenes.create("village");

scenes.add_entity(h, "house",
    Transform{.position = {10, 0, 0}},
    MeshRenderer{.mesh = house_mesh}
);

// Register assets for preloading
scenes.add_asset(h, "Meshes/house.hvemesh");

// Preload assets
scenes.preload(h, server);

// Check readiness
if (scenes.is_preloaded(h, server)) {
    scenes.spawn(h, world, server);
}

// Later: despawn and unload
scenes.despawn(h, world);
scenes.unload(h);
```

## Editor Play/Stop Snapshot

The editor uses in-memory serialization for the Play/Stop workflow:

1. **Play**: `save_to_string(world)` captures all entities as YAML in
   `EditorState::scene_snapshot`.
2. **Stop**: All non-editor entities are despawned, then
   `load_scene_from_string(world, scene_snapshot)` restores the exact
   pre-play state.

This ensures no data loss during play testing.

## Asset Path Resolution

`MeshRenderer` components store both a runtime `Handle<T>` and a persistent
path string:

```cpp
struct MeshRenderer {
    Handle<MeshAsset> mesh;
    std::string mesh_path;       // persisted in scene file
};
```

After loading a scene, `resolve_mesh_paths()` kicks off async loads for
entities that have a path but no loaded handle:

```cpp
void resolve_mesh_paths(World& world) {
    auto& server = world.resource<std::shared_ptr<AssetServer>>();
    auto q = world.query<MeshRenderer>();
    for (auto [e, mr] : q.with_entity()) {
        if (!mr.mesh_path.empty() && !mr.mesh)
            mr.mesh = server->load<MeshAsset>(mr.mesh_path);
    }
}
```

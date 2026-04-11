# Code Snippets

This page provides a collection of concise, ready-to-use code snippets for common
tasks in the Helios engine.

## Rotating an Entity

Use a system that queries for `Transform` components and rotates them based on
the frame delta time.

```cpp
void rotate_system(Query<Transform> query, Res<Time> time) {
    float dt = time->delta();
    for (auto& transform : query) {
        // Rotate around the Y axis (up)
        // Note: rotation is stored as a quaternion
        transform.rotation = glm::rotate(transform.rotation, 1.0f * dt, glm::vec3(0, 1, 0));
    }
}
```

## Loading and Playing a Sound

Load an audio asset via the `AssetServer` and play it using the `AudioDevice`.

```cpp
void play_sound_system(Res<AssetServer> server, ResMut<std::unique_ptr<AudioDevice>> audio) {
    // 1. Load the audio data (returns a Handle<AudioData>)
    auto handle = server->load<AudioData>("Assets/Audio/Explosion.wav");

    // 2. Check if it's loaded before playing
    if (server->is_loaded(handle.untyped())) {
        const auto* data = server->get<AudioData>(handle.untyped());
        
        // 3. Play at a specific world position
        (*audio)->play_at(data->file_bytes.data(), data->file_bytes.size(), glm::vec3(0, 5, 0));
    }
}
```

## Casting a Ray

Perform a raycast in the physics world to detect objects along a line.

```cpp
void raycast_system(Res<std::unique_ptr<physics::PhysicsWorld>> physics) {
    glm::vec3 origin(0, 10, 0);
    glm::vec3 direction(0, -1, 0);
    float max_distance = 100.0f;

    auto hit = (*physics)->raycast(origin, direction, max_distance);
    if (hit.has_value()) {
        Entity entity = hit->entity;
        float distance = hit->distance;
        glm::vec3 point = hit->position;
        // ... handle hit
    }
}
```

## Querying with Filters

Filter entities based on the presence or absence of specific components.

```cpp
// Query for entities that have Transform AND RigidBody, but NOT PlayerTag
void query_filtered_system(Query<const Transform, With<RigidBody>, Without<PlayerTag>> query) {
    for (const auto& transform : query) {
        // ...
    }
}
```

## Spawning a Prefab

Load a scene file (`.hvescn`) and instantiate its entities into the world.

```cpp
void spawn_prefab(World& world, Res<SceneSerializer> serializer) {
    // load_scene returns the root Entity of the instantiated scene
    Entity root = serializer->load_scene(world, "Assets/Scenes/EnemyPrefab.hvescn");
}
```

## Handling Mouse Clicks

Detect when a mouse button is pressed using the `RawInput` resource.

```cpp
void input_system(Res<RawInput> input) {
    if (input->mouse_button_just_pressed(MouseButton::Left)) {
        glm::vec2 pos = input->mouse_position();
        // ... handle left click
    }
}
```

## Related Pages

- [ECS Deep Dive](../book/ecs.md)
- [Assets & Resource Management](../book/assets.md)
- [Physics Guide](../book/physics.md)
- [Audio Guide](../book/audio.md)
- [Scenes Guide](../book/scenes.md)

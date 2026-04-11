# Audio Components API Reference

The Helios Audio module utilizes ECS components to attach audio playback behavior to entities.

## AudioSource (WIP)

The `AudioSource` component is intended to define a sound emitter attached to an entity. **Full integration with the audio backend is currently in progress.**

| Member | Type | Description |
| :--- | :--- | :--- |
| `clip` | `SoundHandle` | The sound clip to be played. |
| `volume` | `float` | Volume for this instance (0.0 to 1.0). |
| `pitch` | `float` | Playback speed multiplier (default: 1.0). |
| `flags` | `uint32_t` | (Placeholder) `Looping`, `PlayOnStart`, etc. |

### Audio Flags (Placeholder)

- **`Looping`**: The sound will repeat indefinitely once started.
- **`PlayOnStart`**: The sound will automatically start playing when the entity is spawned.

## Listener (Implicit)

The audio module is designed to treat the entity with the `ActiveCamera` component as the audio listener. The position and orientation of this entity's `GlobalTransform` will be used to update the `AudioDevice` listener settings. **This system is currently a placeholder.**

## Example: Creating a Spatial Audio Source

```cpp
auto entity = cmds.spawn();
cmds.insert(entity, helios::Transform{ .position = {10, 0, 0} });
cmds.insert(entity, helios::AudioSource{
    .clip = my_clip_handle,
    .volume = 0.8f,
    .flags = helios::AudioFlags::PlayOnStart | helios::AudioFlags::Looping
});
```

## Related Pages

- [Audio API Overview](index.md)
- [AudioDevice API](audio_device.md)

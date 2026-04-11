# AudioDevice API Reference

The `AudioDevice` class provides an abstract interface for audio playback and listener management. It handles raw PCM data playback, 3D spatial calculations, and global audio settings.

## Overview

An `AudioDevice` is created and managed by the `AudioPlugin`. It is available as a world resource in the ECS:

```cpp
#include <helios/audio/interface/audio_device.h>

void play_background_music(helios::ResMut<std::unique_ptr<helios::audio::AudioDevice>> device) {
    if (*device) {
        // Direct audio access...
    }
}
```

## Public Methods

### Playback

| Method | Description |
| :--- | :--- |
| `play(pcm_data, size, params)` | Plays a 2D sound. Returns a `SoundHandle`. |
| `play_at(pcm_data, size, pos, params)` | Plays a spatialized 3D sound at the given position. |
| `stop(SoundHandle)` | Stops the sound instance immediately. |
| `pause(SoundHandle)` | Pauses the sound (can be resumed). |
| `resume(SoundHandle)` | Resumes a paused sound. |
| `is_playing(SoundHandle)` | Returns true if the sound is currently playing. |

### Sound Control

| Method | Description |
| :--- | :--- |
| `set_volume(SoundHandle, float)` | Sets the volume for the sound instance (0.0 to 1.0). |
| `set_position(SoundHandle, vec3)` | Updates the 3D position of a spatial sound instance. |

### Listener Management

| Method | Description |
| :--- | :--- |
| `set_listener(pos, forward, up)` | Sets the 3D position and orientation of the listener. |

## Data Structures

### PlayParams

Used when calling `play` or `play_at`.

- `volume`: Initial volume (default: 1.0).
- `loop`: If true, the sound loops indefinitely (default: false).
- `pitch`: Playback speed multiplier (default: 1.0).

## Code Snippet: Simple Playback

```cpp
#include <helios/audio/interface/audio_device.h>

void fire_and_forget(helios::audio::AudioDevice& device, const void* pcm, size_t size) {
    helios::audio::PlayParams params;
    params.volume = 0.5f;
    params.loop = false;

    device.play(pcm, size, params);
}
```

## Related Pages

- [Audio API Overview](index.md)
- [Audio Components](components.md)

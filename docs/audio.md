# Audio

## Overview

Audio is provided by the `helios-audio` library. It wraps SoLoud behind an
abstract `AudioDevice` interface and integrates with the ECS via an
`AudioPlugin`.

## Setup

```cpp
#include <helios/audio/audio_plugin.h>
#include <helios/audio/soloud_device.h>

app.add_plugin(helios::audio::AudioPlugin<helios::audio::SoLoudDevice>{});
```

The plugin inserts:
- `std::unique_ptr<AudioDevice>` as a world resource.
- An `audio_update` system in `PostUpdate` for per-frame housekeeping.

## AudioDevice Interface

```cpp
class AudioDevice {
public:
    // Play from raw PCM/WAV data. Returns a handle to control the instance.
    virtual SoundHandle play(const void* pcm_data, size_t size,
                             const PlayParams& params = {}) = 0;

    // Play at a 3D world position (spatial audio).
    virtual SoundHandle play_at(const void* pcm_data, size_t size,
                                const glm::vec3& position,
                                const PlayParams& params = {}) = 0;

    // Instance control
    virtual void stop(SoundHandle handle) = 0;
    virtual void pause(SoundHandle handle) = 0;
    virtual void resume(SoundHandle handle) = 0;
    virtual bool is_playing(SoundHandle handle) const = 0;
    virtual void set_volume(SoundHandle handle, float volume) = 0;

    // 3D audio
    virtual void set_position(SoundHandle handle, const glm::vec3& pos) = 0;
    virtual void set_listener(const glm::vec3& position,
                              const glm::vec3& forward,
                              const glm::vec3& up) = 0;

    // Per-frame update (stream buffers, 3D calculations)
    virtual void update() = 0;
};
```

### PlayParams

```cpp
struct PlayParams {
    float volume = 1.0f;    // 0.0 = silent, 1.0 = full
    bool  loop   = false;
    float pitch  = 1.0f;    // playback speed multiplier
};
```

### SoundHandle

`SoundHandle` is an opaque `uint64_t`. Zero means invalid / no sound.

## Playing Sounds

### From C++

```cpp
auto& device = *world.resource<std::unique_ptr<audio::AudioDevice>>();

// 2D playback
auto handle = device.play(pcm_data, data_size, {.volume = 0.8f, .loop = true});

// 3D spatial playback
auto handle = device.play_at(pcm_data, data_size, position, {.volume = 1.0f});

// Control
device.set_volume(handle, 0.5f);
device.set_position(handle, new_position);
device.stop(handle);
```

### From C# scripts

```csharp
// Load an audio asset (do this in OnCreate)
private AssetHandle explosionSound;

public override void OnCreate()
{
    explosionSound = Assets.Load("Sounds/explosion.hveaudio");
}

// Play at a 3D position
public override void OnCollisionEnter(ulong other, Vector3 point, Vector3 normal, float impulse)
{
    Audio.Play(explosionSound, point, volume: 1.0f, loop: false);
}

// Convenience: load + play by path (may be silent on first call if not cached)
Audio.Play("Sounds/click.hveaudio", new Vector3(0, 0, 0));
```

## 3D Spatial Audio

For 3D positioning to work, set the listener each frame:

```cpp
device.set_listener(camera_position, camera_forward, camera_up);
```

Sounds played with `play_at()` are positioned in world space. The device
calculates volume attenuation and stereo panning based on the listener's
position and orientation.

Update a moving sound source's position:

```cpp
device.set_position(handle, new_world_position);
```

## AudioSource Component

The `AudioSource` component stores per-entity audio state:

```cpp
struct AudioSource {
    SoundHandle clip;
    float    volume = 1.0f;
    float    pitch  = 1.0f;
    uint32_t flags  = 0;       // AudioFlags::Looping, AudioFlags::PlayOnStart
};
```

### Audio flags

| Flag | Value | Description |
|---|---|---|
| `AudioFlags::Looping` | `1 << 0` | Sound loops indefinitely |
| `AudioFlags::PlayOnStart` | `1 << 1` | Sound starts playing when the entity is spawned |

## AudioData Asset

Audio files are imported into the `.hveaudio` binary format using the
audio importer. The `AssetServer` loads them as raw PCM data that can be
passed directly to `AudioDevice::play()`.

Load audio assets the same way as any other asset:

```cpp
auto audio = server->load<AudioData>("Sounds/music.hveaudio");
```

## SoLoud Backend

`SoLoudDevice` is the concrete `AudioDevice` implementation. It uses SoLoud
with the miniaudio backend for cross-platform audio output.

SoLoud handles mixing, resampling, and 3D audio calculations internally.
The `update()` call drives stream buffer refills and 3D recalculation.

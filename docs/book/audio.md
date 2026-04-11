# Audio

## Overview

Helios provides a high-level audio system powered by the **SoLoud** library. The `helios-audio` library abstracts audio playback behind an `AudioDevice` interface, supporting both simple 2D sound effects and full 3D spatialized audio.

## Setup

To enable audio, add the `AudioPlugin` with the `SoLoudDevice` implementation to your application:

```cpp
#include <helios/audio/audio_plugin.h>
#include <helios/audio/soloud_device.h>

app.add_plugin(helios::audio::AudioPlugin<helios::audio::SoLoudDevice>{});
```

This plugin inserts a `std::unique_ptr<AudioDevice>` as a world resource and adds an `audio_update` system to the `PostUpdate` schedule.

## AudioSource Component

The `AudioSource` component is used to manage audio playback attached to an entity. It stores state such as the current sound handle, volume, pitch, and playback flags.

```cpp
struct AudioSource {
    SoundHandle clip;      // Runtime handle to the playing sound
    float volume = 1.0f;   // 0.0 to 1.0
    float pitch = 1.0f;    // 1.0 is default playback speed
    uint32_t flags = 0;    // e.g., AudioFlags::Looping, AudioFlags::PlayOnStart
};
```

## Playing Sounds

### Global (2D) Sounds

For UI sounds, background music, or non-spatial effects, use `AudioDevice::play`.

```cpp
void play_ui_click(World& world, Handle<AudioData> click_asset) {
    auto& device = *world.resource<std::unique_ptr<audio::AudioDevice>>();
    auto* audio_data = world.resource<AssetServer>()->get<AudioData>(click_asset);
    
    if (audio_data) {
        device.play(audio_data->file_bytes.data(), audio_data->file_bytes.size(), {
            .volume = 0.5f,
            .loop = false
        });
    }
}
```

### Spatial (3D) Sounds

3D sounds are spatialized based on their world position relative to the listener.

```cpp
void play_explosion(World& world, glm::vec3 position, Handle<AudioData> explosion_asset) {
    auto& device = *world.resource<std::unique_ptr<audio::AudioDevice>>();
    auto* audio_data = world.resource<AssetServer>()->get<AudioData>(explosion_asset);
    
    if (audio_data) {
        device.play_at(audio_data->file_bytes.data(), audio_data->file_bytes.size(), 
                       position, {
                           .volume = 1.0f,
                           .loop = false
                       });
    }
}
```

## Updating the Listener

For spatial audio to work correctly, the `AudioDevice` needs to know the position and orientation of the listener (usually the active camera). This is typically updated once per frame.

```cpp
void update_audio_listener(Res<ActiveCamera> active_cam, 
                           ResMut<std::unique_ptr<AudioDevice>> device,
                           Query<const Transform> transforms) {
    if (auto* transform = transforms.try_get(active_cam->entity)) {
        glm::vec3 pos = transform->position;
        glm::vec3 forward = transform->rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up      = transform->rotation * glm::vec3(0.0f, 1.0f, 0.0f);

        device->set_listener(pos, forward, up);
    }
}
```

## Audio Assets

Helios supports importing common audio formats like `.wav`, `.mp3`, `.ogg`, and `.flac`. During the asset import process, these files are processed and stored as `AudioData` assets (often with the `.hveaudio` extension in the baked asset registry).

You can load these assets using the `AssetServer`:

```cpp
auto music_handle = asset_server->load<AudioData>("Music/MainTheme.mp3");
```

The `AudioData` struct contains the raw file bytes which are passed to the `AudioDevice` for decoding and playback.

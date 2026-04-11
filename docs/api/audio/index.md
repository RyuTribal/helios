# Audio API Reference

The Helios Audio module provides an ECS-integrated interface for spatialized audio playback. It uses **SoLoud** combined with **miniaudio** as the primary backend.

## Core Concepts

- **Audio Device**: The central audio output manager. Handles sound instance lifecycle and 3D spatial calculations.
- **Audio Source**: ECS component used to attach sound clips to entities for spatial playback.
- **Listener**: The virtual 3D "ear" in the scene, usually attached to the active camera.
- **Sound Handle**: An opaque identifier for a playing sound instance, used for runtime control (pause, volume, etc.).

## API Pages

- [**AudioDevice**](audio_device.md): The main interface for manual playback and listener control.
- [**AudioPlugin**](audio_plugin.md): Registration of the audio module.
- [**Components**](components.md): ECS components for defining sound playback behavior.

## Getting Started

To enable audio in your application, add the `AudioPlugin` to your `App` builder:

```cpp
#include <helios/audio/interface/audio_plugin.h>
#include <helios/audio/soloud/soloud_device.h>

using namespace helios::audio;

app.add_plugin(AudioPlugin<SoLoudDevice>{});
```

Once the plugin is active, you can play sounds manually through the `AudioDevice` resource or by attaching an `AudioSource` component to an entity.

## Related Pages

- [Audio Architecture](../../book/audio.md)
- [ECS Overview](../../book/ecs.md)

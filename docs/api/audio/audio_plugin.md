# AudioPlugin API Reference

The `AudioPlugin` handles the registration of audio-related resources and systems within the Helios `App`. It is a template class that accepts an `AudioDevice` backend implementation.

## Overview

```cpp
#include <helios/audio/interface/audio_plugin.h>
#include <helios/audio/soloud/soloud_device.h>

app.add_plugin(helios::audio::AudioPlugin<helios::audio::SoLoudDevice>{});
```

## Registered Systems

When the plugin is added, it automatically registers several systems into the `App` schedule:

| System | Schedule | Description |
| :--- | :--- | :--- |
| `audio_update` | `PostUpdate` | Performs per-frame audio housekeeping (stream buffers, 3D calculations). |
| `update_audio_listener` | `PostUpdate` | (Placeholder) Updates the `AudioDevice` listener from the active camera. |
| `update_spatial_sources` | `PostUpdate` | (Placeholder) Updates playing spatial audio instances from `AudioSource` components. |

## Lifecycle

The `AudioPlugin` ensures the `AudioDevice` is correctly initialized when the application starts and cleaned up when the application exits. It is available as a world resource as a `std::unique_ptr<AudioDevice>`.

## Related Pages

- [Audio API Overview](index.md)
- [AudioDevice API](audio_device.md)

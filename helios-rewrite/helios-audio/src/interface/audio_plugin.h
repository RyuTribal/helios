// helios-audio/src/interface/audio_plugin.h
#pragma once

#include <memory>
#include <type_traits>

#include "interface/audio_device.h"
#include "stub/stub_audio_device.h"

namespace helios::audio {

// ============================================================
// AudioPlugin
// ============================================================

// Backend concept: must derive from AudioDevice.
//
// Example backends:
//   StubAudioDevice  -- no-op (no external dependency)
//   SoLoudDevice     -- production (SoLoud, if available)

template<typename Backend>
struct AudioPlugin {
    static_assert(std::is_base_of_v<AudioDevice, Backend>,
                  "Audio backend must derive from AudioDevice");

    void build(auto& app) {
        auto device = std::make_unique<Backend>();
        app.template insert_resource<std::unique_ptr<AudioDevice>>(std::move(device));

        // Register audio systems (uncomment when ECS API from Plans 1-2 is wired):
        // app.add_system(Schedule::PostUpdate, update_audio_listener, "update_audio_listener");
        // app.add_system(Schedule::PostUpdate, update_spatial_sources, "update_spatial_sources");
        // app.add_system(Schedule::PostUpdate, audio_update, "audio_update");
    }
};

// Convenience alias for stub use
using StubAudioPlugin = AudioPlugin<StubAudioDevice>;

} // namespace helios::audio

// helios-audio/src/interface/audio_plugin.h
#pragma once

#include <memory>
#include <type_traits>

#include "interface/audio_device.h"

#include <helios/ecs/schedule.h>
#include <helios/ecs/system_params.h>

namespace helios::audio {

// ============================================================
// Audio systems (free functions, registered by the plugin)
// ============================================================

// Schedule: PostUpdate
// Per-frame audio housekeeping (stream buffers, 3D calculations).
inline void audio_update(helios::ResMut<std::unique_ptr<AudioDevice>> device) {
    if (*device) {
        (*device)->update();
    }
}

// ============================================================
// AudioPlugin
// ============================================================

// Backend concept: must derive from AudioDevice.
//
// Use SoLoudDevice (the only backend) or AudioPlugin<SoLoudDevice> directly.

template<typename Backend>
struct AudioPlugin {
    static_assert(std::is_base_of_v<AudioDevice, Backend>,
                  "Audio backend must derive from AudioDevice");

    void build(auto& app) {
        auto device = std::make_unique<Backend>();
        app.template insert_resource<std::unique_ptr<AudioDevice>>(std::move(device));

        // Per-frame audio update
        app.add_system(helios::Schedule::PostUpdate, audio_update, "audio_update");

        // Full ECS-driven spatial audio (uncomment when wiring to components):
        // app.add_system(helios::Schedule::PostUpdate, update_audio_listener, "update_audio_listener");
        // app.add_system(helios::Schedule::PostUpdate, update_spatial_sources, "update_spatial_sources");
    }
};

} // namespace helios::audio

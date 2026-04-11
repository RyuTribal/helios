#pragma once

#include <memory>
#include "interface/audio_device.h"

namespace helios::audio {

/// Create an AudioDevice backed by SoLoud.
std::unique_ptr<AudioDevice> create_audio_device();

} // namespace helios::audio

#include "interface/audio_plugin.h"

namespace helios::audio {

/// Plugin that wires up SoLoud automatically.
/// No backend knowledge needed — just add_plugin(DefaultAudioPlugin{}).
struct DefaultAudioPlugin {
    void build(auto& app) {
        app.template insert_resource<std::unique_ptr<AudioDevice>>(
            create_audio_device());
        app.add_system(helios::Schedule::PostUpdate, audio_update, "audio_update");
    }
};

} // namespace helios::audio

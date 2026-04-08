// helios-audio/src/interface/audio_factory.h
//
// Factory function that creates the best available AudioDevice backend.
// This header only exposes the base interface; backend selection happens
// inside the .cpp that the library compiles (where SoLoud headers are available).
#pragma once

#include <memory>
#include "interface/audio_device.h"

namespace helios::audio {

/// Create an AudioDevice using the best available backend.
/// Returns SoLoudDevice when HELIOS_HAS_SOLOUD, otherwise StubAudioDevice.
std::unique_ptr<AudioDevice> create_audio_device();

} // namespace helios::audio

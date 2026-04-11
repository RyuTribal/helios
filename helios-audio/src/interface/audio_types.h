#pragma once

#include <cstdint>

namespace helios::audio {

// Opaque handle to a playing sound instance. Zero means no sound / invalid.
using SoundHandle = uint64_t;

// Parameters for playing a sound.
struct PlayParams {
    float volume = 1.0f;    // 0.0 = silent, 1.0 = full volume
    bool  loop   = false;   // If true, the sound loops indefinitely
    float pitch  = 1.0f;    // Playback speed multiplier (1.0 = normal)
};

} // namespace helios::audio

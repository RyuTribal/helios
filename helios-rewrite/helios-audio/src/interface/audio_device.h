// helios-audio/src/interface/audio_device.h
#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "interface/audio_types.h"

namespace helios::audio {

// Abstract audio device interface.
//
// Implementations:
//   SoLoudDevice     -- production backend using SoLoud + miniaudio
//   StubAudioDevice  -- no-op stub for headless/test use
//
// Ownership: created by the plugin, stored as a World resource via
//   std::unique_ptr<AudioDevice>.
class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    // --- Playback ---

    // Play a sound from raw PCM/WAV data. Returns a handle to control the instance.
    virtual SoundHandle play(const void* pcm_data,
                             size_t size,
                             const PlayParams& params = {}) = 0;

    // Play a sound at a 3D world position (spatial audio).
    virtual SoundHandle play_at(const void* pcm_data,
                                size_t size,
                                const glm::vec3& position,
                                const PlayParams& params = {}) = 0;

    // Stop a playing sound immediately.
    virtual void stop(SoundHandle handle) = 0;

    // Pause a playing sound (can be resumed).
    virtual void pause(SoundHandle handle) = 0;

    // Resume a paused sound.
    virtual void resume(SoundHandle handle) = 0;

    // Check if a sound handle is currently playing.
    virtual bool is_playing(SoundHandle handle) const = 0;

    // --- Control ---

    // Set the volume of a playing sound (0.0 = silent, 1.0 = full).
    virtual void set_volume(SoundHandle handle, float volume) = 0;

    // Update the 3D position of a playing spatial sound.
    virtual void set_position(SoundHandle handle, const glm::vec3& pos) = 0;

    // Set the listener position and orientation for 3D audio.
    virtual void set_listener(const glm::vec3& position,
                              const glm::vec3& forward,
                              const glm::vec3& up) = 0;

    // --- Per-frame ---

    // Per-frame update. Processes stream buffers, updates 3D calculations.
    virtual void update() = 0;
};

} // namespace helios::audio

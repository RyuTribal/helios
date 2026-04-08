// helios-audio/src/stub/stub_audio_device.h
#pragma once

#include <unordered_map>
#include "interface/audio_device.h"

namespace helios::audio {

// No-op stub backend that implements AudioDevice without any real audio output.
// Used when SoLoud is unavailable or for headless/test environments.
class StubAudioDevice final : public AudioDevice {
public:
    StubAudioDevice();
    ~StubAudioDevice() override;

    // Non-copyable, non-movable
    StubAudioDevice(const StubAudioDevice&) = delete;
    StubAudioDevice& operator=(const StubAudioDevice&) = delete;
    StubAudioDevice(StubAudioDevice&&) = delete;
    StubAudioDevice& operator=(StubAudioDevice&&) = delete;

    // --- AudioDevice interface ---

    SoundHandle play(const void* pcm_data, size_t size,
                     const PlayParams& params = {}) override;
    SoundHandle play_at(const void* pcm_data, size_t size,
                        const glm::vec3& position,
                        const PlayParams& params = {}) override;

    void stop(SoundHandle handle) override;
    void pause(SoundHandle handle) override;
    void resume(SoundHandle handle) override;
    bool is_playing(SoundHandle handle) const override;

    void set_volume(SoundHandle handle, float volume) override;
    void set_position(SoundHandle handle, const glm::vec3& pos) override;
    void set_listener(const glm::vec3& position,
                      const glm::vec3& forward,
                      const glm::vec3& up) override;

    void update() override;

private:
    struct StubSound {
        bool playing = true;
        bool paused  = false;
        float volume = 1.0f;
    };

    std::unordered_map<SoundHandle, StubSound> m_sounds;
    SoundHandle m_next_handle = 1;
};

} // namespace helios::audio

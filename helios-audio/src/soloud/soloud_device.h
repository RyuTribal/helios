#pragma once

#include <memory>
#include <unordered_map>

#include "soloud.h"
#include "soloud_wav.h"

#include "interface/audio_device.h"

namespace helios::audio {

// Production AudioDevice implementation backed by SoLoud.
//
// RAII: constructor calls soloud.init(), destructor calls soloud.deinit().
// All SoLoud state is owned by this object. No global state.
class SoLoudDevice final : public AudioDevice {
public:
    SoLoudDevice();
    ~SoLoudDevice() override;

    // Non-copyable, non-movable (SoLoud engine is not relocatable)
    SoLoudDevice(const SoLoudDevice&) = delete;
    SoLoudDevice& operator=(const SoLoudDevice&) = delete;
    SoLoudDevice(SoLoudDevice&&) = delete;
    SoLoudDevice& operator=(SoLoudDevice&&) = delete;

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
    mutable SoLoud::Soloud m_soloud;  // mutable: SoLoud query methods aren't const

    // SoLoud::Wav objects need to stay alive while playing.
    struct ActiveSound {
        std::unique_ptr<SoLoud::Wav> wav;
        SoLoud::handle               voice_handle;
    };
    std::unordered_map<SoundHandle, ActiveSound> m_active_sounds;

    // Monotonically increasing handle counter
    SoundHandle m_next_handle = 1;
};

} // namespace helios::audio

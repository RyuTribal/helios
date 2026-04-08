// helios-audio/src/soloud/soloud_device.cpp

#include "soloud/soloud_device.h"
#include <vector>

namespace helios::audio {

SoLoudDevice::SoLoudDevice() {
    m_soloud.init();
}

SoLoudDevice::~SoLoudDevice() {
    m_soloud.stopAll();
    m_active_sounds.clear();
    m_soloud.deinit();
}

SoundHandle SoLoudDevice::play(const void* pcm_data, size_t size,
                                const PlayParams& params) {
    if (!pcm_data || size == 0) return 0;
    auto wav = std::make_unique<SoLoud::Wav>();
    wav->loadMem(
        static_cast<const unsigned char*>(pcm_data),
        static_cast<unsigned int>(size),
        /*aCopy=*/true,
        /*aTakeOwnership=*/false
    );
    wav->setLooping(params.loop);

    SoLoud::handle voice = m_soloud.play(*wav, params.volume);
    m_soloud.setRelativePlaySpeed(voice, params.pitch);

    SoundHandle handle = m_next_handle++;
    m_active_sounds[handle] = ActiveSound{std::move(wav), voice};
    return handle;
}

SoundHandle SoLoudDevice::play_at(const void* pcm_data, size_t size,
                                   const glm::vec3& position,
                                   const PlayParams& params) {
    if (!pcm_data || size == 0) return 0;
    auto wav = std::make_unique<SoLoud::Wav>();
    wav->loadMem(
        static_cast<const unsigned char*>(pcm_data),
        static_cast<unsigned int>(size),
        /*aCopy=*/true,
        /*aTakeOwnership=*/false
    );
    wav->setLooping(params.loop);

    SoLoud::handle voice = m_soloud.play3d(
        *wav,
        position.x, position.y, position.z,
        0.0f, 0.0f, 0.0f,  // velocity
        params.volume
    );
    m_soloud.setRelativePlaySpeed(voice, params.pitch);

    SoundHandle handle = m_next_handle++;
    m_active_sounds[handle] = ActiveSound{std::move(wav), voice};
    return handle;
}

void SoLoudDevice::stop(SoundHandle handle) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.stop(it->second.voice_handle);
    m_active_sounds.erase(it);
}

void SoLoudDevice::pause(SoundHandle handle) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.setPause(it->second.voice_handle, true);
}

void SoLoudDevice::resume(SoundHandle handle) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.setPause(it->second.voice_handle, false);
}

bool SoLoudDevice::is_playing(SoundHandle handle) const {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return false;

    return m_soloud.isValidVoiceHandle(it->second.voice_handle);
}

void SoLoudDevice::set_volume(SoundHandle handle, float volume) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.setVolume(it->second.voice_handle, volume);
}

void SoLoudDevice::set_position(SoundHandle handle, const glm::vec3& pos) {
    auto it = m_active_sounds.find(handle);
    if (it == m_active_sounds.end()) return;

    m_soloud.set3dSourcePosition(
        it->second.voice_handle,
        pos.x, pos.y, pos.z
    );
}

void SoLoudDevice::set_listener(const glm::vec3& position,
                                 const glm::vec3& forward,
                                 const glm::vec3& up) {
    m_soloud.set3dListenerPosition(position.x, position.y, position.z);
    m_soloud.set3dListenerAt(forward.x, forward.y, forward.z);
    m_soloud.set3dListenerUp(up.x, up.y, up.z);
}

void SoLoudDevice::update() {
    m_soloud.update3dAudio();

    // Clean up finished sounds
    std::vector<SoundHandle> finished;
    for (auto& [handle, sound] : m_active_sounds) {
        if (!m_soloud.isValidVoiceHandle(sound.voice_handle)) {
            finished.push_back(handle);
        }
    }
    for (auto handle : finished) {
        m_active_sounds.erase(handle);
    }
}

} // namespace helios::audio

// helios-audio/src/stub/stub_audio_device.cpp

#include "stub/stub_audio_device.h"

#include <cstdio>

namespace helios::audio {

namespace {
    bool s_warned_once = false;
    void warn_stub() {
        if (!s_warned_once) {
            std::fprintf(stderr,
                "[helios-audio] WARNING: Using stub audio backend. "
                "No sound will be produced.\n");
            s_warned_once = true;
        }
    }
} // anonymous namespace

StubAudioDevice::StubAudioDevice() {
    warn_stub();
}

StubAudioDevice::~StubAudioDevice() = default;

SoundHandle StubAudioDevice::play(const void* /*pcm_data*/, size_t /*size*/,
                                   const PlayParams& params) {
    SoundHandle handle = m_next_handle++;
    m_sounds[handle] = StubSound{true, false, params.volume};
    return handle;
}

SoundHandle StubAudioDevice::play_at(const void* /*pcm_data*/, size_t /*size*/,
                                      const glm::vec3& /*position*/,
                                      const PlayParams& params) {
    SoundHandle handle = m_next_handle++;
    m_sounds[handle] = StubSound{true, false, params.volume};
    return handle;
}

void StubAudioDevice::stop(SoundHandle handle) {
    m_sounds.erase(handle);
}

void StubAudioDevice::pause(SoundHandle handle) {
    auto it = m_sounds.find(handle);
    if (it != m_sounds.end()) {
        it->second.paused = true;
    }
}

void StubAudioDevice::resume(SoundHandle handle) {
    auto it = m_sounds.find(handle);
    if (it != m_sounds.end()) {
        it->second.paused = false;
    }
}

bool StubAudioDevice::is_playing(SoundHandle handle) const {
    auto it = m_sounds.find(handle);
    if (it == m_sounds.end()) return false;
    return it->second.playing && !it->second.paused;
}

void StubAudioDevice::set_volume(SoundHandle handle, float volume) {
    auto it = m_sounds.find(handle);
    if (it != m_sounds.end()) {
        it->second.volume = volume;
    }
}

void StubAudioDevice::set_position(SoundHandle /*handle*/, const glm::vec3& /*pos*/) {
    // No-op
}

void StubAudioDevice::set_listener(const glm::vec3& /*position*/,
                                    const glm::vec3& /*forward*/,
                                    const glm::vec3& /*up*/) {
    // No-op
}

void StubAudioDevice::update() {
    // No-op
}

} // namespace helios::audio

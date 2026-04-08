// helios-audio/src/interface/audio_factory.cpp

#include "interface/audio_factory.h"

#if HELIOS_HAS_SOLOUD
#include "soloud/soloud_device.h"
#else
#include "stub/stub_audio_device.h"
#endif

namespace helios::audio {

std::unique_ptr<AudioDevice> create_audio_device() {
#if HELIOS_HAS_SOLOUD
    return std::make_unique<SoLoudDevice>();
#else
    return std::make_unique<StubAudioDevice>();
#endif
}

} // namespace helios::audio

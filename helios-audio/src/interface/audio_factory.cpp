#include "interface/audio_factory.h"
#include "soloud/soloud_device.h"

namespace helios::audio {

std::unique_ptr<AudioDevice> create_audio_device() {
    return std::make_unique<SoLoudDevice>();
}

} // namespace helios::audio

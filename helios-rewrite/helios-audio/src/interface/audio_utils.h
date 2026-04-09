// helios-audio/src/interface/audio_utils.h
//
// Audio utility functions for generating test sounds.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace helios::audio {

/// Generate a short WAV file containing a sine tone with linear fade-out.
/// Useful for physics collision feedback, UI clicks, or testing.
///
/// @param freq      Frequency in Hz (default 220)
/// @param duration  Duration in seconds (default 0.1)
/// @param volume    Volume multiplier 0..1 (default 0.5)
/// @return Complete WAV file bytes (PCM 16-bit mono 44100Hz)
inline std::vector<uint8_t> generate_bounce_wav(
    float freq = 220.0f,
    float duration = 0.1f,
    float volume = 0.5f)
{
    constexpr int sample_rate = 44100;
    const int num_samples = static_cast<int>(sample_rate * duration);

    std::vector<int16_t> samples(num_samples);
    for (int i = 0; i < num_samples; i++) {
        float t = static_cast<float>(i) / sample_rate;
        float envelope = 1.0f - (t / duration);
        float sample = std::sin(2.0f * 3.14159265f * freq * t) * envelope;
        samples[i] = static_cast<int16_t>(sample * 32767.0f * volume);
    }

    uint32_t data_size = static_cast<uint32_t>(num_samples * sizeof(int16_t));
    uint32_t file_size = 36 + data_size;

    std::vector<uint8_t> wav;
    wav.resize(44 + data_size);
    auto write = [&](size_t off, const void* data, size_t n) {
        std::memcpy(wav.data() + off, data, n);
    };
    // RIFF header
    write(0, "RIFF", 4);
    write(4, &file_size, 4);
    write(8, "WAVE", 4);
    // fmt chunk
    write(12, "fmt ", 4);
    uint32_t fmt_size = 16;       write(16, &fmt_size, 4);
    uint16_t audio_fmt = 1;       write(20, &audio_fmt, 2);   // PCM
    uint16_t channels = 1;        write(22, &channels, 2);
    uint32_t sr = sample_rate;    write(24, &sr, 4);
    uint32_t byte_rate = sample_rate * 2; write(28, &byte_rate, 4);
    uint16_t block_align = 2;     write(32, &block_align, 2);
    uint16_t bits = 16;           write(34, &bits, 2);
    // data chunk
    write(36, "data", 4);
    write(40, &data_size, 4);
    std::memcpy(wav.data() + 44, samples.data(), data_size);

    return wav;
}

} // namespace helios::audio

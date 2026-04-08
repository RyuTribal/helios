#include "helios/assets/importers/audio_importer.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace helios {

AudioFormat AudioImporter::format_from_extension(const std::string& ext) {
    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == ".wav") return AudioFormat::WAV;
    if (lower == ".ogg") return AudioFormat::OGG;
    if (lower == ".mp3") return AudioFormat::MP3;
    if (lower == ".flac") return AudioFormat::FLAC;
    return AudioFormat::Unknown;
}

std::any AudioImporter::import(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Audio file not found: " + path.string());
    }

    auto file_size = std::filesystem::file_size(path);
    if (file_size == 0) {
        throw std::runtime_error("Audio file is empty: " + path.string());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open audio file: " + path.string());
    }

    AudioData data;
    data.file_bytes.resize(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(data.file_bytes.data()),
              static_cast<std::streamsize>(file_size));

    if (!file.good() && !file.eof()) {
        throw std::runtime_error("Failed to read audio file: " + path.string());
    }

    data.format = format_from_extension(path.extension().string());
    data.source_path = path.string();

    return std::any(std::move(data));
}

} // namespace helios

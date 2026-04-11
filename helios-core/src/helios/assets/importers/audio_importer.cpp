#include "helios/assets/importers/audio_importer.h"
#include "helios/assets/asset_binary.h"
#include "helios/core/engine_log_channels.h"

#include <algorithm>
#include <fstream>

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

std::any AudioImporter::import(const std::filesystem::path& path, AssetServer& /*server*/) {
    if (!std::filesystem::exists(path)) {
        HELIOS_LOG(Assets, Error, "Audio file not found: {}", path.string());
        return std::any{};
    }

    // If this is a Helios binary (.hveaudio), extract payload and determine
    // the original format from the metadata.
    if (path.extension() == ".hveaudio") {
        std::ifstream hve_file(path, std::ios::binary);
        if (!hve_file) {
            HELIOS_LOG(Assets, Error, "Failed to open Helios binary: {}", path.string());
            return std::any{};
        }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(hve_file)),
                                    std::istreambuf_iterator<char>{});

        auto result = read_asset_binary(bytes.data(), bytes.size());
        if (!result) {
            HELIOS_LOG(Assets, Error, "Invalid Helios binary: {}", path.string());
            return std::any{};
        }

        auto& reader = result->data;
        AudioData data;
        data.file_bytes.assign(reader.current(),
                               reader.current() + reader.remaining());

        // Try to recover the original format from the _source_path metadata.
        auto it = result->header.metadata.find("_source_path");
        if (it != result->header.metadata.end()) {
            std::filesystem::path src(it->second);
            data.format = format_from_extension(src.extension().string());
        } else {
            data.format = AudioFormat::Unknown;
        }
        data.source_path = path.string();
        return std::any(std::move(data));
    }

    auto file_size = std::filesystem::file_size(path);
    if (file_size == 0) {
        HELIOS_LOG(Assets, Error, "Audio file is empty: {}", path.string());
        return std::any{};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        HELIOS_LOG(Assets, Error, "Failed to open audio file: {}", path.string());
        return std::any{};
    }

    AudioData data;
    data.file_bytes.resize(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(data.file_bytes.data()),
              static_cast<std::streamsize>(file_size));

    if (!file.good() && !file.eof()) {
        HELIOS_LOG(Assets, Error, "Failed to read audio file: {}", path.string());
        return std::any{};
    }

    data.format = format_from_extension(path.extension().string());
    data.source_path = path.string();

    return std::any(std::move(data));
}

} // namespace helios

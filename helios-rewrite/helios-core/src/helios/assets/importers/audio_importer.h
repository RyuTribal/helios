#pragma once

#include <any>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace helios {

enum class AudioFormat : uint8_t {
    WAV,
    OGG,
    MP3,
    FLAC,
    Unknown
};

// Raw audio file data loaded from disk. The audio backend handles decoding.
struct AudioData {
    std::vector<uint8_t> file_bytes;  // full file contents in memory
    AudioFormat format = AudioFormat::Unknown;
    std::string source_path;

    bool is_valid() const { return !file_bytes.empty(); }
    size_t byte_size() const { return file_bytes.size(); }
};

class AudioImporter {
public:
    // Load audio file. Returns AudioData in std::any.
    static std::any import(const std::filesystem::path& path);

    // Determine format from file extension.
    static AudioFormat format_from_extension(const std::string& ext);
};

} // namespace helios

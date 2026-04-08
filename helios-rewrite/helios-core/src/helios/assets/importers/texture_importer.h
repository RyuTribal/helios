#pragma once

#include <any>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace helios {

// Raw texture data loaded from disk. GPU upload is handled by the renderer.
struct TextureData {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;           // actual channels in the file
    int desired_channels = 4;   // what we requested (always RGBA)
    std::string source_path;

    bool is_valid() const { return !pixels.empty() && width > 0 && height > 0; }
    size_t byte_size() const {
        return static_cast<size_t>(width) * height * desired_channels;
    }
};

// HDR texture data (float per channel, for environment maps / IBL).
struct HdrTextureData {
    std::vector<float> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string source_path;

    bool is_valid() const { return !pixels.empty() && width > 0 && height > 0; }
};

// Forward declaration
class AssetServer;

class TextureImporter {
public:
    // Load LDR texture (PNG, JPG, BMP, TGA). Returns TextureData in std::any.
    static std::any import_ldr(const std::filesystem::path& path, AssetServer& server);

    // Load HDR texture (HDR, EXR). Returns HdrTextureData in std::any.
    static std::any import_hdr(const std::filesystem::path& path, AssetServer& server);
};

} // namespace helios

#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/asset_binary.h"
#include "helios/core/engine_log_channels.h"

#include <any>
#include <fstream>

// stb_image implementation is compiled here
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace helios {

namespace {

// Load optimized .hvetex binary directly into TextureData.
// Format: [uint32 width] [uint32 height] [uint32 channels] [raw RGBA8 pixels]
std::any load_optimized_hvetex(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        HELIOS_LOG(Assets, Error, "Failed to open Helios binary: {}", path.string());
        return std::any{};
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>{});

    auto result = read_asset_binary(bytes.data(), bytes.size());
    if (!result) {
        HELIOS_LOG(Assets, Error, "Invalid Helios binary: {}", path.string());
        return std::any{};
    }

    auto& r = result->data;
    uint32_t w        = r.read<uint32_t>();
    uint32_t h        = r.read<uint32_t>();
    uint32_t channels = r.read<uint32_t>();

    if (w == 0 || h == 0) {
        HELIOS_LOG(Assets, Error, "Optimized texture binary has 0 dimensions: {}", path.string());
        return std::any{};
    }

    size_t pixel_bytes = static_cast<size_t>(w) * h * channels;
    TextureData data;
    data.width = static_cast<int>(w);
    data.height = static_cast<int>(h);
    data.channels = static_cast<int>(channels);
    data.desired_channels = static_cast<int>(channels);
    data.source_path = path.string();
    data.pixels.resize(pixel_bytes);
    if (!r.read_bytes(data.pixels.data(), pixel_bytes)) {
        HELIOS_LOG(Assets, Error, "Truncated pixel data in: {}", path.string());
        return std::any{};
    }

    return std::any(std::move(data));
}

// Load optimized .hvecube binary directly into HdrTextureData.
// Format: [uint32 width] [uint32 height] [uint32 channels] [raw HDR float pixels]
std::any load_optimized_hvecube(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        HELIOS_LOG(Assets, Error, "Failed to open Helios binary: {}", path.string());
        return std::any{};
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>{});

    auto result = read_asset_binary(bytes.data(), bytes.size());
    if (!result) {
        HELIOS_LOG(Assets, Error, "Invalid Helios binary: {}", path.string());
        return std::any{};
    }

    auto& r = result->data;
    uint32_t w        = r.read<uint32_t>();
    uint32_t h        = r.read<uint32_t>();
    uint32_t channels = r.read<uint32_t>();

    if (w == 0 || h == 0) {
        HELIOS_LOG(Assets, Error, "Optimized cubemap binary has 0 dimensions: {}", path.string());
        return std::any{};
    }

    size_t float_count = static_cast<size_t>(w) * h * channels;
    HdrTextureData data;
    data.width = static_cast<int>(w);
    data.height = static_cast<int>(h);
    data.channels = static_cast<int>(channels);
    data.source_path = path.string();
    data.pixels.resize(float_count);
    if (!r.read_bytes(reinterpret_cast<uint8_t*>(data.pixels.data()),
                      float_count * sizeof(float))) {
        HELIOS_LOG(Assets, Error, "Truncated HDR pixel data in: {}", path.string());
        return std::any{};
    }

    return std::any(std::move(data));
}

} // anonymous namespace

std::any TextureImporter::import_ldr(const std::filesystem::path& path, AssetServer& /*server*/) {
    if (!std::filesystem::exists(path)) {
        HELIOS_LOG(Assets, Error, "Texture file not found: {}", path.string());
        return std::any{};
    }

    // Optimized .hvetex: direct memcpy load, no stbi decoding.
    if (path.extension() == ".hvetex") {
        return load_optimized_hvetex(path);
    }

    int w, h, channels;
    constexpr int desired = 4; // Always load as RGBA
    stbi_set_flip_vertically_on_load(false);

    unsigned char* raw = stbi_load(path.string().c_str(), &w, &h, &channels,
                                   desired);
    if (!raw) {
        HELIOS_LOG(Assets, Error, "stbi_load failed for '{}': {}", path.string(), stbi_failure_reason());
        return std::any{};
    }

    TextureData data;
    data.width = w;
    data.height = h;
    data.channels = channels;
    data.desired_channels = desired;
    data.source_path = path.string();

    size_t byte_count = static_cast<size_t>(w) * h * desired;
    data.pixels.assign(raw, raw + byte_count);

    stbi_image_free(raw);

    return std::any(std::move(data));
}

std::any TextureImporter::import_hdr(const std::filesystem::path& path, AssetServer& /*server*/) {
    if (!std::filesystem::exists(path)) {
        HELIOS_LOG(Assets, Error, "HDR texture file not found: {}", path.string());
        return std::any{};
    }

    // Optimized .hvecube: direct memcpy load, no stbi decoding.
    if (path.extension() == ".hvecube") {
        return load_optimized_hvecube(path);
    }

    int w, h, channels;
    stbi_set_flip_vertically_on_load(false);

    float* raw = stbi_loadf(path.string().c_str(), &w, &h, &channels, 0);
    if (!raw) {
        HELIOS_LOG(Assets, Error, "stbi_loadf failed for '{}': {}", path.string(), stbi_failure_reason());
        return std::any{};
    }

    HdrTextureData data;
    data.width = w;
    data.height = h;
    data.channels = channels;
    data.source_path = path.string();

    size_t float_count = static_cast<size_t>(w) * h * channels;
    data.pixels.assign(raw, raw + float_count);

    stbi_image_free(raw);

    return std::any(std::move(data));
}

} // namespace helios

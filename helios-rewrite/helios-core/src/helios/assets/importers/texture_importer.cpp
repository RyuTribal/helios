#include "helios/assets/importers/texture_importer.h"

#include <any>
#include <stdexcept>

// stb_image implementation is compiled here
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace helios {

std::any TextureImporter::import_ldr(const std::filesystem::path& path, AssetServer& /*server*/) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Texture file not found: " + path.string());
    }

    int w, h, channels;
    constexpr int desired = 4; // Always load as RGBA
    stbi_set_flip_vertically_on_load(false);

    unsigned char* raw = stbi_load(path.string().c_str(), &w, &h, &channels,
                                   desired);
    if (!raw) {
        throw std::runtime_error(
            "stbi_load failed for '" + path.string() + "': " +
            stbi_failure_reason());
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
        throw std::runtime_error("HDR texture file not found: " + path.string());
    }

    int w, h, channels;
    stbi_set_flip_vertically_on_load(false);

    float* raw = stbi_loadf(path.string().c_str(), &w, &h, &channels, 0);
    if (!raw) {
        throw std::runtime_error(
            "stbi_loadf failed for '" + path.string() + "': " +
            stbi_failure_reason());
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

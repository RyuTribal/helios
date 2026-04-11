#pragma once

#include <cstdint>
#include <vector>

namespace helios {

/// CPU-side texture asset. Stores raw pixel data for GPU upload.
struct TextureAsset {
    std::vector<uint8_t> pixels;  // RGBA8 (hdr=false) or reinterpreted RGBA32F (hdr=true)
    uint32_t width = 0;
    uint32_t height = 0;
    bool hdr = false;
};

} // namespace helios

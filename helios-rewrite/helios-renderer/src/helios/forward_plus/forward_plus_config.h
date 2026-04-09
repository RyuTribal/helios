// helios-renderer/src/helios/forward_plus/forward_plus_config.h
#pragma once

#include "helios/rhi/rhi_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace helios {

/// Configuration resource for the Forward+ rendering pipeline.
/// Inserted into the World by ForwardPlusPlugin::build().
struct ForwardPlusConfig {
    rhi::TextureFormat hdr_format   = rhi::TextureFormat::RGBA16F;
    uint32_t shadow_resolution      = 4096;
    uint32_t shadow_cascades        = 4;
    rhi::TextureFormat depth_format = rhi::TextureFormat::Depth32F;
    float exposure                  = 1.0f;

    // Cascade split distances (computed from camera far plane).
    // If empty, auto-computed as geometric splits.
    std::vector<float> cascade_splits{};

    // IBL settings
    uint32_t irradiance_resolution  = 32;
    uint32_t prefilter_resolution   = 128;
    uint32_t brdf_lut_resolution    = 512;

    // Skybox HDR path (relative to asset root). Empty = no skybox.
    std::string skybox_hdr_path;

    // Tile size for light culling compute shader
    uint32_t tile_size              = 16;

    // Max light counts
    uint32_t max_point_lights       = 1024;
    uint32_t max_dir_lights         = 4;
};

} // namespace helios

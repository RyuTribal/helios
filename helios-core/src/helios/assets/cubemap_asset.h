#pragma once

#include "helios/assets/importers/texture_importer.h"
#include <cstdint>
#include <string>

namespace helios {

/// CPU-side cubemap asset. Holds source HDR data for GPU equirect-to-cube conversion.
/// GPU cubemap texture is created by GPUResourceCache::get_or_upload_cubemap().
struct CubeMapAsset {
    HdrTextureData hdr_source;
    uint32_t cubemap_resolution = 1024;
    std::string source_path;

    bool is_valid() const { return hdr_source.is_valid(); }
};

} // namespace helios

#pragma once

#include "helios/rhi/rhi_device.h"
#include "helios/rhi/rhi_texture.h"
#include "helios/pipeline_cache.h"

#include <cstdint>
#include <memory>

namespace helios {

/// Holds the three IBL textures needed for PBR environment lighting.
struct IBLTextures {
    std::unique_ptr<rhi::Texture> irradiance_map;   // Small cubemap (e.g. 32x32)
    std::unique_ptr<rhi::Texture> prefilter_map;     // Cubemap with mip chain (e.g. 128x128)
    std::unique_ptr<rhi::Texture> brdf_lut;          // 2D texture (e.g. 512x512, RG16F)
};

/// Generate all IBL textures from an environment cubemap.
/// This is a one-shot operation triggered when a skybox is loaded.
/// All three stages run as immediate compute dispatches.
IBLTextures generate_ibl(
    rhi::Device& device,
    rhi::PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t irradiance_size = 32,
    uint32_t prefilter_size  = 128,
    uint32_t brdf_size       = 512);

/// Individual stages (can be called separately if needed):

/// Irradiance convolution: environment cubemap -> small diffuse irradiance cubemap.
/// Shader: irradiance_convolution.comp (16x16x1 workgroup, z = face)
///   binding 0: samplerCube (environment), binding 1: imageCube (output)
std::unique_ptr<rhi::Texture> generate_irradiance_map(
    rhi::Device& device,
    rhi::PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t size);

/// Prefilter environment map: environment cubemap -> mipmap chain of
/// increasingly blurred specular reflections.
/// Shader: prefilter_envmap.comp (16x16x1 workgroup, z = face)
///   binding 0: samplerCube (environment), binding 1: imageCube (per-mip storage write)
///   push constants: { float roughness, uint32_t mip_size }
std::unique_ptr<rhi::Texture> generate_prefilter_map(
    rhi::Device& device,
    rhi::PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t size);

/// BRDF integration LUT: no input texture, purely mathematical.
/// Shader: brdf_lut.comp (16x16x1 workgroup)
///   binding 0: image2D (output, rg16f)
std::unique_ptr<rhi::Texture> generate_brdf_lut(
    rhi::Device& device,
    rhi::PipelineCache& cache,
    uint32_t size);

} // namespace helios

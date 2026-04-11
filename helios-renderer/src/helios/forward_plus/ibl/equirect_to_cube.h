#pragma once

#include "helios/rhi/rhi_device.h"
#include "helios/rhi/rhi_texture.h"
#include "helios/pipeline_cache.h"

#include <cstdint>

namespace helios {

/// Converts an equirectangular 2D texture to a cubemap via compute dispatch.
///
/// The output cubemap must already be created with appropriate size and format.
/// Uses immediate_submit on the device for one-shot GPU work.
///
/// Shader: equirect_to_cube.comp (16x16x1 workgroup, z = face index)
///   - binding 0: sampler2D (equirect input)
///   - binding 1: imageCube (cubemap output, storage write)
void convert_equirect_to_cube(
    rhi::Device& device,
    rhi::PipelineCache& cache,
    const rhi::Texture& equirect_input,
    rhi::Texture& cubemap_output,
    uint32_t cube_size);

} // namespace helios

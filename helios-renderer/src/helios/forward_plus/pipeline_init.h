#pragma once

#include "helios/rhi/rhi_device.h"
#include "helios/pipeline_cache.h"

namespace helios {

/// Pre-creates all Forward+ graphics and compute pipelines.
///
/// Loads SPIR-V shaders from disk via PipelineCache and creates:
///   - depth_prepass (graphics)
///   - shadow_pass (graphics, with geometry shader)
///   - light_culling (compute)
///   - forward_pass (graphics)
///   - skybox (graphics)
///   - tonemap_compute (compute)
///
/// Pipelines that fail to create (e.g. missing shader files) are logged
/// as warnings but do not prevent startup.
void initialize_forward_plus_pipelines(
    rhi::Device& device,
    rhi::PipelineCache& cache);

} // namespace helios

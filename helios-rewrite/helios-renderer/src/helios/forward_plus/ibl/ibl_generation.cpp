// helios-renderer/src/helios/forward_plus/ibl/ibl_generation.cpp
#include "helios/forward_plus/ibl/ibl_generation.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"

namespace helios {

std::unique_ptr<rhi::Texture> generate_irradiance_map(
    rhi::Device& /*device*/,
    rhi::PipelineCache& /*cache*/,
    const rhi::Texture& /*environment_cubemap*/,
    uint32_t size)
{
    // TODO: Implement when running render loop is available.
    //
    // Implementation plan:
    //   1. Create output cubemap: RGBA16F, size x size, 6 layers, 1 mip
    //   2. Load shader: "shaders/irradiance_convolution.comp.spv"
    //   3. Create compute pipeline via cache
    //   4. device.immediate_submit([&](CommandBuffer& cmd) {
    //        - Transition output UNDEFINED -> GENERAL
    //        - Bind pipeline, bind descriptors (env cubemap + output storage)
    //        - Dispatch: ceil(size/16), ceil(size/16), 6
    //        - Transition output GENERAL -> SHADER_READ_ONLY
    //      });
    HELIOS_LOG_WARN(ForwardPlus,
        "generate_irradiance_map({}): stub -- requires GPU pipeline setup", size);
    return nullptr;
}

std::unique_ptr<rhi::Texture> generate_prefilter_map(
    rhi::Device& /*device*/,
    rhi::PipelineCache& /*cache*/,
    const rhi::Texture& /*environment_cubemap*/,
    uint32_t size)
{
    // TODO: Implement when running render loop is available.
    //
    // Implementation plan:
    //   1. Compute max_mip_levels = floor(log2(size)) + 1
    //   2. Create output cubemap: RGBA16F, size x size, 6 layers, max_mip_levels mips
    //   3. Load shader: "shaders/prefilter_envmap.comp.spv"
    //   4. Create pipeline with push constants: { float roughness, uint32_t mip_size }
    //   5. Create per-mip descriptor sets with per-mip image views
    //   6. device.immediate_submit([&](CommandBuffer& cmd) {
    //        - Transition output UNDEFINED -> GENERAL
    //        - Bind pipeline
    //        - For each mip level:
    //            uint32_t mip_size = max(1, size >> mip)
    //            float roughness = float(mip) / float(max_mip_levels - 1)
    //            Push PrefilterPushConstants{roughness, mip_size}
    //            Bind per-mip descriptor
    //            Dispatch: ceil(mip_size/16), ceil(mip_size/16), 6
    //            Memory barrier between mip dispatches
    //        - Transition output GENERAL -> SHADER_READ_ONLY
    //      });
    HELIOS_LOG_WARN(ForwardPlus,
        "generate_prefilter_map({}): stub -- requires GPU pipeline setup", size);
    return nullptr;
}

std::unique_ptr<rhi::Texture> generate_brdf_lut(
    rhi::Device& /*device*/,
    rhi::PipelineCache& /*cache*/,
    uint32_t size)
{
    // TODO: Implement when running render loop is available.
    //
    // Implementation plan:
    //   1. Create output 2D texture: RG16F, size x size, 1 mip
    //   2. Load shader: "shaders/brdf_lut.comp.spv"
    //   3. Create compute pipeline via cache
    //   4. device.immediate_submit([&](CommandBuffer& cmd) {
    //        - Transition output UNDEFINED -> GENERAL
    //        - Bind pipeline + descriptors (output storage image)
    //        - Dispatch: ceil(size/16), ceil(size/16), 1
    //        - Transition output GENERAL -> SHADER_READ_ONLY
    //      });
    HELIOS_LOG_WARN(ForwardPlus,
        "generate_brdf_lut({}): stub -- requires GPU pipeline setup", size);
    return nullptr;
}

IBLTextures generate_ibl(
    rhi::Device& device,
    rhi::PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t irradiance_size,
    uint32_t prefilter_size,
    uint32_t brdf_size)
{
    HELIOS_LOG_INFO(ForwardPlus,
        "Generating IBL textures (irradiance={}x{}, prefilter={}x{}, brdf={}x{})",
        irradiance_size, irradiance_size,
        prefilter_size, prefilter_size,
        brdf_size, brdf_size);

    IBLTextures result;
    result.irradiance_map = generate_irradiance_map(
        device, cache, environment_cubemap, irradiance_size);
    result.prefilter_map = generate_prefilter_map(
        device, cache, environment_cubemap, prefilter_size);
    result.brdf_lut = generate_brdf_lut(device, cache, brdf_size);
    return result;
}

} // namespace helios

// helios-renderer/src/helios/forward_plus/ibl/ibl_generation.cpp
#include "helios/forward_plus/ibl/ibl_generation.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/rhi/rhi.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

namespace helios {

// ---------------------------------------------------------------------------
// Helper: load SPIR-V binary from the shader directory.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> load_spirv(const char* filename) {
#ifdef HELIOS_SHADER_DIR
    const char* shader_dir = HELIOS_SHADER_DIR;
#else
    const char* shader_dir = "shaders";
#endif
    std::filesystem::path path = std::filesystem::path(shader_dir) / filename;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    auto sz = file.tellg();
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

// ---------------------------------------------------------------------------
// Irradiance convolution
// ---------------------------------------------------------------------------

std::unique_ptr<rhi::Texture> generate_irradiance_map(
    rhi::Device& device,
    rhi::PipelineCache& /*cache*/,
    const rhi::Texture& environment_cubemap,
    uint32_t size)
{
    // 1. Create output cubemap: RGBA16F, size x size, 6 layers, 1 mip
    rhi::TextureDesc out_desc;
    out_desc.width        = size;
    out_desc.height       = size;
    out_desc.format       = rhi::TextureFormat::RGBA16F;
    out_desc.type         = rhi::TextureType::TextureCube;
    out_desc.mip_levels   = 1;
    out_desc.array_layers = 6;
    out_desc.usage        = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    out_desc.sampler      = rhi::SamplerMode::ClampToEdge;
    out_desc.debug_name   = "IrradianceMap";
    auto output = device.create_texture(out_desc);
    if (!output) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to create irradiance cubemap texture");
        return nullptr;
    }

    // 2. Load compute shader
    auto spirv = load_spirv("irradiance_convolution.comp.spv");
    if (spirv.empty()) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to load irradiance_convolution.comp.spv");
        return nullptr;
    }

    rhi::ShaderDesc shader_desc;
    shader_desc.stage       = rhi::ShaderStage::Compute;
    shader_desc.spirv_code  = std::move(spirv);
    shader_desc.entry_point = "main";
    shader_desc.debug_name  = "irradiance_convolution_comp";
    auto shader = device.create_shader(shader_desc);

    // 3. Descriptor layout: binding 0 = samplerCube, binding 1 = imageCube
    rhi::DescriptorSetLayoutDesc layout_desc;
    layout_desc.bindings = {
        rhi::DescriptorBinding{
            .binding = 0,
            .type    = rhi::DescriptorType::CombinedImageSampler,
            .stage   = rhi::ShaderStage::Compute,
            .count   = 1,
        },
        rhi::DescriptorBinding{
            .binding = 1,
            .type    = rhi::DescriptorType::StorageImage,
            .stage   = rhi::ShaderStage::Compute,
            .count   = 1,
        },
    };
    layout_desc.debug_name = "IrradianceConvolution_DSL";
    auto layout = device.create_descriptor_set_layout(layout_desc);

    // 4. Compute pipeline
    rhi::ComputePipelineDesc pipe_desc;
    pipe_desc.compute_shader    = shader.get();
    pipe_desc.descriptor_layouts = { layout.get() };
    pipe_desc.debug_name        = "IrradianceConvolution";
    auto pipeline = device.create_compute_pipeline(pipe_desc);

    // 5. Descriptor set
    auto ds = device.allocate_descriptor_set(*layout);
    device.update_descriptor_set(*ds, {
        rhi::DescriptorWrite{
            .binding        = 0,
            .type           = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = const_cast<rhi::Texture*>(&environment_cubemap),
        },
        rhi::DescriptorWrite{
            .binding        = 1,
            .type           = rhi::DescriptorType::StorageImage,
            .texture_handle = output.get(),
        },
    });

    // 6. Record and submit
    auto cmd = device.create_command_buffer();
    cmd->begin();
    cmd->bind_pipeline(*pipeline);
    cmd->bind_descriptor_set(0, *ds);

    uint32_t groups_x = (size + 15) / 16;
    uint32_t groups_y = (size + 15) / 16;
    cmd->dispatch(groups_x, groups_y, 6);

    // Barrier: compute writes -> fragment reads
    rhi::BarrierDesc barrier;
    barrier.src_stage = rhi::ShaderStage::Compute;
    barrier.dst_stage = rhi::ShaderStage::Fragment;
    cmd->pipeline_barrier(barrier);

    cmd->end();
    device.submit(*cmd);
    device.wait_idle();

    HELIOS_LOG_INFO(ForwardPlus, "Generated irradiance map ({}x{})", size, size);
    return output;
}

// ---------------------------------------------------------------------------
// Prefilter environment map (specular IBL with mip chain)
// ---------------------------------------------------------------------------

std::unique_ptr<rhi::Texture> generate_prefilter_map(
    rhi::Device& device,
    rhi::PipelineCache& /*cache*/,
    const rhi::Texture& environment_cubemap,
    uint32_t size)
{
    // 1. Compute mip levels
    uint32_t max_mip_levels = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(size)))) + 1;

    // 2. Create output cubemap with mip chain
    rhi::TextureDesc out_desc;
    out_desc.width        = size;
    out_desc.height       = size;
    out_desc.format       = rhi::TextureFormat::RGBA16F;
    out_desc.type         = rhi::TextureType::TextureCube;
    out_desc.mip_levels   = max_mip_levels;
    out_desc.array_layers = 6;
    out_desc.usage        = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    out_desc.sampler      = rhi::SamplerMode::ClampToEdge;
    out_desc.debug_name   = "PrefilterMap";
    auto output = device.create_texture(out_desc);
    if (!output) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to create prefilter cubemap texture");
        return nullptr;
    }

    // 3. Load compute shader
    auto spirv = load_spirv("prefilter_envmap.comp.spv");
    if (spirv.empty()) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to load prefilter_envmap.comp.spv");
        return nullptr;
    }

    rhi::ShaderDesc shader_desc;
    shader_desc.stage       = rhi::ShaderStage::Compute;
    shader_desc.spirv_code  = std::move(spirv);
    shader_desc.entry_point = "main";
    shader_desc.debug_name  = "prefilter_envmap_comp";
    auto shader = device.create_shader(shader_desc);

    // 4. Descriptor layout: binding 0 = samplerCube, binding 1 = imageCube
    rhi::DescriptorSetLayoutDesc layout_desc;
    layout_desc.bindings = {
        rhi::DescriptorBinding{
            .binding = 0,
            .type    = rhi::DescriptorType::CombinedImageSampler,
            .stage   = rhi::ShaderStage::Compute,
            .count   = 1,
        },
        rhi::DescriptorBinding{
            .binding = 1,
            .type    = rhi::DescriptorType::StorageImage,
            .stage   = rhi::ShaderStage::Compute,
            .count   = 1,
        },
    };
    layout_desc.debug_name = "PrefilterEnvmap_DSL";
    auto layout = device.create_descriptor_set_layout(layout_desc);

    // 5. Compute pipeline with push constants
    rhi::ComputePipelineDesc pipe_desc;
    pipe_desc.compute_shader      = shader.get();
    pipe_desc.descriptor_layouts  = { layout.get() };
    pipe_desc.push_constant_size  = sizeof(PrefilterPushConstants);
    pipe_desc.debug_name          = "PrefilterEnvmap";
    auto pipeline = device.create_compute_pipeline(pipe_desc);

    // 6. Record and submit -- dispatch once per mip level
    //    NOTE: Ideally each mip would have its own image view/descriptor set
    //    pointing at that specific mip level. The current RHI does not expose
    //    per-mip image views, so we bind the base-level storage image and rely
    //    on the shader writing to mip 0 coordinates scaled by mip_size. This
    //    is correct for mip 0 but approximate for higher mips; a future RHI
    //    enhancement will add per-mip view support for fully correct prefiltering.
    auto ds = device.allocate_descriptor_set(*layout);
    device.update_descriptor_set(*ds, {
        rhi::DescriptorWrite{
            .binding        = 0,
            .type           = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = const_cast<rhi::Texture*>(&environment_cubemap),
        },
        rhi::DescriptorWrite{
            .binding        = 1,
            .type           = rhi::DescriptorType::StorageImage,
            .texture_handle = output.get(),
        },
    });

    auto cmd = device.create_command_buffer();
    cmd->begin();
    cmd->bind_pipeline(*pipeline);
    cmd->bind_descriptor_set(0, *ds);

    for (uint32_t mip = 0; mip < max_mip_levels; ++mip) {
        uint32_t mip_size = std::max(1u, size >> mip);
        float roughness = (max_mip_levels > 1)
            ? static_cast<float>(mip) / static_cast<float>(max_mip_levels - 1)
            : 0.0f;

        PrefilterPushConstants pc;
        pc.roughness = roughness;
        pc.mip_size  = mip_size;
        cmd->push_constants(rhi::ShaderStage::Compute, 0,
                            sizeof(PrefilterPushConstants), &pc);

        uint32_t groups_x = (mip_size + 15) / 16;
        uint32_t groups_y = (mip_size + 15) / 16;
        cmd->dispatch(groups_x, groups_y, 6);

        // Memory barrier between mip dispatches
        if (mip + 1 < max_mip_levels) {
            rhi::BarrierDesc mip_barrier;
            mip_barrier.src_stage = rhi::ShaderStage::Compute;
            mip_barrier.dst_stage = rhi::ShaderStage::Compute;
            cmd->pipeline_barrier(mip_barrier);
        }
    }

    // Final barrier: compute writes -> fragment reads
    rhi::BarrierDesc barrier;
    barrier.src_stage = rhi::ShaderStage::Compute;
    barrier.dst_stage = rhi::ShaderStage::Fragment;
    cmd->pipeline_barrier(barrier);

    cmd->end();
    device.submit(*cmd);
    device.wait_idle();

    HELIOS_LOG_INFO(ForwardPlus, "Generated prefilter map ({}x{}, {} mips)",
                    size, size, max_mip_levels);
    return output;
}

// ---------------------------------------------------------------------------
// BRDF integration LUT
// ---------------------------------------------------------------------------

std::unique_ptr<rhi::Texture> generate_brdf_lut(
    rhi::Device& device,
    rhi::PipelineCache& /*cache*/,
    uint32_t size)
{
    // 1. Create output 2D texture: RG16F
    rhi::TextureDesc out_desc;
    out_desc.width        = size;
    out_desc.height       = size;
    out_desc.format       = rhi::TextureFormat::RG16F;
    out_desc.type         = rhi::TextureType::Texture2D;
    out_desc.mip_levels   = 1;
    out_desc.array_layers = 1;
    out_desc.usage        = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    out_desc.sampler      = rhi::SamplerMode::ClampToEdge;
    out_desc.debug_name   = "BrdfLUT";
    auto output = device.create_texture(out_desc);
    if (!output) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to create BRDF LUT texture");
        return nullptr;
    }

    // 2. Load compute shader
    auto spirv = load_spirv("brdf_lut.comp.spv");
    if (spirv.empty()) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to load brdf_lut.comp.spv");
        return nullptr;
    }

    rhi::ShaderDesc shader_desc;
    shader_desc.stage       = rhi::ShaderStage::Compute;
    shader_desc.spirv_code  = std::move(spirv);
    shader_desc.entry_point = "main";
    shader_desc.debug_name  = "brdf_lut_comp";
    auto shader = device.create_shader(shader_desc);

    // 3. Descriptor layout: binding 0 = image2D (storage write)
    rhi::DescriptorSetLayoutDesc layout_desc;
    layout_desc.bindings = {
        rhi::DescriptorBinding{
            .binding = 0,
            .type    = rhi::DescriptorType::StorageImage,
            .stage   = rhi::ShaderStage::Compute,
            .count   = 1,
        },
    };
    layout_desc.debug_name = "BrdfLut_DSL";
    auto layout = device.create_descriptor_set_layout(layout_desc);

    // 4. Compute pipeline
    rhi::ComputePipelineDesc pipe_desc;
    pipe_desc.compute_shader    = shader.get();
    pipe_desc.descriptor_layouts = { layout.get() };
    pipe_desc.debug_name        = "BrdfLut";
    auto pipeline = device.create_compute_pipeline(pipe_desc);

    // 5. Descriptor set
    auto ds = device.allocate_descriptor_set(*layout);
    device.update_descriptor_set(*ds, {
        rhi::DescriptorWrite{
            .binding        = 0,
            .type           = rhi::DescriptorType::StorageImage,
            .texture_handle = output.get(),
        },
    });

    // 6. Record and submit
    auto cmd = device.create_command_buffer();
    cmd->begin();
    cmd->bind_pipeline(*pipeline);
    cmd->bind_descriptor_set(0, *ds);

    uint32_t groups_x = (size + 15) / 16;
    uint32_t groups_y = (size + 15) / 16;
    cmd->dispatch(groups_x, groups_y, 1);

    // Barrier: compute writes -> fragment reads
    rhi::BarrierDesc barrier;
    barrier.src_stage = rhi::ShaderStage::Compute;
    barrier.dst_stage = rhi::ShaderStage::Fragment;
    cmd->pipeline_barrier(barrier);

    cmd->end();
    device.submit(*cmd);
    device.wait_idle();

    HELIOS_LOG_INFO(ForwardPlus, "Generated BRDF LUT ({}x{})", size, size);
    return output;
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

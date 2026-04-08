// sandbox/asset_loader.cpp
// Utility loaders for the PBR sandbox demo.
// Contains STB_IMAGE_IMPLEMENTATION for HDR texture loading.
// The mesh loading has moved to helios-core's MeshImporter.

#include "stb_image.h"

#include "asset_loader.h"

#include <helios/rhi/rhi_device.h>
#include <helios/rhi/rhi_types.h>
#include <helios/core/log_macros.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>

HELIOS_DEFINE_LOG_CHANNEL(AssetLoader);

namespace sandbox {

// ---------------------------------------------------------------------------
// Helper: read SPIR-V from disk
// ---------------------------------------------------------------------------

std::vector<uint8_t> read_spirv(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    auto sz = file.tellg();
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

std::vector<glm::vec3> build_skybox_cube() {
    return {
        {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{ 1, 1, 1},{-1, 1, 1},{-1,-1, 1},
        { 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1},
        { 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1,-1},{ 1, 1, 1},{ 1,-1, 1},
        {-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1, 1},{-1, 1,-1},{-1,-1,-1},
        {-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{ 1, 1,-1},{-1, 1,-1},{-1, 1, 1},
        {-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{ 1,-1, 1},{-1,-1, 1},{-1,-1,-1},
    };
}

// ---------------------------------------------------------------------------
// load_hdr_texture
// ---------------------------------------------------------------------------

std::unique_ptr<helios::rhi::Texture> load_hdr_texture(
    helios::rhi::Device& device, const char* path, const char* debug_name)
{
    int w, h, channels;
    float* pixels = stbi_loadf(path, &w, &h, &channels, 4);
    if (!pixels) {
        HELIOS_LOG(AssetLoader, Error, "Failed to load HDR texture: {}", path);
        return nullptr;
    }

    helios::rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(w);
    desc.height = static_cast<uint32_t>(h);
    desc.format = helios::rhi::TextureFormat::RGBA32F;
    desc.type = helios::rhi::TextureType::Texture2D;
    desc.mip_levels = 1;
    desc.array_layers = 1;
    desc.usage = helios::rhi::TextureUsage::Sampled;
    desc.sampler = helios::rhi::SamplerMode::ClampToEdge;
    desc.debug_name = debug_name;

    auto tex = device.create_texture(desc, pixels);
    stbi_image_free(pixels);

    HELIOS_LOG(AssetLoader, Info, "Loaded HDR texture '{}': {}x{}", debug_name, w, h);
    return tex;
}

// ---------------------------------------------------------------------------
// convert_equirect_to_cubemap
// ---------------------------------------------------------------------------

std::unique_ptr<helios::rhi::Texture> convert_equirect_to_cubemap(
    helios::rhi::Device& device, helios::rhi::CommandBuffer& cmd,
    const helios::rhi::Texture& equirect, uint32_t cube_size)
{
    // 1. Create output cubemap
    helios::rhi::TextureDesc cube_desc;
    cube_desc.width = cube_size;
    cube_desc.height = cube_size;
    cube_desc.format = helios::rhi::TextureFormat::RGBA16F;
    cube_desc.type = helios::rhi::TextureType::TextureCube;
    cube_desc.mip_levels = 1;
    cube_desc.array_layers = 6;
    cube_desc.usage = helios::rhi::TextureUsage::Sampled | helios::rhi::TextureUsage::Storage;
    cube_desc.sampler = helios::rhi::SamplerMode::ClampToEdge;
    cube_desc.debug_name = "EnvCubemap";

    auto cubemap = device.create_texture(cube_desc);
    if (!cubemap) {
        HELIOS_LOG(AssetLoader, Error, "Failed to create cubemap texture");
        return nullptr;
    }

    // 2. Load compute shader
#ifdef HELIOS_SHADER_DIR
    const char* shader_dir = HELIOS_SHADER_DIR;
#else
    const char* shader_dir = "shaders";
#endif

    auto comp_spirv = read_spirv(std::filesystem::path(shader_dir) / "equirect_to_cube.comp.spv");
    if (comp_spirv.empty()) {
        HELIOS_LOG(AssetLoader, Error, "Failed to load equirect_to_cube.comp.spv");
        return nullptr;
    }

    helios::rhi::ShaderDesc shader_desc;
    shader_desc.stage = helios::rhi::ShaderStage::Compute;
    shader_desc.spirv_code = std::move(comp_spirv);
    shader_desc.entry_point = "main";
    shader_desc.debug_name = "equirect_to_cube_comp";
    auto comp_shader = device.create_shader(shader_desc);

    // 3. Create descriptor layout
    helios::rhi::DescriptorSetLayoutDesc layout_desc;
    layout_desc.bindings = {
        helios::rhi::DescriptorBinding{
            .binding = 0,
            .type = helios::rhi::DescriptorType::CombinedImageSampler,
            .stage = helios::rhi::ShaderStage::Compute,
            .count = 1,
        },
        helios::rhi::DescriptorBinding{
            .binding = 1,
            .type = helios::rhi::DescriptorType::StorageImage,
            .stage = helios::rhi::ShaderStage::Compute,
            .count = 1,
        },
    };
    layout_desc.debug_name = "EquirectToCube_DSL";
    auto layout = device.create_descriptor_set_layout(layout_desc);

    // 4. Create compute pipeline
    helios::rhi::ComputePipelineDesc pipe_desc;
    pipe_desc.compute_shader = comp_shader.get();
    pipe_desc.descriptor_layouts = { layout.get() };
    pipe_desc.debug_name = "EquirectToCube";
    auto pipeline = device.create_compute_pipeline(pipe_desc);

    // 5. Allocate and write descriptor set
    auto ds = device.allocate_descriptor_set(*layout);
    device.update_descriptor_set(*ds, {
        helios::rhi::DescriptorWrite{
            .binding = 0,
            .type = helios::rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = const_cast<helios::rhi::Texture*>(&equirect),
        },
        helios::rhi::DescriptorWrite{
            .binding = 1,
            .type = helios::rhi::DescriptorType::StorageImage,
            .texture_handle = cubemap.get(),
        },
    });

    // 6. Record and submit
    cmd.begin();
    cmd.bind_pipeline(*pipeline);
    cmd.bind_descriptor_set(0, *ds);

    uint32_t groups_x = (cube_size + 15) / 16;
    uint32_t groups_y = (cube_size + 15) / 16;
    cmd.dispatch(groups_x, groups_y, 6);

    // Barrier: compute writes -> fragment reads
    helios::rhi::BarrierDesc barrier;
    barrier.src_stage = helios::rhi::ShaderStage::Compute;
    barrier.dst_stage = helios::rhi::ShaderStage::Fragment;
    cmd.pipeline_barrier(barrier);

    cmd.end();
    device.submit(cmd);
    device.wait_idle();

    HELIOS_LOG(AssetLoader, Info, "Converted equirectangular to {}x{} cubemap", cube_size, cube_size);
    return cubemap;
}

} // namespace sandbox

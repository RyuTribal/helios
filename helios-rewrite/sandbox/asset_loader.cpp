// sandbox/asset_loader.cpp
// One-off asset loaders for the PBR helmet demo.
// Contains STB_IMAGE_IMPLEMENTATION and CGLTF_IMPLEMENTATION.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

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

static std::vector<uint8_t> read_spirv(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    auto sz = file.tellg();
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

// ---------------------------------------------------------------------------
// load_gltf_mesh
// ---------------------------------------------------------------------------

LoadedMesh load_gltf_mesh(helios::rhi::Device& device, const char* gltf_path) {
    LoadedMesh result;

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result parse_result = cgltf_parse_file(&options, gltf_path, &data);
    if (parse_result != cgltf_result_success) {
        HELIOS_LOG(AssetLoader, Error, "Failed to parse glTF: {}", gltf_path);
        return result;
    }

    cgltf_result buf_result = cgltf_load_buffers(&options, data, gltf_path);
    if (buf_result != cgltf_result_success) {
        HELIOS_LOG(AssetLoader, Error, "Failed to load glTF buffers: {}", gltf_path);
        cgltf_free(data);
        return result;
    }

    if (data->meshes_count == 0 || data->meshes[0].primitives_count == 0) {
        HELIOS_LOG(AssetLoader, Error, "No meshes found in glTF: {}", gltf_path);
        cgltf_free(data);
        return result;
    }

    const cgltf_primitive& prim = data->meshes[0].primitives[0];

    // Find vertex count from position accessor
    const cgltf_accessor* pos_accessor = nullptr;
    const cgltf_accessor* normal_accessor = nullptr;
    const cgltf_accessor* uv_accessor = nullptr;
    const cgltf_accessor* tangent_accessor = nullptr;

    for (cgltf_size i = 0; i < prim.attributes_count; i++) {
        const cgltf_attribute& attr = prim.attributes[i];
        switch (attr.type) {
            case cgltf_attribute_type_position: pos_accessor = attr.data; break;
            case cgltf_attribute_type_normal:   normal_accessor = attr.data; break;
            case cgltf_attribute_type_texcoord: uv_accessor = attr.data; break;
            case cgltf_attribute_type_tangent:  tangent_accessor = attr.data; break;
            default: break;
        }
    }

    if (!pos_accessor) {
        HELIOS_LOG(AssetLoader, Error, "No position attribute in glTF mesh");
        cgltf_free(data);
        return result;
    }

    cgltf_size vertex_count = pos_accessor->count;
    std::vector<PBRVertex> vertices(vertex_count);

    // Read positions
    for (cgltf_size i = 0; i < vertex_count; i++) {
        cgltf_accessor_read_float(pos_accessor, i, &vertices[i].position.x, 3);
    }

    // Read normals
    if (normal_accessor) {
        for (cgltf_size i = 0; i < vertex_count; i++) {
            cgltf_accessor_read_float(normal_accessor, i, &vertices[i].normal.x, 3);
        }
    } else {
        for (cgltf_size i = 0; i < vertex_count; i++) {
            vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    // Read UVs
    if (uv_accessor) {
        for (cgltf_size i = 0; i < vertex_count; i++) {
            cgltf_accessor_read_float(uv_accessor, i, &vertices[i].uv.x, 2);
        }
    }

    // Read tangents
    if (tangent_accessor) {
        for (cgltf_size i = 0; i < vertex_count; i++) {
            cgltf_accessor_read_float(tangent_accessor, i, &vertices[i].tangent.x, 4);
        }
    } else {
        // Default tangent
        for (cgltf_size i = 0; i < vertex_count; i++) {
            vertices[i].tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    // Read indices
    std::vector<uint32_t> indices;
    if (prim.indices) {
        indices.resize(prim.indices->count);
        for (cgltf_size i = 0; i < prim.indices->count; i++) {
            indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i));
        }
    }

    cgltf_free(data);

    // Create GPU buffers
    helios::rhi::BufferDesc vbo_desc;
    vbo_desc.size = static_cast<uint32_t>(vertices.size() * sizeof(PBRVertex));
    vbo_desc.usage = helios::rhi::BufferUsage::Vertex;
    vbo_desc.access = helios::rhi::MemoryAccess::CPU_to_GPU;
    vbo_desc.debug_name = "HelmetVBO";
    result.vbo = device.create_buffer(vbo_desc, vertices.data());

    helios::rhi::BufferDesc ibo_desc;
    ibo_desc.size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
    ibo_desc.usage = helios::rhi::BufferUsage::Index;
    ibo_desc.access = helios::rhi::MemoryAccess::CPU_to_GPU;
    ibo_desc.debug_name = "HelmetIBO";
    result.ibo = device.create_buffer(ibo_desc, indices.data());
    result.index_count = static_cast<uint32_t>(indices.size());

    HELIOS_LOG(AssetLoader, Info, "Loaded glTF mesh: {} vertices, {} indices",
               vertex_count, indices.size());
    return result;
}

// ---------------------------------------------------------------------------
// load_texture_2d
// ---------------------------------------------------------------------------

std::unique_ptr<helios::rhi::Texture> load_texture_2d(
    helios::rhi::Device& device, const char* path, const char* debug_name)
{
    int w, h, channels;
    unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
    if (!pixels) {
        HELIOS_LOG(AssetLoader, Error, "Failed to load texture: {}", path);
        return nullptr;
    }

    helios::rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(w);
    desc.height = static_cast<uint32_t>(h);
    desc.format = helios::rhi::TextureFormat::RGBA8;
    desc.type = helios::rhi::TextureType::Texture2D;
    desc.mip_levels = 1;
    desc.array_layers = 1;
    desc.usage = helios::rhi::TextureUsage::Sampled;
    desc.debug_name = debug_name;

    auto tex = device.create_texture(desc, pixels);
    stbi_image_free(pixels);

    HELIOS_LOG(AssetLoader, Info, "Loaded texture '{}': {}x{}", debug_name, w, h);
    return tex;
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

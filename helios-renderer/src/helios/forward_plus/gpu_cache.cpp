#include "helios/forward_plus/gpu_cache.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/ibl/equirect_to_cube.h"
#include "helios/pipeline_cache.h"

#include <cstring>

namespace helios {

const GPUMesh* GPUResourceCache::get_or_upload_mesh(
    AssetHandle handle, const MeshAsset& asset, rhi::Device& device)
{
    uint64_t key = handle.packed();
    auto it = m_meshes.find(key);
    if (it != m_meshes.end()) {
        return &it->second;
    }

    if (asset.vertices.empty() || asset.indices.empty()) {
        return nullptr;
    }

    GPUMesh gpu;

    // Vertex buffer
    rhi::BufferDesc vbo_desc;
    vbo_desc.size = static_cast<uint32_t>(asset.vertices.size() * sizeof(PBRVertex));
    vbo_desc.usage = rhi::BufferUsage::Vertex;
    vbo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
    vbo_desc.debug_name = "MeshVBO_" + std::to_string(handle.index);
    gpu.vbo = device.create_buffer(vbo_desc, asset.vertices.data());

    // Index buffer
    rhi::BufferDesc ibo_desc;
    ibo_desc.size = static_cast<uint32_t>(asset.indices.size() * sizeof(uint32_t));
    ibo_desc.usage = rhi::BufferUsage::Index;
    ibo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
    ibo_desc.debug_name = "MeshIBO_" + std::to_string(handle.index);
    gpu.ibo = device.create_buffer(ibo_desc, asset.indices.data());

    gpu.index_count = static_cast<uint32_t>(asset.indices.size());

    HELIOS_LOG_TRACE(ForwardPlus, "Uploaded mesh GPU buffers: {} verts, {} indices (handle {}/{})",
                     asset.vertices.size(), asset.indices.size(),
                     handle.index, handle.generation);

    auto [inserted, _] = m_meshes.emplace(key, std::move(gpu));
    return &inserted->second;
}

rhi::Texture* GPUResourceCache::get_or_upload_texture(
    AssetHandle handle, const TextureAsset& asset, rhi::Device& device)
{
    uint64_t key = handle.packed();
    auto it = m_textures.find(key);
    if (it != m_textures.end()) {
        return it->second.get();
    }

    if (asset.pixels.empty() || asset.width == 0 || asset.height == 0) {
        return nullptr;
    }

    rhi::TextureDesc desc;
    desc.width = asset.width;
    desc.height = asset.height;
    desc.format = asset.hdr ? rhi::TextureFormat::RGBA32F : rhi::TextureFormat::RGBA8;
    desc.type = rhi::TextureType::Texture2D;
    desc.mip_levels = 1;
    desc.array_layers = 1;
    desc.usage = rhi::TextureUsage::Sampled;
    desc.debug_name = "Texture_" + std::to_string(handle.index);

    auto tex = device.create_texture(desc, asset.pixels.data());
    if (!tex) return nullptr;

    HELIOS_LOG_TRACE(ForwardPlus, "Uploaded texture {}x{} (handle {}/{})",
                     asset.width, asset.height, handle.index, handle.generation);

    auto* raw = tex.get();
    m_textures.emplace(key, std::move(tex));
    return raw;
}

const GPUMaterial* GPUResourceCache::get_or_upload_material(
    AssetHandle handle,
    const MaterialAsset& asset,
    rhi::Device& device,
    AssetServer& server,
    rhi::DescriptorSetLayout& layout,
    const rhi::Texture& default_white,
    const rhi::Texture& default_blue,
    const rhi::Texture& default_black,
    rhi::Texture* env_cubemap)
{
    uint64_t key = handle.packed();
    auto it = m_materials.find(key);
    if (it != m_materials.end()) {
        return &it->second;
    }

    // Resolve textures: use uploaded GPU texture or fall back to defaults
    auto resolve_texture = [&](Handle<TextureAsset> tex_handle,
                               const rhi::Texture& fallback) -> rhi::Texture* {
        if (!tex_handle) return const_cast<rhi::Texture*>(&fallback);
        auto untyped = tex_handle.untyped();
        const auto* tex_asset = server.get<TextureAsset>(untyped);
        if (!tex_asset) return const_cast<rhi::Texture*>(&fallback);
        auto* gpu_tex = get_or_upload_texture(untyped, *tex_asset, device);
        if (!gpu_tex) return const_cast<rhi::Texture*>(&fallback);
        return gpu_tex;
    };

    rhi::Texture* albedo_tex = resolve_texture(asset.albedo, default_white);
    rhi::Texture* normal_tex = resolve_texture(asset.normal, default_blue);

    // For MR: if no texture, bake the material's scalar factors into a 1x1 pixel
    // (G = roughness, B = metallic) instead of defaulting to white (full metallic)
    rhi::Texture* mr_tex = nullptr;
    if (asset.metallic_roughness) {
        mr_tex = resolve_texture(asset.metallic_roughness, default_white);
    } else {
        // Synthetic 1x1 metallic-roughness texture keyed by material handle.
        // Stored in a separate map to avoid key collisions with real textures.
        auto mr_it = m_synthetic_textures.find(key);
        if (mr_it != m_synthetic_textures.end()) {
            mr_tex = mr_it->second.get();
        } else {
            uint8_t mr_pixel[4] = {
                0,                                                         // R (unused)
                static_cast<uint8_t>(asset.roughness * 255.0f),           // G = roughness
                static_cast<uint8_t>(asset.metallic * 255.0f),            // B = metallic
                255                                                        // A
            };
            rhi::TextureDesc mr_desc;
            mr_desc.width = 1; mr_desc.height = 1;
            mr_desc.format = rhi::TextureFormat::RGBA8;
            mr_desc.usage = rhi::TextureUsage::Sampled;
            mr_desc.debug_name = "MR_synthetic_" + std::to_string(handle.index);
            auto tex = device.create_texture(mr_desc, mr_pixel);
            mr_tex = tex.get();
            m_synthetic_textures[key] = std::move(tex);
        }
    }

    rhi::Texture* emissive_tex = resolve_texture(asset.emissive, default_black);
    rhi::Texture* env_tex = env_cubemap ? env_cubemap : const_cast<rhi::Texture*>(&default_white);

    GPUMaterial gpu;
    gpu.ds = device.allocate_descriptor_set(layout);

    device.update_descriptor_set(*gpu.ds, {
        rhi::DescriptorWrite{
            .binding = 0,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = albedo_tex,
        },
        rhi::DescriptorWrite{
            .binding = 1,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = normal_tex,
        },
        rhi::DescriptorWrite{
            .binding = 2,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = mr_tex,
        },
        rhi::DescriptorWrite{
            .binding = 3,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = emissive_tex,
        },
        rhi::DescriptorWrite{
            .binding = 4,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = env_tex,
        },
    });

    HELIOS_LOG_TRACE(ForwardPlus, "Created GPU material descriptor set (handle {}/{})",
                     handle.index, handle.generation);

    auto [inserted, _] = m_materials.emplace(key, std::move(gpu));
    return &inserted->second;
}

rhi::Texture* GPUResourceCache::get_or_create_default_white(rhi::Device& device) {
    if (!m_default_white) {
        uint32_t data = 0xFFFFFFFF;
        rhi::TextureDesc desc{
            .width = 1, .height = 1,
            .format = rhi::TextureFormat::RGBA8,
            .type = rhi::TextureType::Texture2D,
            .mip_levels = 1,
            .usage = rhi::TextureUsage::Sampled,
            .debug_name = "GPUCache_DefaultWhite",
        };
        m_default_white = device.create_texture(desc, &data);
    }
    return m_default_white.get();
}

rhi::Texture* GPUResourceCache::get_or_create_default_blue(rhi::Device& device) {
    if (!m_default_blue) {
        uint32_t data = 0xFFFF8080; // ABGR: flat normal (128,128,255,255)
        rhi::TextureDesc desc{
            .width = 1, .height = 1,
            .format = rhi::TextureFormat::RGBA8,
            .type = rhi::TextureType::Texture2D,
            .mip_levels = 1,
            .usage = rhi::TextureUsage::Sampled,
            .debug_name = "GPUCache_DefaultBlue",
        };
        m_default_blue = device.create_texture(desc, &data);
    }
    return m_default_blue.get();
}

rhi::Texture* GPUResourceCache::get_or_create_default_black(rhi::Device& device) {
    if (!m_default_black) {
        uint32_t data = 0xFF000000; // ABGR: black (0,0,0,255)
        rhi::TextureDesc desc{
            .width = 1, .height = 1,
            .format = rhi::TextureFormat::RGBA8,
            .type = rhi::TextureType::Texture2D,
            .mip_levels = 1,
            .usage = rhi::TextureUsage::Sampled,
            .debug_name = "GPUCache_DefaultBlack",
        };
        m_default_black = device.create_texture(desc, &data);
    }
    return m_default_black.get();
}

void GPUResourceCache::evict_unused(const AssetServer& server, rhi::Device& device) {
    // All evictions route through defer_destroy so GPU resources aren't freed
    // while in-flight command buffers still reference them.

    for (auto it = m_meshes.begin(); it != m_meshes.end(); ) {
        if (!server.is_loaded(AssetHandle::from_packed(it->first))) {
            if (it->second.vbo) device.defer_destroy(std::move(it->second.vbo));
            if (it->second.ibo) device.defer_destroy(std::move(it->second.ibo));
            it = m_meshes.erase(it);
        } else { ++it; }
    }

    for (auto it = m_textures.begin(); it != m_textures.end(); ) {
        if (!server.is_loaded(AssetHandle::from_packed(it->first))) {
            device.defer_destroy(std::move(it->second));
            it = m_textures.erase(it);
        } else { ++it; }
    }

    for (auto it = m_materials.begin(); it != m_materials.end(); ) {
        if (!server.is_loaded(AssetHandle::from_packed(it->first))) {
            if (it->second.ds) device.defer_destroy(std::move(it->second.ds));
            it = m_materials.erase(it);
        } else { ++it; }
    }

    for (auto it = m_cubemaps.begin(); it != m_cubemaps.end(); ) {
        if (!server.is_loaded(AssetHandle::from_packed(it->first))) {
            device.defer_destroy(std::move(it->second));
            it = m_cubemaps.erase(it);
        } else { ++it; }
    }

    for (auto it = m_synthetic_textures.begin(); it != m_synthetic_textures.end(); ) {
        if (!server.is_loaded(AssetHandle::from_packed(it->first))) {
            device.defer_destroy(std::move(it->second));
            it = m_synthetic_textures.erase(it);
        } else { ++it; }
    }
}

rhi::Texture* GPUResourceCache::get_or_upload_cubemap(
    AssetHandle handle, const CubeMapAsset& asset, rhi::Device& device)
{
    uint64_t key = handle.packed();
    auto it = m_cubemaps.find(key);
    if (it != m_cubemaps.end()) return it->second.get();

    if (!asset.is_valid()) return nullptr;

    const auto& hdr = asset.hdr_source;

    // Expand 3-channel HDR to RGBA32F
    int total_pixels = hdr.width * hdr.height;
    std::vector<float> rgba(static_cast<size_t>(total_pixels) * 4);
    const float* src = hdr.pixels.data();
    if (hdr.channels == 4) {
        std::memcpy(rgba.data(), src, rgba.size() * sizeof(float));
    } else if (hdr.channels == 3) {
        for (int i = 0; i < total_pixels; ++i) {
            rgba[i * 4 + 0] = src[i * 3 + 0];
            rgba[i * 4 + 1] = src[i * 3 + 1];
            rgba[i * 4 + 2] = src[i * 3 + 2];
            rgba[i * 4 + 3] = 1.0f;
        }
    } else {
        return nullptr;
    }

    // Upload equirect as 2D GPU texture
    rhi::TextureDesc eq_desc;
    eq_desc.width = static_cast<uint32_t>(hdr.width);
    eq_desc.height = static_cast<uint32_t>(hdr.height);
    eq_desc.format = rhi::TextureFormat::RGBA32F;
    eq_desc.usage = rhi::TextureUsage::Sampled;
    eq_desc.sampler = rhi::SamplerMode::ClampToEdge;
    eq_desc.debug_name = "CubemapEquirect";
    auto equirect = device.create_texture(eq_desc, rgba.data());
    if (!equirect) return nullptr;

    // Create output cubemap
    rhi::TextureDesc cube_desc;
    cube_desc.width = asset.cubemap_resolution;
    cube_desc.height = asset.cubemap_resolution;
    cube_desc.format = rhi::TextureFormat::RGBA16F;
    cube_desc.type = rhi::TextureType::TextureCube;
    cube_desc.mip_levels = 1;
    cube_desc.array_layers = 6;
    cube_desc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    cube_desc.sampler = rhi::SamplerMode::ClampToEdge;
    cube_desc.debug_name = "EnvCubemap";
    auto cubemap = device.create_texture(cube_desc);
    if (!cubemap) return nullptr;

    // GPU compute: equirect -> cube
    rhi::PipelineCache pipe_cache(device);
    convert_equirect_to_cube(device, pipe_cache, *equirect, *cubemap,
                              asset.cubemap_resolution);

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Uploaded cubemap {}x{} from {}x{} equirect (handle {}/{})",
                     asset.cubemap_resolution, asset.cubemap_resolution,
                     hdr.width, hdr.height, handle.index, handle.generation);

    auto* ptr = cubemap.get();
    m_cubemaps[key] = std::move(cubemap);
    return ptr;
}

rhi::Texture* GPUResourceCache::get_or_create_default_normal(rhi::Device& device) {
    if (m_default_normal) return m_default_normal.get();
    uint8_t pixels[4] = {128, 128, 255, 255};
    rhi::TextureDesc desc;
    desc.width = 1;
    desc.height = 1;
    desc.format = rhi::TextureFormat::RGBA8;
    desc.usage = rhi::TextureUsage::Sampled;
    desc.debug_name = "DefaultNormal";
    m_default_normal = device.create_texture(desc, pixels);
    return m_default_normal.get();
}

} // namespace helios

// helios-renderer/src/helios/forward_plus/gpu_cache.cpp
#include "helios/forward_plus/gpu_cache.h"
#include "helios/forward_plus/forward_plus_log_channel.h"

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
    rhi::Texture* mr_tex = resolve_texture(asset.metallic_roughness, default_white);
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

void GPUResourceCache::evict_unused(const AssetServer& server) {
    // Evict meshes
    for (auto it = m_meshes.begin(); it != m_meshes.end(); ) {
        auto handle = AssetHandle::from_packed(it->first);
        if (!server.is_loaded(handle)) {
            HELIOS_LOG_TRACE(ForwardPlus, "Evicting GPU mesh (handle {}/{})",
                             handle.index, handle.generation);
            it = m_meshes.erase(it);
        } else {
            ++it;
        }
    }

    // Evict textures
    for (auto it = m_textures.begin(); it != m_textures.end(); ) {
        auto handle = AssetHandle::from_packed(it->first);
        if (!server.is_loaded(handle)) {
            HELIOS_LOG_TRACE(ForwardPlus, "Evicting GPU texture (handle {}/{})",
                             handle.index, handle.generation);
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }

    // Evict materials
    for (auto it = m_materials.begin(); it != m_materials.end(); ) {
        auto handle = AssetHandle::from_packed(it->first);
        if (!server.is_loaded(handle)) {
            HELIOS_LOG_TRACE(ForwardPlus, "Evicting GPU material (handle {}/{})",
                             handle.index, handle.generation);
            it = m_materials.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace helios

// helios-renderer/src/helios/forward_plus/gpu_cache.h
//
// GPU resource cache: lazily uploads CPU-side assets (MeshAsset, TextureAsset,
// MaterialAsset) to the GPU on first use. Keyed by AssetHandle packed ID.
#pragma once

#include "helios/assets/mesh_asset.h"
#include "helios/assets/material_asset.h"
#include "helios/assets/texture_asset.h"
#include "helios/assets/asset_server.h"
#include "helios/ecs/asset_handle.h"
#include "helios/rhi/rhi.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace helios {

/// Cached GPU representation of a mesh: vertex buffer + index buffer.
struct GPUMesh {
    std::unique_ptr<rhi::Buffer> vbo;
    std::unique_ptr<rhi::Buffer> ibo;
    uint32_t index_count = 0;
};

/// Cached GPU representation of a material: descriptor set binding textures.
struct GPUMaterial {
    std::unique_ptr<rhi::DescriptorSet> ds;
};

/// Lazily uploads CPU-side assets to the GPU and caches the results.
/// One instance per renderer; stored as a World resource.
class GPUResourceCache {
public:
    /// Get-or-upload a mesh. Returns nullptr if the asset data is not available.
    const GPUMesh* get_or_upload_mesh(AssetHandle handle,
                                      const MeshAsset& asset,
                                      rhi::Device& device);

    /// Get-or-upload a texture. Returns nullptr if the asset data is not available.
    rhi::Texture* get_or_upload_texture(AssetHandle handle,
                                        const TextureAsset& asset,
                                        rhi::Device& device);

    /// Get-or-upload a material descriptor set.
    /// Resolves texture handles through the AssetServer.
    const GPUMaterial* get_or_upload_material(AssetHandle handle,
                                              const MaterialAsset& asset,
                                              rhi::Device& device,
                                              AssetServer& server,
                                              rhi::DescriptorSetLayout& layout,
                                              const rhi::Texture& default_white,
                                              const rhi::Texture& default_blue,
                                              const rhi::Texture& default_black,
                                              rhi::Texture* env_cubemap);

    /// Remove cached GPU resources for handles no longer in the AssetServer.
    void evict_unused(const AssetServer& server);

private:
    std::unordered_map<uint64_t, GPUMesh> m_meshes;
    std::unordered_map<uint64_t, std::unique_ptr<rhi::Texture>> m_textures;
    std::unordered_map<uint64_t, GPUMaterial> m_materials;
};

} // namespace helios

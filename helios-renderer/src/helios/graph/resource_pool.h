#pragma once

#include "helios/rhi/rhi.h"
#include "helios/rhi/rhi_types.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace helios::graph {

class ResourcePool {
public:
    explicit ResourcePool(rhi::Device& device);
    ~ResourcePool();

    ResourcePool(ResourcePool&&) noexcept;
    ResourcePool& operator=(ResourcePool&&) noexcept;
    ResourcePool(const ResourcePool&) = delete;
    ResourcePool& operator=(const ResourcePool&) = delete;

    // Acquire a texture matching the given desc. Reuses an existing free
    // texture if one matches, otherwise creates a new one.
    rhi::Texture* acquire_texture(const rhi::TextureDesc& desc);

    // Acquire a buffer matching the given desc.
    rhi::Buffer* acquire_buffer(const rhi::BufferDesc& desc);

    // Release a texture back to the pool for future reuse.
    void release_texture(rhi::Texture* texture);

    // Release a buffer back to the pool for future reuse.
    void release_buffer(rhi::Buffer* buffer);

    // Called once per frame after execution. Promotes released resources back
    // to the free list. Evicts resources that have been free for too many frames.
    void tick(uint32_t frames_before_eviction = 4);

    // Destroy all pooled resources (called on shutdown).
    void clear();

    // Stats
    uint32_t texture_count() const;
    uint32_t buffer_count() const;

private:
    // Internal texture entry
    struct PooledTexture {
        std::unique_ptr<rhi::Texture> texture;
        rhi::TextureDesc desc;
        uint32_t idle_frames = 0;   // frames since last use
        bool in_use = false;
    };

    // Internal buffer entry
    struct PooledBuffer {
        std::unique_ptr<rhi::Buffer> buffer;
        rhi::BufferDesc desc;
        uint32_t idle_frames = 0;
        bool in_use = false;
    };

    // Returns true if two texture descs are compatible for reuse.
    static bool texture_compatible(const rhi::TextureDesc& a, const rhi::TextureDesc& b);

    // Returns true if two buffer descs are compatible for reuse.
    static bool buffer_compatible(const rhi::BufferDesc& a, const rhi::BufferDesc& b);

    rhi::Device& m_device;
    std::vector<PooledTexture> m_textures;
    std::vector<PooledBuffer> m_buffers;
};

} // namespace helios::graph

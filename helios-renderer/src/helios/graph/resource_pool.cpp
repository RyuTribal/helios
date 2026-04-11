#include "helios/graph/resource_pool.h"
#include "helios/graph/graph_log_channel.h"
#include <algorithm>

namespace helios::graph {

ResourcePool::ResourcePool(rhi::Device& device)
    : m_device(device)
{}

ResourcePool::~ResourcePool() {
    clear();
}

ResourcePool::ResourcePool(ResourcePool&& other) noexcept
    : m_device(other.m_device)
    , m_textures(std::move(other.m_textures))
    , m_buffers(std::move(other.m_buffers))
{}

ResourcePool& ResourcePool::operator=(ResourcePool&& other) noexcept {
    if (this != &other) {
        // m_device is a reference; it must point to the same device (cannot rebind)
        m_textures = std::move(other.m_textures);
        m_buffers = std::move(other.m_buffers);
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------

bool ResourcePool::texture_compatible(const rhi::TextureDesc& a, const rhi::TextureDesc& b) {
    return a.width        == b.width
        && a.height       == b.height
        && a.format       == b.format
        && a.type         == b.type
        && a.mip_levels   == b.mip_levels
        && a.array_layers == b.array_layers
        && a.usage        == b.usage;
}

rhi::Texture* ResourcePool::acquire_texture(const rhi::TextureDesc& desc) {
    // Try to find a free texture with matching desc.
    for (auto& entry : m_textures) {
        if (!entry.in_use && texture_compatible(entry.desc, desc)) {
            entry.in_use = true;
            entry.idle_frames = 0;
            HELIOS_LOG(Graph, Trace, "ResourcePool: reusing texture '{}'", desc.debug_name);
            return entry.texture.get();
        }
    }

    // None available -- create a new one.
    auto& entry = m_textures.emplace_back();
    entry.desc = desc;
    entry.texture = m_device.create_texture(desc);
    entry.in_use = true;
    entry.idle_frames = 0;
    HELIOS_LOG(Graph, Debug, "ResourcePool: created new texture '{}' ({}x{}, pool size={})",
        desc.debug_name, desc.width, desc.height, m_textures.size());
    return entry.texture.get();
}

void ResourcePool::release_texture(rhi::Texture* texture) {
    for (auto& entry : m_textures) {
        if (entry.texture.get() == texture) {
            entry.in_use = false;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Buffer
// ---------------------------------------------------------------------------

bool ResourcePool::buffer_compatible(const rhi::BufferDesc& a, const rhi::BufferDesc& b) {
    // Reuse if same usage flags and size is >= requested.
    return a.usage == b.usage && a.size >= b.size;
}

rhi::Buffer* ResourcePool::acquire_buffer(const rhi::BufferDesc& desc) {
    for (auto& entry : m_buffers) {
        if (!entry.in_use && buffer_compatible(entry.desc, desc)) {
            entry.in_use = true;
            entry.idle_frames = 0;
            HELIOS_LOG(Graph, Trace, "ResourcePool: reusing buffer '{}'", desc.debug_name);
            return entry.buffer.get();
        }
    }

    auto& entry = m_buffers.emplace_back();
    entry.desc = desc;
    entry.buffer = m_device.create_buffer(desc);
    entry.in_use = true;
    entry.idle_frames = 0;
    HELIOS_LOG(Graph, Debug, "ResourcePool: created new buffer '{}' (size={}, pool size={})",
        desc.debug_name, desc.size, m_buffers.size());
    return entry.buffer.get();
}

void ResourcePool::release_buffer(rhi::Buffer* buffer) {
    for (auto& entry : m_buffers) {
        if (entry.buffer.get() == buffer) {
            entry.in_use = false;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Frame tick
// ---------------------------------------------------------------------------

void ResourcePool::tick(uint32_t frames_before_eviction) {
    // Increment idle counters for free resources. Evict stale ones.
    auto evict_texture = [&](PooledTexture& e) {
        if (!e.in_use) {
            e.idle_frames++;
            if (e.idle_frames >= frames_before_eviction) {
                HELIOS_LOG(Graph, Trace, "ResourcePool: evicting idle texture '{}'", e.desc.debug_name);
                return true; // mark for removal (unique_ptr destructor frees GPU resource)
            }
        }
        return false;
    };
    m_textures.erase(
        std::remove_if(m_textures.begin(), m_textures.end(), evict_texture),
        m_textures.end()
    );

    auto evict_buffer = [&](PooledBuffer& e) {
        if (!e.in_use) {
            e.idle_frames++;
            if (e.idle_frames >= frames_before_eviction) {
                HELIOS_LOG(Graph, Trace, "ResourcePool: evicting idle buffer '{}'", e.desc.debug_name);
                return true;
            }
        }
        return false;
    };
    m_buffers.erase(
        std::remove_if(m_buffers.begin(), m_buffers.end(), evict_buffer),
        m_buffers.end()
    );
}

void ResourcePool::clear() {
    m_textures.clear();   // unique_ptr destructors free GPU resources
    m_buffers.clear();
}

uint32_t ResourcePool::texture_count() const {
    return static_cast<uint32_t>(m_textures.size());
}

uint32_t ResourcePool::buffer_count() const {
    return static_cast<uint32_t>(m_buffers.size());
}

} // namespace helios::graph

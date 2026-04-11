#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include "helios/rhi/rhi_types.h"

namespace helios::rhi {
class Texture;
class Buffer;
} // namespace helios::rhi

namespace helios::graph {

// ---------------------------------------------------------------------------
// Handles -- lightweight indices into the graph's resource table.
// These are NOT GPU resources. They become GPU resources during compile phase.
// ---------------------------------------------------------------------------

struct TextureHandle {
    uint32_t index = UINT32_MAX;
    bool is_valid() const { return index != UINT32_MAX; }
    bool operator==(const TextureHandle&) const = default;
    bool operator!=(const TextureHandle&) const = default;
};

struct BufferHandle {
    uint32_t index = UINT32_MAX;
    bool is_valid() const { return index != UINT32_MAX; }
    bool operator==(const BufferHandle&) const = default;
    bool operator!=(const BufferHandle&) const = default;
};

// ---------------------------------------------------------------------------
// Resource usage flags -- how a pass accesses a resource.
// Used by the compiler to derive pipeline barriers.
// ---------------------------------------------------------------------------

enum class ResourceUsage : uint32_t {
    None            = 0,
    ColorAttachment = 1 << 0,   // written as render target
    DepthAttachment = 1 << 1,   // written as depth/stencil
    ShaderRead      = 1 << 2,   // sampled in shader
    ShaderWrite     = 1 << 3,   // storage image / SSBO write
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5,
    Present         = 1 << 6,   // swapchain present
};

inline ResourceUsage operator|(ResourceUsage a, ResourceUsage b) {
    return static_cast<ResourceUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline ResourceUsage operator&(ResourceUsage a, ResourceUsage b) {
    return static_cast<ResourceUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool has_flag(ResourceUsage flags, ResourceUsage test) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

// ---------------------------------------------------------------------------
// Resource node -- metadata entry in the graph's resource table.
// ---------------------------------------------------------------------------

enum class ResourceType : uint8_t {
    Texture,
    Buffer,
};

enum class ResourceLifetime : uint8_t {
    Imported,    // externally owned (e.g. swapchain image, persistent buffer)
    Transient,   // graph-managed, allocated from ResourcePool, freed at frame end
};

struct TextureResource {
    rhi::TextureDesc desc;
    rhi::Texture* imported = nullptr;   // non-null for imported textures
};

struct BufferResource {
    rhi::BufferDesc desc;
    rhi::Buffer* imported = nullptr;    // non-null for imported buffers
};

struct ResourceNode {
    std::string name;
    ResourceType type = ResourceType::Texture;
    ResourceLifetime lifetime = ResourceLifetime::Transient;
    std::variant<TextureResource, BufferResource> resource;

    // Bookkeeping set during compile phase
    uint32_t first_pass  = UINT32_MAX;  // first pass that uses this resource
    uint32_t last_pass   = UINT32_MAX;  // last pass that uses this resource
    uint32_t ref_count   = 0;           // number of passes referencing this resource

    // Index into ResourcePool's allocated array (set during execute phase)
    uint32_t pool_index  = UINT32_MAX;
};

// ---------------------------------------------------------------------------
// Pass resource access -- records how a single pass touches a single resource.
// ---------------------------------------------------------------------------

struct ResourceAccess {
    uint32_t resource_index = UINT32_MAX;
    ResourceUsage usage = ResourceUsage::None;
};

} // namespace helios::graph

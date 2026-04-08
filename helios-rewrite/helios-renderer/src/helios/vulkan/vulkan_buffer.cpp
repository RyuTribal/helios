#include "helios/vulkan/vulkan_buffer.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <cstring>
#include <utility>

namespace helios::rhi::vulkan {

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static VkBufferUsageFlags map_buffer_usage(BufferUsage usage)
{
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Always allow uploads

    if (has_flag(usage, BufferUsage::Vertex))
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::Index))
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::Uniform))
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::Storage))
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::Transfer))
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    return flags;
}

static VmaMemoryUsage map_memory_access(MemoryAccess access)
{
    switch (access) {
        case MemoryAccess::GPU_Only:   return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryAccess::CPU_to_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryAccess::GPU_to_CPU: return VMA_MEMORY_USAGE_GPU_TO_CPU;
    }
    return VMA_MEMORY_USAGE_GPU_ONLY;
}

// ---------------------------------------------------------------------------
//  Constructor
// ---------------------------------------------------------------------------

VulkanBuffer::VulkanBuffer(VulkanDevice& device, const BufferDesc& desc,
                           const void* initial_data)
    : m_desc(desc)
    , m_device(&device)
{
    HELIOS_ASSERT(desc.size > 0, "Buffer size must be > 0");

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = desc.size;
    buffer_info.usage = map_buffer_usage(desc.usage);
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = map_memory_access(desc.access);

    VkResult result = vmaCreateBuffer(device.allocator(), &buffer_info, &alloc_info,
                                      &m_buffer, &m_allocation, &m_alloc_info);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create buffer '{}' (size={})",
                         desc.debug_name, desc.size);
        return;
    }

    // Upload initial data
    if (initial_data) {
        if (desc.access == MemoryAccess::GPU_Only) {
            // Stage through a CPU-visible buffer, then copy via immediate_submit
            VkBufferCreateInfo staging_buffer_info{};
            staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            staging_buffer_info.size = desc.size;
            staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo staging_alloc_info{};
            staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            VkBuffer staging_buffer = VK_NULL_HANDLE;
            VmaAllocation staging_allocation = VK_NULL_HANDLE;

            vmaCreateBuffer(device.allocator(), &staging_buffer_info, &staging_alloc_info,
                            &staging_buffer, &staging_allocation, nullptr);

            void* mapped = nullptr;
            vmaMapMemory(device.allocator(), staging_allocation, &mapped);
            std::memcpy(mapped, initial_data, desc.size);
            vmaUnmapMemory(device.allocator(), staging_allocation);

            device.immediate_submit([&](VkCommandBuffer cmd) {
                VkBufferCopy copy_region{};
                copy_region.size = desc.size;
                vkCmdCopyBuffer(cmd, staging_buffer, m_buffer, 1, &copy_region);
            });

            vmaDestroyBuffer(device.allocator(), staging_buffer, staging_allocation);
        } else {
            // CPU-accessible -- map and copy directly
            void* mapped = nullptr;
            vmaMapMemory(device.allocator(), m_allocation, &mapped);
            std::memcpy(mapped, initial_data, desc.size);
            vmaUnmapMemory(device.allocator(), m_allocation);
        }
    }

    // Debug name
    if (!desc.debug_name.empty()) {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_BUFFER,
            reinterpret_cast<uint64_t>(m_buffer), desc.debug_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Created buffer '{}' (size={})", desc.debug_name, desc.size);
}

// ---------------------------------------------------------------------------
//  Destructor
// ---------------------------------------------------------------------------

VulkanBuffer::~VulkanBuffer()
{
    destroy();
}

// ---------------------------------------------------------------------------
//  Move semantics
// ---------------------------------------------------------------------------

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE))
    , m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE))
    , m_alloc_info(std::exchange(other.m_alloc_info, VmaAllocationInfo{}))
    , m_desc(std::move(other.m_desc))
    , m_device(std::exchange(other.m_device, nullptr))
{
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_buffer     = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
        m_alloc_info = std::exchange(other.m_alloc_info, VmaAllocationInfo{});
        m_desc       = std::move(other.m_desc);
        m_device     = std::exchange(other.m_device, nullptr);
    }
    return *this;
}

// ---------------------------------------------------------------------------
//  Data access
// ---------------------------------------------------------------------------

void VulkanBuffer::set_data(const void* data, uint32_t size, uint32_t offset)
{
    HELIOS_ASSERT(m_buffer != VK_NULL_HANDLE, "Buffer not initialized");
    HELIOS_ASSERT(data != nullptr, "Data must not be null");
    HELIOS_ASSERT(offset + size <= m_desc.size, "Write exceeds buffer bounds");

    if (m_desc.access == MemoryAccess::GPU_Only) {
        // Stage through a CPU-visible buffer, then copy via immediate_submit
        VkBufferCreateInfo staging_buffer_info{};
        staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_buffer_info.size = size;
        staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo staging_alloc_info{};
        staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VmaAllocation staging_allocation = VK_NULL_HANDLE;

        vmaCreateBuffer(m_device->allocator(), &staging_buffer_info, &staging_alloc_info,
                        &staging_buffer, &staging_allocation, nullptr);

        void* mapped = nullptr;
        vmaMapMemory(m_device->allocator(), staging_allocation, &mapped);
        std::memcpy(mapped, data, size);
        vmaUnmapMemory(m_device->allocator(), staging_allocation);

        m_device->immediate_submit([&](VkCommandBuffer cmd) {
            VkBufferCopy copy_region{};
            copy_region.srcOffset = 0;
            copy_region.dstOffset = offset;
            copy_region.size = size;
            vkCmdCopyBuffer(cmd, staging_buffer, m_buffer, 1, &copy_region);
        });

        vmaDestroyBuffer(m_device->allocator(), staging_buffer, staging_allocation);
    } else {
        // CPU-accessible -- map and copy directly
        void* mapped = nullptr;
        vmaMapMemory(m_device->allocator(), m_allocation, &mapped);
        std::memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
        vmaUnmapMemory(m_device->allocator(), m_allocation);
    }
}

void* VulkanBuffer::map()
{
    HELIOS_ASSERT(m_buffer != VK_NULL_HANDLE, "Buffer not initialized");
    HELIOS_ASSERT(m_desc.access != MemoryAccess::GPU_Only,
                  "Cannot map a GPU_Only buffer");

    void* mapped = nullptr;
    vmaMapMemory(m_device->allocator(), m_allocation, &mapped);
    return mapped;
}

void VulkanBuffer::unmap()
{
    HELIOS_ASSERT(m_buffer != VK_NULL_HANDLE, "Buffer not initialized");
    vmaUnmapMemory(m_device->allocator(), m_allocation);
}

// ---------------------------------------------------------------------------
//  Private
// ---------------------------------------------------------------------------

void VulkanBuffer::destroy()
{
    if (m_buffer != VK_NULL_HANDLE && m_device != nullptr) {
        vmaDestroyBuffer(m_device->allocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

} // namespace helios::rhi::vulkan

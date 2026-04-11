#pragma once

#include "helios/rhi/rhi_buffer.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII Vulkan buffer. Constructor allocates VkBuffer + VmaAllocation.
// Destructor frees both. Move-only.
class VulkanBuffer : public rhi::Buffer {
public:
    VulkanBuffer() = default;  // null/empty state
    VulkanBuffer(VulkanDevice& device, const BufferDesc& desc, const void* initial_data = nullptr);
    ~VulkanBuffer() override;

    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // Update buffer contents. For CPU-visible buffers: direct memcpy.
    // For GPU-only buffers: staging buffer + ImmediateSubmit.
    void set_data(const void* data, uint32_t size, uint32_t offset = 0) override;

    void* map() override;
    void unmap() override;
    uint32_t size() const override { return m_desc.size; }
    const BufferDesc& desc() const override { return m_desc; }

    VkBuffer vk_buffer() const { return m_buffer; }

    template<typename T> T native_handle() const;

    explicit operator bool() const { return m_buffer != VK_NULL_HANDLE; }

private:
    void destroy();

    VkBuffer m_buffer              = VK_NULL_HANDLE;
    VmaAllocation m_allocation     = VK_NULL_HANDLE;
    VmaAllocationInfo m_alloc_info{};
    BufferDesc m_desc;
    VulkanDevice* m_device         = nullptr;
};

template<> inline VkBuffer VulkanBuffer::native_handle<VkBuffer>() const {
    return m_buffer;
}

} // namespace helios::rhi::vulkan

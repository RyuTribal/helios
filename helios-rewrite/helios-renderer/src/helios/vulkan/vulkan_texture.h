#pragma once

#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII Vulkan texture. Constructor allocates VkImage + VmaAllocation +
// VkImageView + VkSampler. Destructor frees all. Move-only.
//
// Two construction modes:
//   1. Owned: allocates image via VMA (normal textures)
//   2. Wrapped: wraps an existing VkImage without ownership (swapchain images)
class VulkanTexture {
public:
    VulkanTexture() = default;  // null/empty state

    // Owned construction: allocates image via VMA
    VulkanTexture(VulkanDevice& device, const TextureDesc& desc,
                  const void* initial_data = nullptr);

    // Wrapped construction: wraps existing VkImage (e.g. swapchain images)
    // Does NOT own the image -- destructor will not free it.
    VulkanTexture(VulkanDevice& device, VkImage image, VkFormat format,
                  uint32_t width, uint32_t height, const char* debug_name);

    ~VulkanTexture();

    VulkanTexture(VulkanTexture&& other) noexcept;
    VulkanTexture& operator=(VulkanTexture&& other) noexcept;
    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    uint32_t width() const { return m_desc.width; }
    uint32_t height() const { return m_desc.height; }
    TextureFormat format() const { return m_desc.format; }
    const TextureDesc& desc() const { return m_desc; }

    VkImage vk_image() const { return m_image; }
    VkImageView vk_image_view() const { return m_image_view; }
    VkSampler vk_sampler() const { return m_sampler; }
    VkImageLayout current_layout() const { return m_current_layout; }
    void set_current_layout(VkImageLayout layout) { m_current_layout = layout; }

    template<typename T> T native_handle() const;

    explicit operator bool() const { return m_image != VK_NULL_HANDLE; }

    // Static utility: transition image layout via synchronization2
    static void transition_layout(VkCommandBuffer cmd, VkImage image,
                                  VkImageLayout old_layout, VkImageLayout new_layout,
                                  VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

private:
    void destroy();

    VkImage m_image                = VK_NULL_HANDLE;
    VmaAllocation m_allocation     = VK_NULL_HANDLE;
    VkImageView m_image_view       = VK_NULL_HANDLE;
    VkSampler m_sampler            = VK_NULL_HANDLE;
    VkImageLayout m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureDesc m_desc;
    VulkanDevice* m_device         = nullptr;
    bool m_owns_image              = true;
};

template<> inline VkImage VulkanTexture::native_handle<VkImage>() const {
    return m_image;
}
template<> inline VkImageView VulkanTexture::native_handle<VkImageView>() const {
    return m_image_view;
}
template<> inline VkSampler VulkanTexture::native_handle<VkSampler>() const {
    return m_sampler;
}

} // namespace helios::rhi::vulkan

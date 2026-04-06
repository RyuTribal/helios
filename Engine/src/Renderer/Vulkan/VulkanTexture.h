#pragma once

#include "RHI/RHIResources.h"
#include "RHI/RHITypes.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Engine {

    class VulkanDevice;

    class VulkanTexture : public RHITexture {
    public:
        // Create a new texture from a description (allocates image via VMA)
        VulkanTexture(VulkanDevice* device, const TextureDesc& desc, const void* initialData);

        // Wrap an existing VkImage (e.g. swapchain images -- we don't own/destroy them)
        VulkanTexture(VulkanDevice* device, VkImage image, VkFormat format,
                      uint32_t width, uint32_t height, const char* debugName);

        ~VulkanTexture() override;

        uint32_t GetWidth() const override { return m_Desc.Width; }
        uint32_t GetHeight() const override { return m_Desc.Height; }

        VkImage GetVkImage() const { return m_Image; }
        VkImageView GetVkImageView() const { return m_ImageView; }
        VkSampler GetVkSampler() const { return m_Sampler; }
        VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
        void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }

        // Transition image layout via a command buffer (Vulkan 1.3 synchronization2)
        static void TransitionLayout(VkCommandBuffer cmd, VkImage image,
            VkImageLayout oldLayout, VkImageLayout newLayout,
            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    private:
        VkImage m_Image = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkSampler m_Sampler = VK_NULL_HANDLE;
        VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        TextureDesc m_Desc;
        VulkanDevice* m_Device;
        bool m_OwnsImage = true; // false for swapchain images
    };

} // namespace Engine

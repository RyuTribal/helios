#include "pch.h"
#include "Renderer/Vulkan/VulkanTexture.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanUtils.h"

namespace Engine {

    // ---- Helpers ----

    static bool IsDepthFormat(ImageFormat format)
    {
        return format == ImageFormat::DEPTH24_STENCIL8 || format == ImageFormat::DEPTH32F;
    }

    static VkImageUsageFlags MapTextureUsage(TextureUsage usage)
    {
        VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Always allow uploads

        if (usage & TextureUsage::Sampled)
            flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (usage & TextureUsage::ColorAttachment)
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (usage & TextureUsage::DepthAttachment)
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (usage & TextureUsage::Storage)
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (usage & TextureUsage::Transfer)
            flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        return flags;
    }

    static VkSampleCountFlagBits MapSampleCount(uint32_t samples)
    {
        switch (samples) {
            case 1:  return VK_SAMPLE_COUNT_1_BIT;
            case 2:  return VK_SAMPLE_COUNT_2_BIT;
            case 4:  return VK_SAMPLE_COUNT_4_BIT;
            case 8:  return VK_SAMPLE_COUNT_8_BIT;
            case 16: return VK_SAMPLE_COUNT_16_BIT;
            case 32: return VK_SAMPLE_COUNT_32_BIT;
            case 64: return VK_SAMPLE_COUNT_64_BIT;
            default: return VK_SAMPLE_COUNT_1_BIT;
        }
    }

    static VkImageViewType MapViewType(TextureType type)
    {
        switch (type) {
            case TextureType::Texture2D:      return VK_IMAGE_VIEW_TYPE_2D;
            case TextureType::TextureCube:     return VK_IMAGE_VIEW_TYPE_CUBE;
            case TextureType::Texture2DArray:  return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    static uint32_t FormatBytesPerPixel(ImageFormat format)
    {
        switch (format) {
            case ImageFormat::R8:               return 1;
            case ImageFormat::RG8:              return 2;
            case ImageFormat::RGB8:             return 3;
            case ImageFormat::RGBA8:            return 4;
            case ImageFormat::RG16F:            return 4;
            case ImageFormat::RGBA16F:          return 8;
            case ImageFormat::RGBA32F:          return 16;
            case ImageFormat::R32F:             return 4;
            case ImageFormat::RG32F:            return 8;
            case ImageFormat::RGB32F:           return 12;
            case ImageFormat::DEPTH24_STENCIL8: return 4;
            case ImageFormat::DEPTH32F:         return 4;
        }
        return 4;
    }

    // =========================================================================
    //  Main constructor: allocate image via VMA
    // =========================================================================
    VulkanTexture::VulkanTexture(VulkanDevice* device, const TextureDesc& desc, const void* initialData)
        : m_Device(device), m_Desc(desc), m_OwnsImage(true)
    {
        VkFormat vkFormat = ToVkFormat(desc.Format);

        // -- 1. Create VkImage via VMA --
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = vkFormat;
        imageInfo.extent        = { desc.Width, desc.Height, 1 };
        imageInfo.mipLevels     = desc.MipLevels;
        imageInfo.arrayLayers   = desc.ArrayLayers;
        imageInfo.samples       = MapSampleCount(desc.Samples);
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = MapTextureUsage(desc.Usage);
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (desc.Type == TextureType::TextureCube) {
            imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult result = vmaCreateImage(device->GetAllocator(), &imageInfo, &allocInfo,
                                         &m_Image, &m_Allocation, nullptr);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create image '{}' ({}x{})",
                               desc.DebugName, desc.Width, desc.Height);
            return;
        }

        // -- 2. Create image view --
        bool isDepth = IsDepthFormat(desc.Format);
        VkImageAspectFlags aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = m_Image;
        viewInfo.viewType                        = MapViewType(desc.Type);
        viewInfo.format                          = vkFormat;
        viewInfo.subresourceRange.aspectMask     = aspectMask;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = desc.MipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = desc.ArrayLayers;

        result = vkCreateImageView(device->GetDevice(), &viewInfo, nullptr, &m_ImageView);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create image view for '{}'", desc.DebugName);
            return;
        }

        // -- 3. Create sampler --
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = VK_FILTER_LINEAR;
        samplerInfo.minFilter               = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = static_cast<float>(desc.MipLevels);
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        result = vkCreateSampler(device->GetDevice(), &samplerInfo, nullptr, &m_Sampler);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create sampler for '{}'", desc.DebugName);
            return;
        }

        // -- 4. Upload initial data if provided --
        if (initialData) {
            uint32_t imageSize = desc.Width * desc.Height * desc.ArrayLayers * FormatBytesPerPixel(desc.Format);

            // Create staging buffer
            VkBufferCreateInfo stagingBufferInfo{};
            stagingBufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingBufferInfo.size        = imageSize;
            stagingBufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo stagingAllocInfo{};
            stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            VkBuffer stagingBuffer = VK_NULL_HANDLE;
            VmaAllocation stagingAllocation = VK_NULL_HANDLE;

            vmaCreateBuffer(device->GetAllocator(), &stagingBufferInfo, &stagingAllocInfo,
                            &stagingBuffer, &stagingAllocation, nullptr);

            void* mapped = nullptr;
            vmaMapMemory(device->GetAllocator(), stagingAllocation, &mapped);
            std::memcpy(mapped, initialData, imageSize);
            vmaUnmapMemory(device->GetAllocator(), stagingAllocation);

            // Transition, copy, transition via ImmediateSubmit
            device->ImmediateSubmit([&](VkCommandBuffer cmd) {
                // Transition UNDEFINED -> TRANSFER_DST_OPTIMAL
                TransitionLayout(cmd, m_Image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    aspectMask);

                // Copy buffer to image
                VkBufferImageCopy region{};
                region.bufferOffset                    = 0;
                region.bufferRowLength                 = 0;
                region.bufferImageHeight               = 0;
                region.imageSubresource.aspectMask     = aspectMask;
                region.imageSubresource.mipLevel       = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount     = desc.ArrayLayers;
                region.imageOffset                     = {0, 0, 0};
                region.imageExtent                     = {desc.Width, desc.Height, 1};

                vkCmdCopyBufferToImage(cmd, stagingBuffer, m_Image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                // Transition TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
                TransitionLayout(cmd, m_Image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    aspectMask);
            });

            m_CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            vmaDestroyBuffer(device->GetAllocator(), stagingBuffer, stagingAllocation);
        }
        else
        {
            // No initial data — still transition to a usable layout so the image
            // can be safely sampled (prevents GPU hang on Intel when ImGui reads it)
            VkImageLayout targetLayout = isDepth
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            device->ImmediateSubmit([&](VkCommandBuffer cmd) {
                TransitionLayout(cmd, m_Image,
                    VK_IMAGE_LAYOUT_UNDEFINED, targetLayout, aspectMask);
            });
            m_CurrentLayout = targetLayout;
        }

        // -- 5. Debug names --
        if (!desc.DebugName.empty()) {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_IMAGE,
                reinterpret_cast<uint64_t>(m_Image), desc.DebugName.c_str());

            std::string viewName = desc.DebugName + "_View";
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_IMAGE_VIEW,
                reinterpret_cast<uint64_t>(m_ImageView), viewName.c_str());

            std::string samplerName = desc.DebugName + "_Sampler";
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_SAMPLER,
                reinterpret_cast<uint64_t>(m_Sampler), samplerName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Created texture '{}' ({}x{}, {} mips, {} layers)",
                           desc.DebugName, desc.Width, desc.Height, desc.MipLevels, desc.ArrayLayers);
    }

    // =========================================================================
    //  Swapchain wrapper constructor: wrap existing VkImage
    // =========================================================================
    VulkanTexture::VulkanTexture(VulkanDevice* device, VkImage image, VkFormat format,
                                 uint32_t width, uint32_t height, const char* debugName)
        : m_Device(device), m_Image(image), m_OwnsImage(false)
    {
        m_Desc.Width  = width;
        m_Desc.Height = height;
        m_Desc.DebugName = debugName ? debugName : "";

        // Create image view for the swapchain image
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = format;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VkResult result = vkCreateImageView(device->GetDevice(), &viewInfo, nullptr, &m_ImageView);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create image view for swapchain image '{}'",
                               debugName ? debugName : "");
            return;
        }

        // No sampler needed for swapchain images

        // Debug names
        if (debugName && debugName[0] != '\0') {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_IMAGE,
                reinterpret_cast<uint64_t>(m_Image), debugName);

            std::string viewName = std::string(debugName) + "_View";
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_IMAGE_VIEW,
                reinterpret_cast<uint64_t>(m_ImageView), viewName.c_str());
        }
    }

    // =========================================================================
    //  Destructor
    // =========================================================================
    VulkanTexture::~VulkanTexture()
    {
        if (m_Sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_Device->GetDevice(), m_Sampler, nullptr);
            m_Sampler = VK_NULL_HANDLE;
        }

        if (m_ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_Device->GetDevice(), m_ImageView, nullptr);
            m_ImageView = VK_NULL_HANDLE;
        }

        if (m_OwnsImage && m_Image != VK_NULL_HANDLE) {
            vmaDestroyImage(m_Device->GetAllocator(), m_Image, m_Allocation);
            m_Image = VK_NULL_HANDLE;
            m_Allocation = VK_NULL_HANDLE;
        }
    }

    // =========================================================================
    //  TransitionLayout -- Vulkan 1.3 synchronization2
    // =========================================================================
    void VulkanTexture::TransitionLayout(VkCommandBuffer cmd, VkImage image,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkImageAspectFlags aspectMask)
    {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;

        barrier.subresourceRange.aspectMask     = aspectMask;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

        // Use broad stage/access masks -- safe for all transitions
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        VkDependencyInfo depInfo{};
        depInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount  = 1;
        depInfo.pImageMemoryBarriers     = &barrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

} // namespace Engine

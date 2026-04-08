#include "helios/vulkan/vulkan_texture.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_utils.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <cstring>
#include <string>
#include <utility>

namespace helios::rhi::vulkan {

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static VkImageUsageFlags map_texture_usage(TextureUsage usage)
{
    VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Always allow uploads

    if (has_flag(usage, TextureUsage::Sampled))
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (has_flag(usage, TextureUsage::ColorAttachment))
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (has_flag(usage, TextureUsage::DepthAttachment))
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (has_flag(usage, TextureUsage::Storage))
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (has_flag(usage, TextureUsage::Transfer))
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return flags;
}

static VkSampleCountFlagBits map_sample_count(uint32_t samples)
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

// =========================================================================
//  Owned constructor: allocate image via VMA
// =========================================================================

VulkanTexture::VulkanTexture(VulkanDevice& device, const TextureDesc& desc,
                             const void* initial_data)
    : m_desc(desc)
    , m_device(&device)
    , m_owns_image(true)
{
    HELIOS_ASSERT(desc.width > 0 && desc.height > 0, "Texture dimensions must be > 0");

    VkFormat vk_format = to_vk_format(desc.format);

    // -- 1. Create VkImage via VMA --
    VkImageCreateInfo image_info{};
    image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType     = VK_IMAGE_TYPE_2D;
    image_info.format        = vk_format;
    image_info.extent        = { desc.width, desc.height, 1 };
    image_info.mipLevels     = desc.mip_levels;
    image_info.arrayLayers   = desc.array_layers;
    image_info.samples       = map_sample_count(1);  // TextureDesc has no samples field; default 1
    image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage         = map_texture_usage(desc.usage);
    image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (desc.type == TextureType::TextureCube) {
        image_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(device.allocator(), &image_info, &alloc_info,
                                     &m_image, &m_allocation, nullptr);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create image '{}' ({}x{})",
                         desc.debug_name, desc.width, desc.height);
        return;
    }

    // -- 2. Create image view --
    bool depth = is_depth_format(desc.format);
    VkImageAspectFlags aspect_mask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo view_info{};
    view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                           = m_image;
    view_info.viewType                        = to_vk_view_type(desc.type);
    view_info.format                          = vk_format;
    view_info.subresourceRange.aspectMask     = aspect_mask;
    view_info.subresourceRange.baseMipLevel   = 0;
    view_info.subresourceRange.levelCount     = desc.mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount     = desc.array_layers;

    result = vkCreateImageView(device.device(), &view_info, nullptr, &m_image_view);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create image view for '{}'", desc.debug_name);
        return;
    }

    // -- 3. Create sampler --
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter               = VK_FILTER_LINEAR;
    sampler_info.minFilter               = VK_FILTER_LINEAR;
    sampler_info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addr_mode       = to_vk_sampler_mode(desc.sampler);
    sampler_info.addressModeU            = addr_mode;
    sampler_info.addressModeV            = addr_mode;
    sampler_info.addressModeW            = addr_mode;
    sampler_info.mipLodBias              = 0.0f;
    sampler_info.anisotropyEnable        = VK_FALSE;
    sampler_info.maxAnisotropy           = 1.0f;
    sampler_info.compareEnable           = VK_FALSE;
    sampler_info.compareOp               = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod                  = 0.0f;
    sampler_info.maxLod                  = static_cast<float>(desc.mip_levels);
    sampler_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    result = vkCreateSampler(device.device(), &sampler_info, nullptr, &m_sampler);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create sampler for '{}'", desc.debug_name);
        return;
    }

    // -- 4. Upload initial data if provided --
    if (initial_data) {
        uint32_t image_size = desc.width * desc.height * desc.array_layers
                            * format_bytes_per_pixel(desc.format);

        // Create staging buffer
        VkBufferCreateInfo staging_buffer_info{};
        staging_buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_buffer_info.size        = image_size;
        staging_buffer_info.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo staging_alloc_info{};
        staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VmaAllocation staging_allocation = VK_NULL_HANDLE;

        vmaCreateBuffer(device.allocator(), &staging_buffer_info, &staging_alloc_info,
                        &staging_buffer, &staging_allocation, nullptr);

        void* mapped = nullptr;
        vmaMapMemory(device.allocator(), staging_allocation, &mapped);
        std::memcpy(mapped, initial_data, image_size);
        vmaUnmapMemory(device.allocator(), staging_allocation);

        // Transition, copy, transition via immediate_submit
        device.immediate_submit([&](VkCommandBuffer cmd) {
            // Transition UNDEFINED -> TRANSFER_DST_OPTIMAL
            transition_layout(cmd, m_image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                aspect_mask);

            // Copy buffer to image
            VkBufferImageCopy region{};
            region.bufferOffset                    = 0;
            region.bufferRowLength                 = 0;
            region.bufferImageHeight               = 0;
            region.imageSubresource.aspectMask     = aspect_mask;
            region.imageSubresource.mipLevel       = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = desc.array_layers;
            region.imageOffset                     = {0, 0, 0};
            region.imageExtent                     = {desc.width, desc.height, 1};

            vkCmdCopyBufferToImage(cmd, staging_buffer, m_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            // Transition TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
            transition_layout(cmd, m_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                aspect_mask);
        });

        m_current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vmaDestroyBuffer(device.allocator(), staging_buffer, staging_allocation);
    }
    // Note: images without initial data stay in UNDEFINED layout.
    // They MUST be transitioned before first use.

    // -- 5. Debug names --
    if (!desc.debug_name.empty()) {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_IMAGE,
            reinterpret_cast<uint64_t>(m_image), desc.debug_name.c_str());

        std::string view_name = desc.debug_name + "_View";
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_IMAGE_VIEW,
            reinterpret_cast<uint64_t>(m_image_view), view_name.c_str());

        std::string sampler_name = desc.debug_name + "_Sampler";
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_SAMPLER,
            reinterpret_cast<uint64_t>(m_sampler), sampler_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Created texture '{}' ({}x{}, {} mips, {} layers)",
                    desc.debug_name, desc.width, desc.height, desc.mip_levels, desc.array_layers);
}

// =========================================================================
//  Wrapped constructor: wrap existing VkImage (e.g. swapchain images)
// =========================================================================

VulkanTexture::VulkanTexture(VulkanDevice& device, VkImage image, VkFormat format,
                             uint32_t width, uint32_t height, const char* debug_name)
    : m_image(image)
    , m_device(&device)
    , m_owns_image(false)
{
    HELIOS_ASSERT(image != VK_NULL_HANDLE, "VkImage must not be null for wrapped construction");

    m_desc.width  = width;
    m_desc.height = height;
    m_desc.debug_name = debug_name ? debug_name : "";

    // Create image view for the swapchain image
    VkImageViewCreateInfo view_info{};
    view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                           = image;
    view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format                          = format;
    view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel   = 0;
    view_info.subresourceRange.levelCount     = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount     = 1;

    VkResult result = vkCreateImageView(device.device(), &view_info, nullptr, &m_image_view);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create image view for swapchain image '{}'",
                         debug_name ? debug_name : "");
        return;
    }

    // No sampler needed for swapchain images

    // Debug names
    if (debug_name && debug_name[0] != '\0') {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_IMAGE,
            reinterpret_cast<uint64_t>(m_image), debug_name);

        std::string view_name_str = std::string(debug_name) + "_View";
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_IMAGE_VIEW,
            reinterpret_cast<uint64_t>(m_image_view), view_name_str.c_str());
    }
}

// =========================================================================
//  Destructor
// =========================================================================

VulkanTexture::~VulkanTexture()
{
    destroy();
}

// =========================================================================
//  Move semantics
// =========================================================================

VulkanTexture::VulkanTexture(VulkanTexture&& other) noexcept
    : m_image(std::exchange(other.m_image, VK_NULL_HANDLE))
    , m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE))
    , m_image_view(std::exchange(other.m_image_view, VK_NULL_HANDLE))
    , m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE))
    , m_current_layout(std::exchange(other.m_current_layout, VK_IMAGE_LAYOUT_UNDEFINED))
    , m_desc(std::move(other.m_desc))
    , m_device(std::exchange(other.m_device, nullptr))
    , m_owns_image(std::exchange(other.m_owns_image, true))
{
}

VulkanTexture& VulkanTexture::operator=(VulkanTexture&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_image          = std::exchange(other.m_image, VK_NULL_HANDLE);
        m_allocation     = std::exchange(other.m_allocation, VK_NULL_HANDLE);
        m_image_view     = std::exchange(other.m_image_view, VK_NULL_HANDLE);
        m_sampler        = std::exchange(other.m_sampler, VK_NULL_HANDLE);
        m_current_layout = std::exchange(other.m_current_layout, VK_IMAGE_LAYOUT_UNDEFINED);
        m_desc           = std::move(other.m_desc);
        m_device         = std::exchange(other.m_device, nullptr);
        m_owns_image     = std::exchange(other.m_owns_image, true);
    }
    return *this;
}

// =========================================================================
//  Private
// =========================================================================

void VulkanTexture::destroy()
{
    if (m_device == nullptr) return;

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device->device(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device->device(), m_image_view, nullptr);
        m_image_view = VK_NULL_HANDLE;
    }

    if (m_owns_image && m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device->allocator(), m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

// =========================================================================
//  TransitionLayout -- Vulkan 1.3 synchronization2
// =========================================================================

void VulkanTexture::transition_layout(VkCommandBuffer cmd, VkImage image,
    VkImageLayout old_layout, VkImageLayout new_layout,
    VkImageAspectFlags aspect_mask)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout           = old_layout;
    barrier.newLayout           = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;

    barrier.subresourceRange.aspectMask     = aspect_mask;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

    // Use broad stage/access masks -- safe for all transitions
    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    VkDependencyInfo dep_info{};
    dep_info.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.imageMemoryBarrierCount  = 1;
    dep_info.pImageMemoryBarriers     = &barrier;

    vkCmdPipelineBarrier2(cmd, &dep_info);
}

} // namespace helios::rhi::vulkan

#include "helios/vulkan/vulkan_framebuffer.h"
#include "helios/vulkan/vulkan_render_pass.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_texture.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <string>
#include <utility>

namespace helios::rhi::vulkan {

// =========================================================================
//  Constructor
// =========================================================================

VulkanFramebuffer::VulkanFramebuffer(VulkanDevice& device,
                                     const VulkanRenderPass* render_pass,
                                     const std::vector<const VulkanTexture*>& attachments,
                                     uint32_t width, uint32_t height,
                                     const char* debug_name)
    : m_device(&device)
    , m_render_pass(render_pass)
    , m_width(width)
    , m_height(height)
    , m_debug_name(debug_name ? debug_name : "")
{
    HELIOS_ASSERT(device, "VulkanDevice must be initialized");
    HELIOS_ASSERT(width > 0 && height > 0, "Framebuffer dimensions must be > 0");

    // Extract VkImageView from each texture attachment
    m_attachment_views.reserve(attachments.size());
    for (const auto* attachment : attachments) {
        if (attachment) {
            m_attachment_views.push_back(attachment->vk_image_view());
        }
    }

    HELIOS_LOG_INFO(Renderer, "Created framebuffer '{}' ({}x{}, {} attachments)",
                    m_debug_name, width, height, attachments.size());
}

// =========================================================================
//  Destructor
// =========================================================================

VulkanFramebuffer::~VulkanFramebuffer()
{
    destroy();
}

// =========================================================================
//  Move semantics
// =========================================================================

VulkanFramebuffer::VulkanFramebuffer(VulkanFramebuffer&& other) noexcept
    : m_framebuffer(std::exchange(other.m_framebuffer, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, nullptr))
    , m_render_pass(std::exchange(other.m_render_pass, nullptr))
    , m_attachment_views(std::move(other.m_attachment_views))
    , m_width(std::exchange(other.m_width, 0))
    , m_height(std::exchange(other.m_height, 0))
    , m_debug_name(std::move(other.m_debug_name))
{
}

VulkanFramebuffer& VulkanFramebuffer::operator=(VulkanFramebuffer&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_framebuffer      = std::exchange(other.m_framebuffer, VK_NULL_HANDLE);
        m_device           = std::exchange(other.m_device, nullptr);
        m_render_pass      = std::exchange(other.m_render_pass, nullptr);
        m_attachment_views = std::move(other.m_attachment_views);
        m_width            = std::exchange(other.m_width, 0);
        m_height           = std::exchange(other.m_height, 0);
        m_debug_name       = std::move(other.m_debug_name);
    }
    return *this;
}

// =========================================================================
//  Lazy VkFramebuffer creation
// =========================================================================

VkFramebuffer VulkanFramebuffer::get_or_create_vk_framebuffer()
{
    if (m_framebuffer != VK_NULL_HANDLE)
        return m_framebuffer;

    if (!m_render_pass || m_attachment_views.empty()) {
        HELIOS_LOG_ERROR(Renderer, "Cannot create VkFramebuffer: missing render pass or attachment views");
        return VK_NULL_HANDLE;
    }

    VkFramebufferCreateInfo fb_info{};
    fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass      = m_render_pass->vk_render_pass();
    fb_info.attachmentCount = static_cast<uint32_t>(m_attachment_views.size());
    fb_info.pAttachments    = m_attachment_views.data();
    fb_info.width           = m_width;
    fb_info.height          = m_height;
    fb_info.layers          = 1;

    VkResult result = vkCreateFramebuffer(m_device->device(), &fb_info, nullptr, &m_framebuffer);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create VkFramebuffer '{}'", m_debug_name);
        return VK_NULL_HANDLE;
    }

    if (!m_debug_name.empty()) {
        m_device->context().set_debug_name(m_device->device(), VK_OBJECT_TYPE_FRAMEBUFFER,
                                           reinterpret_cast<uint64_t>(m_framebuffer),
                                           m_debug_name.c_str());
    }

    return m_framebuffer;
}

// =========================================================================
//  Private
// =========================================================================

void VulkanFramebuffer::destroy()
{
    if (m_framebuffer != VK_NULL_HANDLE && m_device != nullptr) {
        vkDestroyFramebuffer(m_device->device(), m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }
    m_attachment_views.clear();
}

} // namespace helios::rhi::vulkan

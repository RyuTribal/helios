#include "helios/vulkan/vulkan_render_pass.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_utils.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <utility>
#include <vector>

namespace helios::rhi::vulkan {

// =========================================================================
//  Constructor
// =========================================================================

VulkanRenderPass::VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc)
    : m_device(&device)
    , m_desc(desc)
{
    HELIOS_ASSERT(device, "VulkanDevice must be initialized");

    // -- Build attachment descriptions --
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> color_refs;

    for (uint32_t i = 0; i < static_cast<uint32_t>(desc.color_attachments.size()); i++) {
        const auto& ca = desc.color_attachments[i];

        VkAttachmentDescription attachment{};
        attachment.format         = to_vk_format(ca.format);
        attachment.samples        = static_cast<VkSampleCountFlagBits>(ca.samples);
        attachment.loadOp         = to_vk_load_op(ca.load);
        attachment.storeOp        = to_vk_store_op(ca.store);
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments.push_back(attachment);

        VkAttachmentReference ref{};
        ref.attachment = i;
        ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_refs.push_back(ref);
    }

    // -- Optional depth attachment --
    VkAttachmentReference depth_ref{};
    if (desc.has_depth) {
        const auto& da = desc.depth_attachment;

        VkAttachmentDescription depth_attachment{};
        depth_attachment.format         = to_vk_format(da.format);
        depth_attachment.samples        = static_cast<VkSampleCountFlagBits>(da.samples);
        depth_attachment.loadOp         = to_vk_load_op(da.load);
        depth_attachment.storeOp        = to_vk_store_op(da.store);
        depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depth_attachment);

        depth_ref.attachment = static_cast<uint32_t>(attachments.size() - 1);
        depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // -- Single subpass --
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments       = color_refs.data();
    subpass.pDepthStencilAttachment = desc.has_depth ? &depth_ref : nullptr;

    // -- Subpass dependencies --
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (desc.has_depth)
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // -- Create render pass --
    VkRenderPassCreateInfo rp_info{};
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    rp_info.pAttachments    = attachments.data();
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &dependency;

    VkResult result = vkCreateRenderPass(device.device(), &rp_info, nullptr, &m_render_pass);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create render pass '{}'", desc.debug_name);
        return;
    }

    // Debug name
    if (!desc.debug_name.empty()) {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_RENDER_PASS,
                                        reinterpret_cast<uint64_t>(m_render_pass),
                                        desc.debug_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Created render pass '{}' ({} color attachments, depth={})",
                    desc.debug_name, desc.color_attachments.size(), desc.has_depth);
}

// =========================================================================
//  Destructor
// =========================================================================

VulkanRenderPass::~VulkanRenderPass()
{
    destroy();
}

// =========================================================================
//  Move semantics
// =========================================================================

VulkanRenderPass::VulkanRenderPass(VulkanRenderPass&& other) noexcept
    : m_render_pass(std::exchange(other.m_render_pass, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, nullptr))
    , m_desc(std::move(other.m_desc))
{
}

VulkanRenderPass& VulkanRenderPass::operator=(VulkanRenderPass&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_render_pass = std::exchange(other.m_render_pass, VK_NULL_HANDLE);
        m_device      = std::exchange(other.m_device, nullptr);
        m_desc        = std::move(other.m_desc);
    }
    return *this;
}

// =========================================================================
//  Private
// =========================================================================

void VulkanRenderPass::destroy()
{
    if (m_render_pass != VK_NULL_HANDLE && m_device != nullptr) {
        vkDestroyRenderPass(m_device->device(), m_render_pass, nullptr);
        m_render_pass = VK_NULL_HANDLE;
    }
}

} // namespace helios::rhi::vulkan

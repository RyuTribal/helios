#include "pch.h"
#include "Renderer/Vulkan/VulkanRenderPass.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanUtils.h"

namespace Engine {

    VulkanRenderPass::VulkanRenderPass(VulkanDevice* device, const RenderPassDesc& desc)
        : m_Device(device), m_Desc(desc)
    {
        // -- Build attachment descriptions --
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorRefs;

        for (uint32_t i = 0; i < static_cast<uint32_t>(desc.ColorAttachments.size()); i++) {
            const auto& ca = desc.ColorAttachments[i];

            VkAttachmentDescription attachment{};
            attachment.format         = ToVkFormat(ca.Format);
            attachment.samples        = static_cast<VkSampleCountFlagBits>(ca.Samples);
            attachment.loadOp         = ToVkLoadOp(ca.Load);
            attachment.storeOp        = ToVkStoreOp(ca.Store);
            attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.push_back(attachment);

            VkAttachmentReference ref{};
            ref.attachment = i;
            ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs.push_back(ref);
        }

        // -- Optional depth attachment --
        VkAttachmentReference depthRef{};
        if (desc.HasDepth) {
            const auto& da = desc.DepthAttachment;

            VkAttachmentDescription depthAttachment{};
            depthAttachment.format         = ToVkFormat(da.Format);
            depthAttachment.samples        = static_cast<VkSampleCountFlagBits>(da.Samples);
            depthAttachment.loadOp         = ToVkLoadOp(da.Load);
            depthAttachment.storeOp        = ToVkStoreOp(da.Store);
            depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(depthAttachment);

            depthRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
            depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // -- Single subpass --
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments       = colorRefs.data();
        subpass.pDepthStencilAttachment = desc.HasDepth ? &depthRef : nullptr;

        // -- Subpass dependencies --
        VkSubpassDependency dependency{};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                 | (desc.HasDepth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0u);

        // -- Create render pass --
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpInfo.pAttachments    = attachments.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dependency;

        VkResult result = vkCreateRenderPass(device->GetDevice(), &rpInfo, nullptr, &m_RenderPass);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create render pass '{}'", desc.DebugName);
            return;
        }

        // Debug name
        if (!desc.DebugName.empty()) {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_RENDER_PASS,
                                       reinterpret_cast<uint64_t>(m_RenderPass), desc.DebugName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Created render pass '{}' ({} color attachments, depth={})",
                           desc.DebugName, desc.ColorAttachments.size(), desc.HasDepth);
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        if (m_RenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_Device->GetDevice(), m_RenderPass, nullptr);
            m_RenderPass = VK_NULL_HANDLE;
        }
    }

} // namespace Engine

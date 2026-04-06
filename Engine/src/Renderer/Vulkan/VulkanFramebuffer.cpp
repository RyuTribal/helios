#include "pch.h"
#include "Renderer/Vulkan/VulkanFramebuffer.h"
#include "Renderer/Vulkan/VulkanRenderPass.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"

// NOTE: VulkanTexture does not exist yet (Task 7). Once it does, include its
// header here and extract VkImageView from each attachment in the constructor.
// #include "Renderer/Vulkan/VulkanTexture.h"

namespace Engine {

    VulkanFramebuffer::VulkanFramebuffer(VulkanDevice* device, const FramebufferDesc& desc)
        : m_Device(device), m_Desc(desc)
    {
        // Store the compatible render pass
        if (desc.RenderPass) {
            m_RenderPass = static_cast<VulkanRenderPass*>(desc.RenderPass);
        }

        // TODO (Task 7): Once VulkanTexture is implemented, extract VkImageView
        // from each attachment:
        //
        // for (auto* attachment : desc.Attachments) {
        //     auto* vkTex = static_cast<VulkanTexture*>(attachment);
        //     m_AttachmentViews.push_back(vkTex->GetImageView());
        // }
        //
        // For now the views vector stays empty. Dynamic rendering in the command
        // buffer will check for empty views and skip rendering gracefully.

        HVE_CORE_INFO_TAG("Vulkan", "Created framebuffer '{}' ({}x{}, {} attachments)",
                           desc.DebugName, desc.Width, desc.Height, desc.Attachments.size());
    }

    VulkanFramebuffer::~VulkanFramebuffer()
    {
        if (m_Framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_Device->GetDevice(), m_Framebuffer, nullptr);
            m_Framebuffer = VK_NULL_HANDLE;
        }
    }

    VkFramebuffer VulkanFramebuffer::GetOrCreateVkFramebuffer()
    {
        if (m_Framebuffer != VK_NULL_HANDLE)
            return m_Framebuffer;

        if (!m_RenderPass || m_AttachmentViews.empty()) {
            HVE_CORE_ERROR_TAG("Vulkan", "Cannot create VkFramebuffer: missing render pass or attachment views");
            return VK_NULL_HANDLE;
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_RenderPass->GetVkRenderPass();
        fbInfo.attachmentCount = static_cast<uint32_t>(m_AttachmentViews.size());
        fbInfo.pAttachments    = m_AttachmentViews.data();
        fbInfo.width           = m_Desc.Width;
        fbInfo.height          = m_Desc.Height;
        fbInfo.layers          = 1;

        VkResult result = vkCreateFramebuffer(m_Device->GetDevice(), &fbInfo, nullptr, &m_Framebuffer);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create VkFramebuffer '{}'", m_Desc.DebugName);
            return VK_NULL_HANDLE;
        }

        if (!m_Desc.DebugName.empty()) {
            VulkanContext::SetDebugName(m_Device->GetDevice(), VK_OBJECT_TYPE_FRAMEBUFFER,
                                       reinterpret_cast<uint64_t>(m_Framebuffer), m_Desc.DebugName.c_str());
        }

        return m_Framebuffer;
    }

} // namespace Engine

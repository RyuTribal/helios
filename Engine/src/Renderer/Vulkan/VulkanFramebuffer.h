#pragma once

#include "RHI/RHIResources.h"
#include "RHI/RHITypes.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Engine {

    class VulkanDevice;
    class VulkanRenderPass;

    class VulkanFramebuffer : public RHIFramebuffer {
    public:
        VulkanFramebuffer(VulkanDevice* device, const FramebufferDesc& desc);
        ~VulkanFramebuffer() override;

        uint32_t GetWidth() const override { return m_Desc.Width; }
        uint32_t GetHeight() const override { return m_Desc.Height; }

        // For dynamic rendering -- command buffer uses these directly
        const std::vector<VkImageView>& GetAttachmentViews() const { return m_AttachmentViews; }

        // Legacy framebuffer (created lazily, for ImGui compatibility)
        VkFramebuffer GetOrCreateVkFramebuffer();
        VulkanRenderPass* GetRenderPass() const { return m_RenderPass; }

    private:
        VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
        VulkanDevice* m_Device;
        FramebufferDesc m_Desc;
        VulkanRenderPass* m_RenderPass = nullptr;
        std::vector<VkImageView> m_AttachmentViews;
    };

} // namespace Engine

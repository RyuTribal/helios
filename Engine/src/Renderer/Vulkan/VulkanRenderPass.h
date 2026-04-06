#pragma once

#include "RHI/RHIResources.h"
#include "RHI/RHITypes.h"

#include <vulkan/vulkan.h>

namespace Engine {

    class VulkanDevice;

    class VulkanRenderPass : public RHIRenderPass {
    public:
        VulkanRenderPass(VulkanDevice* device, const RenderPassDesc& desc);
        ~VulkanRenderPass() override;

        VkRenderPass GetVkRenderPass() const { return m_RenderPass; }
        const RenderPassDesc& GetDesc() const { return m_Desc; }

    private:
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        VulkanDevice* m_Device;
        RenderPassDesc m_Desc;
    };

} // namespace Engine

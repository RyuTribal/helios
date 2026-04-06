#pragma once

#include "RHI/RHIResources.h"
#include "RHI/RHITypes.h"

#include <vulkan/vulkan.h>

namespace Engine {

    class VulkanDevice;

    class VulkanPipeline : public RHIPipeline {
    public:
        // Graphics pipeline
        VulkanPipeline(VulkanDevice* device, const GraphicsPipelineDesc& desc);
        // Compute pipeline
        VulkanPipeline(VulkanDevice* device, const ComputePipelineDesc& desc);
        ~VulkanPipeline() override;

        VkPipeline GetVkPipeline() const { return m_Pipeline; }
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        VkPipelineBindPoint GetBindPoint() const { return m_BindPoint; }

    private:
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint m_BindPoint;
        VulkanDevice* m_Device;
    };

} // namespace Engine

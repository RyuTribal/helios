#pragma once

#include "helios/rhi/rhi_pipeline.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII Vulkan pipeline. Supports both graphics and compute.
// Stores VkPipeline + VkPipelineLayout. Destructor destroys both.
class VulkanPipeline : public rhi::Pipeline {
public:
    VulkanPipeline() = default;

    // Graphics pipeline
    VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc);
    // Compute pipeline
    VulkanPipeline(VulkanDevice& device, const ComputePipelineDesc& desc);

    ~VulkanPipeline() override;

    VulkanPipeline(VulkanPipeline&& other) noexcept;
    VulkanPipeline& operator=(VulkanPipeline&& other) noexcept;
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    VkPipeline vk_pipeline() const { return m_pipeline; }
    VkPipelineLayout vk_layout() const { return m_pipeline_layout; }
    VkPipelineBindPoint bind_point() const { return m_bind_point; }

    explicit operator bool() const { return m_pipeline != VK_NULL_HANDLE; }

private:
    void destroy();

    VkPipeline m_pipeline              = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VulkanDevice* m_device             = nullptr;
    VkPipelineBindPoint m_bind_point   = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

} // namespace helios::rhi::vulkan

#pragma once

#include "helios/rhi/rhi_render_pass.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII wrapper around VkRenderPass. Used for pipeline creation compatibility
// and ImGui. Primary rendering uses dynamic rendering (vkCmdBeginRendering).
class VulkanRenderPass : public rhi::RenderPass {
public:
    VulkanRenderPass() = default;
    VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc);
    ~VulkanRenderPass() override;

    VulkanRenderPass(VulkanRenderPass&& other) noexcept;
    VulkanRenderPass& operator=(VulkanRenderPass&& other) noexcept;
    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    VkRenderPass vk_render_pass() const { return m_render_pass; }
    const RenderPassDesc& desc() const override { return m_desc; }

    explicit operator bool() const { return m_render_pass != VK_NULL_HANDLE; }

private:
    void destroy();

    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VulkanDevice* m_device     = nullptr;
    RenderPassDesc m_desc;
};

} // namespace helios::rhi::vulkan

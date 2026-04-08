#pragma once

#include "helios/rhi/rhi_framebuffer.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace helios::rhi::vulkan {

class VulkanDevice;
class VulkanRenderPass;
class VulkanTexture;

// RAII wrapper. Stores attachment views for dynamic rendering.
// Lazily creates VkFramebuffer for legacy render pass compatibility (ImGui).
class VulkanFramebuffer : public rhi::Framebuffer {
public:
    VulkanFramebuffer() = default;
    VulkanFramebuffer(VulkanDevice& device, const VulkanRenderPass* render_pass,
                      const std::vector<const VulkanTexture*>& attachments,
                      uint32_t width, uint32_t height,
                      const char* debug_name = nullptr);
    ~VulkanFramebuffer() override;

    VulkanFramebuffer(VulkanFramebuffer&& other) noexcept;
    VulkanFramebuffer& operator=(VulkanFramebuffer&& other) noexcept;
    VulkanFramebuffer(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

    uint32_t width() const override { return m_width; }
    uint32_t height() const override { return m_height; }
    const std::vector<VkImageView>& attachment_views() const { return m_attachment_views; }

    // Lazy VkFramebuffer creation for legacy render pass path
    VkFramebuffer get_or_create_vk_framebuffer();

    explicit operator bool() const { return !m_attachment_views.empty(); }

private:
    void destroy();

    VkFramebuffer m_framebuffer                = VK_NULL_HANDLE;
    VulkanDevice* m_device                     = nullptr;
    const VulkanRenderPass* m_render_pass      = nullptr;
    std::vector<VkImageView> m_attachment_views;
    uint32_t m_width                           = 0;
    uint32_t m_height                          = 0;
    std::string m_debug_name;
};

} // namespace helios::rhi::vulkan

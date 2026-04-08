#pragma once

#include "helios/rhi/rhi_swapchain.h"
#include "helios/rhi/rhi_command_buffer.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII swapchain. Constructor creates VkSwapchainKHR + sync primitives.
// Destructor destroys everything.
//
// No Resize() method. To resize:
//   device.wait_idle();
//   swapchain = device.create_swapchain(SwapchainDesc{.width=new_w, .height=new_h, ...});
// The move-assignment destroys the old swapchain first.
class VulkanSwapchain : public rhi::Swapchain {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    VulkanSwapchain() = default;
    VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc);
    ~VulkanSwapchain() override;

    VulkanSwapchain(VulkanSwapchain&& other) noexcept;
    VulkanSwapchain& operator=(VulkanSwapchain&& other) noexcept;
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    // Frame operations
    bool acquire_next_image() override;
    void present() override;

    // Accessors
    uint32_t image_count() const override { return static_cast<uint32_t>(m_images.size()); }
    uint32_t current_image_index() const override { return m_current_image_index; }
    uint32_t current_frame() const override { return m_current_frame; }
    uint32_t width() const override { return m_extent.width; }
    uint32_t height() const override { return m_extent.height; }
    VkFormat vk_format() const { return m_image_format; }

    VkSwapchainKHR vk_swapchain() const { return m_swapchain; }
    VkImage current_vk_image() const { return m_images[m_current_image_index]; }
    VkImageView current_image_view() const { return m_image_views[m_current_image_index]; }
    const std::vector<VkImage>& images() const { return m_images; }
    const std::vector<VkImageView>& image_views() const { return m_image_views; }

    // Sync objects for the current frame-in-flight (abstract interface)
    void* image_available_semaphore() const override {
        return m_image_available[m_current_frame];
    }
    void* render_finished_semaphore() const override {
        return m_render_finished[m_current_frame];
    }
    void* in_flight_fence() const override {
        return m_in_flight_fences[m_current_frame];
    }

    // Convenience rendering (abstract interface)
    void begin_rendering(rhi::CommandBuffer& cmd, const ClearValues& clear,
                         rhi::Texture* depth_attachment = nullptr) override;
    void end_rendering(rhi::CommandBuffer& cmd) override;

    explicit operator bool() const { return m_swapchain != VK_NULL_HANDLE; }

private:
    void create_swapchain(const SwapchainDesc& desc);
    void create_sync_objects();
    void destroy();

    VulkanDevice* m_device                = nullptr;
    VkSurfaceKHR m_surface                = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain            = VK_NULL_HANDLE;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_image_views;
    VkFormat m_image_format               = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};

    uint32_t m_current_image_index        = 0;
    uint32_t m_current_frame              = 0;

    VkSemaphore m_image_available[MAX_FRAMES_IN_FLIGHT]{};
    VkSemaphore m_render_finished[MAX_FRAMES_IN_FLIGHT]{};
    VkFence m_in_flight_fences[MAX_FRAMES_IN_FLIGHT]{};
};

} // namespace helios::rhi::vulkan

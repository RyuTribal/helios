#pragma once
#include "RHI/RHISwapchain.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Engine {
    class VulkanDevice;

    class VulkanSwapchain : public RHISwapchain {
    public:
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

        VulkanSwapchain(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
        ~VulkanSwapchain() override;

        // RHISwapchain interface
        bool AcquireNextImage() override;
        void Present() override;
        void Resize(uint32_t width, uint32_t height) override;
        RHITexture* GetCurrentImage() override;
        uint32_t GetImageCount() const override;
        uint32_t GetCurrentImageIndex() const override;
        uint32_t GetWidth() const override;
        uint32_t GetHeight() const override;
        ImageFormat GetFormat() const override;

        // Vulkan-specific accessors
        VkSwapchainKHR GetVkSwapchain() const { return m_Swapchain; }
        VkFormat GetVkFormat() const { return m_ImageFormat; }
        VkImageView GetCurrentImageView() const { return m_ImageViews[m_CurrentImageIndex]; }
        VkImage GetCurrentVkImage() const { return m_Images[m_CurrentImageIndex]; }
        const std::vector<VkImage>& GetImages() const { return m_Images; }
        const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

        // Sync objects for the current frame-in-flight
        VkSemaphore GetImageAvailableSemaphore() const { return m_ImageAvailable[m_CurrentFrame]; }
        VkSemaphore GetRenderFinishedSemaphore() const { return m_RenderFinished[m_CurrentFrame]; }
        VkFence GetInFlightFence() const { return m_InFlightFences[m_CurrentFrame]; }
        uint32_t GetCurrentFrame() const { return m_CurrentFrame; }

    private:
        void CreateSwapchain(uint32_t width, uint32_t height);
        void DestroySwapchain();
        void CreateSyncObjects();
        void DestroySyncObjects();

        VulkanDevice* m_Device = nullptr;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;

        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;
        VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{};

        uint32_t m_CurrentImageIndex = 0;
        uint32_t m_CurrentFrame = 0;

        VkSemaphore m_ImageAvailable[MAX_FRAMES_IN_FLIGHT]{};
        VkSemaphore m_RenderFinished[MAX_FRAMES_IN_FLIGHT]{};
        VkFence m_InFlightFences[MAX_FRAMES_IN_FLIGHT]{};
        bool m_NeedsResize = false;
    };
}

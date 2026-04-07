#include "pch.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"

#include <VkBootstrap.h>

namespace Engine {

    VulkanSwapchain::VulkanSwapchain(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height)
        : m_Device(device), m_Surface(surface)
    {
        CreateSwapchain(width, height);
        CreateSyncObjects();
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        m_Device->WaitIdle();
        DestroySyncObjects();
        DestroySwapchain();
    }

    // ---- Swapchain creation / destruction ----

    void VulkanSwapchain::CreateSwapchain(uint32_t width, uint32_t height)
    {
        vkb::SwapchainBuilder builder(m_Device->GetPhysicalDevice(), m_Device->GetDevice(), m_Surface);
        builder.set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
               .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
               .set_desired_extent(width, height)
               .set_old_swapchain(m_Swapchain);

        auto result = builder.build();
        if (!result) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create swapchain: {}", result.error().message());
            return;
        }

        // Destroy the old swapchain (after build, old handle is no longer needed)
        if (m_Swapchain != VK_NULL_HANDLE) {
            for (auto view : m_ImageViews)
                vkDestroyImageView(m_Device->GetDevice(), view, nullptr);
            vkDestroySwapchainKHR(m_Device->GetDevice(), m_Swapchain, nullptr);
            m_Images.clear();
            m_ImageViews.clear();
        }

        vkb::Swapchain vkbSwapchain = result.value();
        m_Swapchain = vkbSwapchain.swapchain;
        m_ImageFormat = vkbSwapchain.image_format;
        m_Extent = vkbSwapchain.extent;

        auto images = vkbSwapchain.get_images();
        if (!images) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to get swapchain images: {}", images.error().message());
            return;
        }
        m_Images = images.value();

        auto imageViews = vkbSwapchain.get_image_views();
        if (!imageViews) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to get swapchain image views: {}", imageViews.error().message());
            return;
        }
        m_ImageViews = imageViews.value();

        // Name every Vulkan object for validation layer diagnostics
        VkDevice device = m_Device->GetDevice();
        VulkanContext::SetDebugName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR,
            reinterpret_cast<uint64_t>(m_Swapchain), "Swapchain");

        for (uint32_t i = 0; i < m_Images.size(); i++) {
            std::string imgName = "SwapchainImage_" + std::to_string(i);
            std::string viewName = "SwapchainImageView_" + std::to_string(i);
            VulkanContext::SetDebugName(device, VK_OBJECT_TYPE_IMAGE,
                reinterpret_cast<uint64_t>(m_Images[i]), imgName.c_str());
            VulkanContext::SetDebugName(device, VK_OBJECT_TYPE_IMAGE_VIEW,
                reinterpret_cast<uint64_t>(m_ImageViews[i]), viewName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Swapchain created: {}x{}, format={}, {} images",
            m_Extent.width, m_Extent.height, static_cast<int>(m_ImageFormat),
            m_Images.size());
    }

    void VulkanSwapchain::DestroySwapchain()
    {
        VkDevice device = m_Device->GetDevice();

        for (auto view : m_ImageViews)
            vkDestroyImageView(device, view, nullptr);
        m_ImageViews.clear();

        if (m_Swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }

        m_Images.clear();
    }

    // ---- Sync object creation / destruction ----

    void VulkanSwapchain::CreateSyncObjects()
    {
        VkDevice device = m_Device->GetDevice();

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkCreateSemaphore(device, &semInfo, nullptr, &m_ImageAvailable[i]);
            vkCreateSemaphore(device, &semInfo, nullptr, &m_RenderFinished[i]);
            vkCreateFence(device, &fenceInfo, nullptr, &m_InFlightFences[i]);

            std::string semAvailName = "ImageAvailableSem_" + std::to_string(i);
            std::string semFinishName = "RenderFinishedSem_" + std::to_string(i);
            std::string fenceName = "InFlightFence_" + std::to_string(i);

            VulkanContext::SetDebugName(device, VK_OBJECT_TYPE_SEMAPHORE,
                reinterpret_cast<uint64_t>(m_ImageAvailable[i]), semAvailName.c_str());
            VulkanContext::SetDebugName(device, VK_OBJECT_TYPE_SEMAPHORE,
                reinterpret_cast<uint64_t>(m_RenderFinished[i]), semFinishName.c_str());
            VulkanContext::SetDebugName(device, VK_OBJECT_TYPE_FENCE,
                reinterpret_cast<uint64_t>(m_InFlightFences[i]), fenceName.c_str());
        }
    }

    void VulkanSwapchain::DestroySyncObjects()
    {
        VkDevice device = m_Device->GetDevice();

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (m_ImageAvailable[i] != VK_NULL_HANDLE)
                vkDestroySemaphore(device, m_ImageAvailable[i], nullptr);
            if (m_RenderFinished[i] != VK_NULL_HANDLE)
                vkDestroySemaphore(device, m_RenderFinished[i], nullptr);
            if (m_InFlightFences[i] != VK_NULL_HANDLE)
                vkDestroyFence(device, m_InFlightFences[i], nullptr);
        }
    }

    // ---- Frame operations ----

    bool VulkanSwapchain::AcquireNextImage()
    {
        VkDevice device = m_Device->GetDevice();

        vkWaitForFences(device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(device, m_Swapchain, UINT64_MAX,
            m_ImageAvailable[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Swapchain truly out of date — caller must resize
            return false;
        }

        // VK_SUBOPTIMAL_KHR is still a success — the image was acquired, just render it
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to acquire swapchain image: {}", (int)result);
            return false;
        }

        vkResetFences(device, 1, &m_InFlightFences[m_CurrentFrame]);
        return true;
    }

    void VulkanSwapchain::Present()
    {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_RenderFinished[m_CurrentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

        VkResult result = vkQueuePresentKHR(m_Device->GetGraphicsQueue(), &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            HVE_CORE_WARN_TAG("Vulkan", "Swapchain out of date on present, will recreate");
            m_NeedsResize = true;
        }
        // VK_SUBOPTIMAL_KHR is normal on Wayland/some compositors — don't log every frame

        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanSwapchain::Resize(uint32_t width, uint32_t height)
    {
        m_Device->WaitIdle();
        DestroySwapchain();
        CreateSwapchain(width, height);
    }

    // ---- Accessors ----

    RHITexture* VulkanSwapchain::GetCurrentImage()
    {
        // VulkanTexture wrapping comes in Task 7; for now Vulkan backend
        // accesses swapchain images directly via GetCurrentVkImage()/GetCurrentImageView()
        return nullptr;
    }

    uint32_t VulkanSwapchain::GetImageCount() const
    {
        return static_cast<uint32_t>(m_Images.size());
    }

    uint32_t VulkanSwapchain::GetCurrentImageIndex() const
    {
        return m_CurrentImageIndex;
    }

    uint32_t VulkanSwapchain::GetWidth() const
    {
        return m_Extent.width;
    }

    uint32_t VulkanSwapchain::GetHeight() const
    {
        return m_Extent.height;
    }

    ImageFormat VulkanSwapchain::GetFormat() const
    {
        // VK_FORMAT_B8G8R8A8_SRGB maps closest to RGBA8
        switch (m_ImageFormat) {
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_R8G8B8A8_UNORM:
                return ImageFormat::RGBA8;
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return ImageFormat::RGBA16F;
            default:
                return ImageFormat::RGBA8;
        }
    }

}

#include "helios/vulkan/vulkan_swapchain.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_utils.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <VkBootstrap.h>
#include <string>
#include <utility>

namespace helios::rhi::vulkan {

// =========================================================================
//  Constructor
// =========================================================================

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc)
    : m_device(&device)
    , m_surface(static_cast<VkSurfaceKHR>(desc.surface))
{
    HELIOS_ASSERT(device, "VulkanDevice must be initialized");
    HELIOS_ASSERT(desc.surface != nullptr, "Surface must not be null");
    HELIOS_ASSERT(desc.width > 0 && desc.height > 0, "Swapchain dimensions must be > 0");

    create_swapchain(desc);
    create_sync_objects();
}

// =========================================================================
//  Destructor
// =========================================================================

VulkanSwapchain::~VulkanSwapchain()
{
    destroy();
}

// =========================================================================
//  Move semantics
// =========================================================================

VulkanSwapchain::VulkanSwapchain(VulkanSwapchain&& other) noexcept
    : m_device(std::exchange(other.m_device, nullptr))
    , m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE))
    , m_swapchain(std::exchange(other.m_swapchain, VK_NULL_HANDLE))
    , m_images(std::move(other.m_images))
    , m_image_views(std::move(other.m_image_views))
    , m_image_format(std::exchange(other.m_image_format, VK_FORMAT_UNDEFINED))
    , m_extent(std::exchange(other.m_extent, VkExtent2D{}))
    , m_current_image_index(std::exchange(other.m_current_image_index, 0))
    , m_current_frame(std::exchange(other.m_current_frame, 0))
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_image_available[i]  = std::exchange(other.m_image_available[i], VK_NULL_HANDLE);
        m_render_finished[i]  = std::exchange(other.m_render_finished[i], VK_NULL_HANDLE);
        m_in_flight_fences[i] = std::exchange(other.m_in_flight_fences[i], VK_NULL_HANDLE);
    }
}

VulkanSwapchain& VulkanSwapchain::operator=(VulkanSwapchain&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_device              = std::exchange(other.m_device, nullptr);
        m_surface             = std::exchange(other.m_surface, VK_NULL_HANDLE);
        m_swapchain           = std::exchange(other.m_swapchain, VK_NULL_HANDLE);
        m_images              = std::move(other.m_images);
        m_image_views         = std::move(other.m_image_views);
        m_image_format        = std::exchange(other.m_image_format, VK_FORMAT_UNDEFINED);
        m_extent              = std::exchange(other.m_extent, VkExtent2D{});
        m_current_image_index = std::exchange(other.m_current_image_index, 0);
        m_current_frame       = std::exchange(other.m_current_frame, 0);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_image_available[i]  = std::exchange(other.m_image_available[i], VK_NULL_HANDLE);
            m_render_finished[i]  = std::exchange(other.m_render_finished[i], VK_NULL_HANDLE);
            m_in_flight_fences[i] = std::exchange(other.m_in_flight_fences[i], VK_NULL_HANDLE);
        }
    }
    return *this;
}

// =========================================================================
//  Frame operations
// =========================================================================

bool VulkanSwapchain::acquire_next_image()
{
    HELIOS_ASSERT(m_device != nullptr, "Swapchain not initialized");
    HELIOS_ASSERT(m_swapchain != VK_NULL_HANDLE, "Swapchain not created");

    VkDevice device = m_device->device();

    vkWaitForFences(device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX,
        m_image_available[m_current_frame], VK_NULL_HANDLE, &m_current_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain truly out of date -- caller must recreate
        return false;
    }

    // VK_SUBOPTIMAL_KHR is still a success -- the image was acquired, just render it
    // This is common on Wayland compositors
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        HELIOS_LOG_ERROR(Renderer, "Failed to acquire swapchain image: {}", static_cast<int>(result));
        return false;
    }

    vkResetFences(device, 1, &m_in_flight_fences[m_current_frame]);
    return true;
}

void VulkanSwapchain::present()
{
    HELIOS_ASSERT(m_device != nullptr, "Swapchain not initialized");

    VkPresentInfoKHR present_info{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &m_render_finished[m_current_frame];
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &m_swapchain;
    present_info.pImageIndices      = &m_current_image_index;

    VkResult result = vkQueuePresentKHR(m_device->graphics_queue(), &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        HELIOS_LOG_WARN(Renderer, "Swapchain out of date on present, will recreate");
    }
    // VK_SUBOPTIMAL_KHR is normal on Wayland/some compositors -- don't log every frame

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// =========================================================================
//  Private: create swapchain
// =========================================================================

void VulkanSwapchain::create_swapchain(const SwapchainDesc& desc)
{
    vkb::SwapchainBuilder builder(m_device->physical_device(), m_device->device(), m_surface);
    builder.set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
           .set_desired_present_mode(to_vk_present_mode(desc.present_mode))
           .set_desired_extent(desc.width, desc.height)
           .set_old_swapchain(m_swapchain);

    auto result = builder.build();
    if (!result) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create swapchain: {}", result.error().message());
        return;
    }

    // Destroy the old swapchain (after build, old handle is no longer needed)
    if (m_swapchain != VK_NULL_HANDLE) {
        for (auto view : m_image_views)
            vkDestroyImageView(m_device->device(), view, nullptr);
        vkDestroySwapchainKHR(m_device->device(), m_swapchain, nullptr);
        m_images.clear();
        m_image_views.clear();
    }

    vkb::Swapchain vkb_swapchain = result.value();
    m_swapchain   = vkb_swapchain.swapchain;
    m_image_format = vkb_swapchain.image_format;
    m_extent      = vkb_swapchain.extent;

    auto images = vkb_swapchain.get_images();
    if (!images) {
        HELIOS_LOG_ERROR(Renderer, "Failed to get swapchain images: {}", images.error().message());
        return;
    }
    m_images = images.value();

    auto image_views = vkb_swapchain.get_image_views();
    if (!image_views) {
        HELIOS_LOG_ERROR(Renderer, "Failed to get swapchain image views: {}", image_views.error().message());
        return;
    }
    m_image_views = image_views.value();

    // Name every Vulkan object for validation layer diagnostics
    VkDevice device = m_device->device();
    m_device->context().set_debug_name(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR,
        reinterpret_cast<uint64_t>(m_swapchain), "Swapchain");

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_images.size()); i++) {
        std::string img_name  = "SwapchainImage_" + std::to_string(i);
        std::string view_name = "SwapchainImageView_" + std::to_string(i);
        m_device->context().set_debug_name(device, VK_OBJECT_TYPE_IMAGE,
            reinterpret_cast<uint64_t>(m_images[i]), img_name.c_str());
        m_device->context().set_debug_name(device, VK_OBJECT_TYPE_IMAGE_VIEW,
            reinterpret_cast<uint64_t>(m_image_views[i]), view_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Swapchain created: {}x{}, format={}, {} images",
        m_extent.width, m_extent.height, static_cast<int>(m_image_format),
        m_images.size());
}

// =========================================================================
//  Private: create sync objects
// =========================================================================

void VulkanSwapchain::create_sync_objects()
{
    VkDevice device = m_device->device();

    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(device, &sem_info, nullptr, &m_image_available[i]);
        vkCreateSemaphore(device, &sem_info, nullptr, &m_render_finished[i]);
        vkCreateFence(device, &fence_info, nullptr, &m_in_flight_fences[i]);

        std::string sem_avail_name  = "ImageAvailableSem_" + std::to_string(i);
        std::string sem_finish_name = "RenderFinishedSem_" + std::to_string(i);
        std::string fence_name      = "InFlightFence_" + std::to_string(i);

        m_device->context().set_debug_name(device, VK_OBJECT_TYPE_SEMAPHORE,
            reinterpret_cast<uint64_t>(m_image_available[i]), sem_avail_name.c_str());
        m_device->context().set_debug_name(device, VK_OBJECT_TYPE_SEMAPHORE,
            reinterpret_cast<uint64_t>(m_render_finished[i]), sem_finish_name.c_str());
        m_device->context().set_debug_name(device, VK_OBJECT_TYPE_FENCE,
            reinterpret_cast<uint64_t>(m_in_flight_fences[i]), fence_name.c_str());
    }
}

// =========================================================================
//  Private: destroy
// =========================================================================

void VulkanSwapchain::destroy()
{
    if (m_device == nullptr) return;

    VkDevice device = m_device->device();

    // Destroy sync objects
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_image_available[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, m_image_available[i], nullptr);
        if (m_render_finished[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, m_render_finished[i], nullptr);
        if (m_in_flight_fences[i] != VK_NULL_HANDLE)
            vkDestroyFence(device, m_in_flight_fences[i], nullptr);

        m_image_available[i]  = VK_NULL_HANDLE;
        m_render_finished[i]  = VK_NULL_HANDLE;
        m_in_flight_fences[i] = VK_NULL_HANDLE;
    }

    // Destroy image views
    for (auto view : m_image_views)
        vkDestroyImageView(device, view, nullptr);
    m_image_views.clear();

    // Destroy swapchain
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    m_images.clear();
}

} // namespace helios::rhi::vulkan

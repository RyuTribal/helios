#include "helios/vulkan/vulkan_swapchain.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_command_buffer.h"
#include "helios/vulkan/vulkan_texture.h"
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
    if (!m_device || m_swapchain == VK_NULL_HANDLE) return false;

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
        // Flush all GPU work immediately. On Wayland, a failed present can
        // leave the surface in a state that causes wl_display_flush() to fail
        // on the next glfwPollEvents(), which GLFW interprets as a disconnect
        // and closes all windows.
        vkDeviceWaitIdle(m_device->device());
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
//  Convenience rendering helpers
// =========================================================================

static void transition_image(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout old_layout, VkImageLayout new_layout,
                             VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                             VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask        = src_stage;
    barrier.srcAccessMask       = src_access;
    barrier.dstStageMask        = dst_stage;
    barrier.dstAccessMask       = dst_access;
    barrier.oldLayout           = old_layout;
    barrier.newLayout           = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkDependencyInfo dep{};
    dep.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount  = 1;
    dep.pImageMemoryBarriers     = &barrier;

    vkCmdPipelineBarrier2(cmd, &dep);
}

TextureFormat VulkanSwapchain::color_format() const
{
    return from_vk_format(m_image_format);
}

void VulkanSwapchain::begin_rendering(rhi::CommandBuffer& cmd, const ClearValues& clear,
                                      rhi::Texture* depth_attachment)
{
    HELIOS_ASSERT(m_device != nullptr, "Swapchain not initialized");

    const auto& vk_cmd = static_cast<const VulkanCommandBuffer&>(cmd);
    VkCommandBuffer raw_cmd = vk_cmd.vk_command_buffer();
    VkImage image = m_images[m_current_image_index];

    // Transition UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
    transition_image(raw_cmd, image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    // Begin dynamic rendering with clear color
    VkRenderingAttachmentInfo color_att{};
    color_att.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_att.imageView   = m_image_views[m_current_image_index];
    color_att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_att.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.clearValue.color = {{clear.color[0], clear.color[1], clear.color[2], clear.color[3]}};

    // Optional depth attachment
    VkRenderingAttachmentInfo depth_att{};
    if (depth_attachment) {
        auto* vk_depth = static_cast<VulkanTexture*>(depth_attachment);

        // Transition depth image from UNDEFINED to DEPTH_ATTACHMENT_OPTIMAL
        VulkanTexture::transition_layout(raw_cmd, vk_depth->vk_image(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        vk_depth->set_current_layout(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        depth_att.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_att.imageView   = vk_depth->vk_image_view();
        depth_att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_att.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_att.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_att.clearValue.depthStencil = {clear.depth, clear.stencil};
    }

    VkRenderingInfo rendering{};
    rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea           = {{0, 0}, m_extent};
    rendering.layerCount           = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments    = &color_att;
    if (depth_attachment) {
        rendering.pDepthAttachment = &depth_att;
    }

    vkCmdBeginRendering(raw_cmd, &rendering);
}

void VulkanSwapchain::end_rendering(rhi::CommandBuffer& cmd)
{
    HELIOS_ASSERT(m_device != nullptr, "Swapchain not initialized");

    const auto& vk_cmd = static_cast<const VulkanCommandBuffer&>(cmd);
    VkCommandBuffer raw_cmd = vk_cmd.vk_command_buffer();
    VkImage image = m_images[m_current_image_index];

    vkCmdEndRendering(raw_cmd);

    // Transition COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
    transition_image(raw_cmd, image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
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

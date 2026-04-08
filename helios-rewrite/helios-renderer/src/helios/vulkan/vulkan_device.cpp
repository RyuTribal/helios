#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_buffer.h"
#include "helios/vulkan/vulkan_shader.h"
#include "helios/vulkan/vulkan_texture.h"
#include "helios/vulkan/vulkan_pipeline.h"
#include "helios/vulkan/vulkan_command_buffer.h"
#include "helios/vulkan/vulkan_swapchain.h"
#include "helios/vulkan/vulkan_descriptor.h"
#include "helios/vulkan/vulkan_render_pass.h"
#include "helios/vulkan/vulkan_framebuffer.h"
#include "helios/vulkan/vulkan_utils.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <VkBootstrap.h>
#include <utility>
#include <vector>

namespace helios::rhi::vulkan {

VulkanDevice::VulkanDevice(VulkanContext& context, VkSurfaceKHR surface)
    : m_context(&context)
{
    HELIOS_ASSERT(context, "VulkanContext must be initialized");
    HELIOS_ASSERT(surface != VK_NULL_HANDLE, "VkSurfaceKHR must not be null");

    // --- Physical device selection ---
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    features13.maintenance4 = VK_TRUE;

    vkb::PhysicalDeviceSelector selector(context.vkb_instance(), surface);
    selector.set_minimum_version(1, 3)
            .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
            .set_required_features_13(features13);

    auto phys_result = selector.select();
    if (!phys_result) {
        HELIOS_LOG_ERROR(Renderer, "Failed to select physical device: {}",
                         phys_result.error().message());
        return;
    }

    vkb::PhysicalDevice physical_device = phys_result.value();

    // Log all available GPUs
    auto all_devices = selector.select_device_names();
    if (all_devices) {
        HELIOS_LOG_INFO(Renderer, "Available GPUs:");
        for (const auto& name : all_devices.value())
            HELIOS_LOG_INFO(Renderer, "  - {}", name);
    }

    // Log selected device properties
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device.physical_device, &props);
    const char* device_type_str = "Unknown";
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   device_type_str = "Discrete GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type_str = "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    device_type_str = "Virtual GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            device_type_str = "CPU"; break;
        default: break;
    }
    HELIOS_LOG_INFO(Renderer, "Selected GPU: {} ({})", physical_device.name, device_type_str);
    HELIOS_LOG_INFO(Renderer, "  Vulkan API: {}.{}.{}",
        VK_API_VERSION_MAJOR(props.apiVersion),
        VK_API_VERSION_MINOR(props.apiVersion),
        VK_API_VERSION_PATCH(props.apiVersion));
    HELIOS_LOG_INFO(Renderer, "  Driver: {}.{}.{}",
        VK_API_VERSION_MAJOR(props.driverVersion),
        VK_API_VERSION_MINOR(props.driverVersion),
        VK_API_VERSION_PATCH(props.driverVersion));

    // --- Logical device creation ---
    vkb::DeviceBuilder device_builder(physical_device);
    auto dev_result = device_builder.build();
    if (!dev_result) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create logical device: {}",
                         dev_result.error().message());
        return;
    }

    vkb::Device vkb_device = dev_result.value();
    m_device = vkb_device.device;
    m_physical_device = physical_device.physical_device;

    // --- Queue retrieval ---
    auto graphics_queue_result = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!graphics_queue_result) {
        HELIOS_LOG_ERROR(Renderer, "Failed to get graphics queue: {}",
                         graphics_queue_result.error().message());
        return;
    }
    m_graphics_queue = graphics_queue_result.value();

    auto graphics_queue_index_result = vkb_device.get_queue_index(vkb::QueueType::graphics);
    if (!graphics_queue_index_result) {
        HELIOS_LOG_ERROR(Renderer, "Failed to get graphics queue family index: {}",
                         graphics_queue_index_result.error().message());
        return;
    }
    m_graphics_queue_family = graphics_queue_index_result.value();

    // --- VMA allocator ---
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.physicalDevice = m_physical_device;
    allocator_info.device = m_device;
    allocator_info.instance = context.instance();
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateAllocator(&allocator_info, &m_allocator);

    // --- Immediate submit resources ---
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = m_graphics_queue_family;
    vkCreateCommandPool(m_device, &pool_info, nullptr, &m_immediate_cmd_pool);

    VkCommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.commandPool = m_immediate_cmd_pool;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &m_immediate_cmd_buffer);

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(m_device, &fence_info, nullptr, &m_immediate_fence);

    // --- Descriptor pool ---
    std::vector<VkDescriptorPoolSize> pool_sizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           20 },
    };

    VkDescriptorPoolCreateInfo descriptor_pool_info{};
    descriptor_pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.maxSets       = 200;
    descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    descriptor_pool_info.pPoolSizes    = pool_sizes.data();
    descriptor_pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_descriptor_pool);

    // --- Debug names ---
    context.set_debug_name(m_device, VK_OBJECT_TYPE_DEVICE,
        reinterpret_cast<uint64_t>(m_device), "MainDevice");
    context.set_debug_name(m_device, VK_OBJECT_TYPE_QUEUE,
        reinterpret_cast<uint64_t>(m_graphics_queue), "GraphicsQueue");
    context.set_debug_name(m_device, VK_OBJECT_TYPE_COMMAND_POOL,
        reinterpret_cast<uint64_t>(m_immediate_cmd_pool), "ImmediateCommandPool");
    context.set_debug_name(m_device, VK_OBJECT_TYPE_FENCE,
        reinterpret_cast<uint64_t>(m_immediate_fence), "ImmediateFence");
    context.set_debug_name(m_device, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        reinterpret_cast<uint64_t>(m_descriptor_pool), "MainDescriptorPool");

    HELIOS_LOG_INFO(Renderer, "Vulkan device and VMA allocator created");
}

VulkanDevice::~VulkanDevice()
{
    destroy();
}

VulkanDevice::VulkanDevice(VulkanDevice&& other) noexcept
    : m_context(std::exchange(other.m_context, nullptr))
    , m_physical_device(std::exchange(other.m_physical_device, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, VK_NULL_HANDLE))
    , m_graphics_queue(std::exchange(other.m_graphics_queue, VK_NULL_HANDLE))
    , m_graphics_queue_family(std::exchange(other.m_graphics_queue_family, 0))
    , m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE))
    , m_descriptor_pool(std::exchange(other.m_descriptor_pool, VK_NULL_HANDLE))
    , m_immediate_cmd_pool(std::exchange(other.m_immediate_cmd_pool, VK_NULL_HANDLE))
    , m_immediate_cmd_buffer(std::exchange(other.m_immediate_cmd_buffer, VK_NULL_HANDLE))
    , m_immediate_fence(std::exchange(other.m_immediate_fence, VK_NULL_HANDLE))
{
}

VulkanDevice& VulkanDevice::operator=(VulkanDevice&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_context              = std::exchange(other.m_context, nullptr);
        m_physical_device      = std::exchange(other.m_physical_device, VK_NULL_HANDLE);
        m_device               = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_graphics_queue       = std::exchange(other.m_graphics_queue, VK_NULL_HANDLE);
        m_graphics_queue_family = std::exchange(other.m_graphics_queue_family, 0);
        m_allocator            = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_descriptor_pool      = std::exchange(other.m_descriptor_pool, VK_NULL_HANDLE);
        m_immediate_cmd_pool   = std::exchange(other.m_immediate_cmd_pool, VK_NULL_HANDLE);
        m_immediate_cmd_buffer = std::exchange(other.m_immediate_cmd_buffer, VK_NULL_HANDLE);
        m_immediate_fence      = std::exchange(other.m_immediate_fence, VK_NULL_HANDLE);
    }
    return *this;
}

void VulkanDevice::destroy()
{
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);

        if (m_immediate_fence != VK_NULL_HANDLE)
            vkDestroyFence(m_device, m_immediate_fence, nullptr);

        if (m_immediate_cmd_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(m_device, m_immediate_cmd_pool, nullptr);

        if (m_descriptor_pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

        if (m_allocator != VK_NULL_HANDLE)
            vmaDestroyAllocator(m_allocator);

        vkDestroyDevice(m_device, nullptr);

        m_immediate_fence      = VK_NULL_HANDLE;
        m_immediate_cmd_pool   = VK_NULL_HANDLE;
        m_immediate_cmd_buffer = VK_NULL_HANDLE;
        m_descriptor_pool      = VK_NULL_HANDLE;
        m_allocator            = VK_NULL_HANDLE;
        m_device               = VK_NULL_HANDLE;
        m_physical_device      = VK_NULL_HANDLE;
        m_graphics_queue       = VK_NULL_HANDLE;

        HELIOS_LOG_INFO(Renderer, "Vulkan device destroyed");
    }
}

void VulkanDevice::wait_idle()
{
    HELIOS_ASSERT(m_device != VK_NULL_HANDLE, "Device not initialized");
    vkDeviceWaitIdle(m_device);
}

void VulkanDevice::immediate_submit(std::function<void(VkCommandBuffer)>&& fn)
{
    HELIOS_ASSERT(m_device != VK_NULL_HANDLE, "Device not initialized");
    HELIOS_ASSERT(m_immediate_fence != VK_NULL_HANDLE, "Immediate fence not created");

    vkWaitForFences(m_device, 1, &m_immediate_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_immediate_fence);

    vkResetCommandBuffer(m_immediate_cmd_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_immediate_cmd_buffer, &begin_info);

    fn(m_immediate_cmd_buffer);

    vkEndCommandBuffer(m_immediate_cmd_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_immediate_cmd_buffer;
    vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_immediate_fence);

    vkWaitForFences(m_device, 1, &m_immediate_fence, VK_TRUE, UINT64_MAX);
}

// --- Factory methods ---

VulkanTexture VulkanDevice::create_texture(const TextureDesc& desc, const void* data)
{
    return VulkanTexture(*this, desc, data);
}

VulkanBuffer VulkanDevice::create_buffer(const BufferDesc& desc, const void* data)
{
    return VulkanBuffer(*this, desc, data);
}

VulkanShader VulkanDevice::create_shader(const ShaderDesc& desc)
{
    return VulkanShader(*this, desc);
}

VulkanPipeline VulkanDevice::create_graphics_pipeline(const GraphicsPipelineDesc& desc)
{
    return VulkanPipeline(*this, desc);
}

VulkanPipeline VulkanDevice::create_compute_pipeline(const ComputePipelineDesc& desc)
{
    return VulkanPipeline(*this, desc);
}

VulkanCommandBuffer VulkanDevice::create_command_buffer()
{
    return VulkanCommandBuffer(*this);
}

VulkanSwapchain VulkanDevice::create_swapchain(const SwapchainDesc& desc)
{
    return VulkanSwapchain(*this, desc);
}

VulkanDescriptorSetLayout VulkanDevice::create_descriptor_set_layout(
    const DescriptorSetLayoutDesc& desc)
{
    return VulkanDescriptorSetLayout(*this, desc);
}

VulkanDescriptorSet VulkanDevice::create_descriptor_set(
    const VulkanDescriptorSetLayout& layout)
{
    VkDescriptorSetLayout dsl = layout.vk_layout();

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool     = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts        = &dsl;

    VkDescriptorSet set;
    VkResult result = vkAllocateDescriptorSets(m_device, &alloc_info, &set);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to allocate descriptor set");
        return VulkanDescriptorSet{};  // return empty/null
    }

    return VulkanDescriptorSet(*this, set);
}

VulkanRenderPass VulkanDevice::create_render_pass(const RenderPassDesc& desc)
{
    return VulkanRenderPass(*this, desc);
}

VulkanFramebuffer VulkanDevice::create_framebuffer(const FramebufferDesc& desc)
{
    // FramebufferDesc in the new API is minimal; actual construction uses
    // the overload in VulkanFramebuffer that takes attachments directly.
    // This factory is a convenience for the simple case.
    (void)desc;
    return VulkanFramebuffer{};
}

void VulkanDevice::update_descriptor_set(VulkanDescriptorSet& set,
                                         const std::vector<DescriptorWrite>& writes)
{
    HELIOS_ASSERT(m_device != VK_NULL_HANDLE, "Device not initialized");
    HELIOS_ASSERT(set, "Descriptor set must be valid");

    std::vector<VkWriteDescriptorSet> vk_writes;
    // Keep buffer/image info alive until vkUpdateDescriptorSets completes
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    std::vector<VkDescriptorImageInfo> image_infos;
    buffer_infos.reserve(writes.size());
    image_infos.reserve(writes.size());

    for (const auto& write : writes) {
        VkWriteDescriptorSet vk_write{};
        vk_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vk_write.dstSet          = set.vk_set();
        vk_write.dstBinding      = write.binding;
        vk_write.dstArrayElement = 0;
        vk_write.descriptorCount = 1;
        vk_write.descriptorType  = to_vk_descriptor_type(write.type);

        if (write.type == DescriptorType::UniformBuffer ||
            write.type == DescriptorType::StorageBuffer)
        {
            auto* buffer = static_cast<VulkanBuffer*>(write.buffer_handle);
            HELIOS_ASSERT(buffer != nullptr, "Buffer handle must not be null for buffer descriptor");

            VkDescriptorBufferInfo buf_info{};
            buf_info.buffer = buffer->vk_buffer();
            buf_info.offset = write.offset;
            buf_info.range  = write.range == 0 ? VK_WHOLE_SIZE : write.range;
            buffer_infos.push_back(buf_info);
            vk_write.pBufferInfo = &buffer_infos.back();
        }
        else if (write.type == DescriptorType::CombinedImageSampler ||
                 write.type == DescriptorType::StorageImage)
        {
            auto* texture = static_cast<VulkanTexture*>(write.texture_handle);
            HELIOS_ASSERT(texture != nullptr, "Texture handle must not be null for image descriptor");

            VkDescriptorImageInfo img_info{};
            img_info.imageView   = texture->vk_image_view();
            img_info.sampler     = texture->vk_sampler();
            img_info.imageLayout = (write.type == DescriptorType::StorageImage)
                                 ? VK_IMAGE_LAYOUT_GENERAL
                                 : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos.push_back(img_info);
            vk_write.pImageInfo = &image_infos.back();
        }

        vk_writes.push_back(vk_write);
    }

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(vk_writes.size()),
                           vk_writes.data(), 0, nullptr);
}

void VulkanDevice::submit(const VulkanCommandBuffer& cmd, const SubmitInfo& info)
{
    VkCommandBuffer vk_cmd = cmd.vk_command_buffer();

    auto wait_sem   = static_cast<VkSemaphore>(info.wait_semaphore);
    auto signal_sem = static_cast<VkSemaphore>(info.signal_semaphore);
    auto fence      = static_cast<VkFence>(info.fence);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info{};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &vk_cmd;

    if (wait_sem) {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores    = &wait_sem;
        submit_info.pWaitDstStageMask  = &wait_stage;
    }

    if (signal_sem) {
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &signal_sem;
    }

    vkQueueSubmit(m_graphics_queue, 1, &submit_info,
                  fence ? fence : VK_NULL_HANDLE);
}

} // namespace helios::rhi::vulkan

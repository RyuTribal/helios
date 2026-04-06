#include "pch.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanShader.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"

#include <VkBootstrap.h>

namespace Engine {

    VulkanDevice::VulkanDevice(VkSurfaceKHR surface)
    {
        // --- Physical device selection ---
        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;
        features13.maintenance4 = VK_TRUE;

        vkb::PhysicalDeviceSelector selector(VulkanContext::GetVkbInstance(), surface);
        selector.set_minimum_version(1, 3)
                .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                .set_required_features_13(features13);

        auto physResult = selector.select();
        if (!physResult) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to select physical device: {}", physResult.error().message());
            return;
        }

        vkb::PhysicalDevice physicalDevice = physResult.value();

        // Log all available GPUs so the user knows what was considered
        auto allDevices = selector.select_device_names();
        if (allDevices) {
            HVE_CORE_INFO_TAG("Vulkan", "Available GPUs:");
            for (const auto& name : allDevices.value())
                HVE_CORE_INFO_TAG("Vulkan", "  - {}", name);
        }

        // Log selected device properties
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice.physical_device, &props);
        const char* deviceType = "Unknown";
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   deviceType = "Discrete GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    deviceType = "Virtual GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            deviceType = "CPU"; break;
            default: break;
        }
        HVE_CORE_INFO_TAG("Vulkan", "Selected GPU: {} ({})", physicalDevice.name, deviceType);
        HVE_CORE_INFO_TAG("Vulkan", "  Vulkan API: {}.{}.{}",
            VK_API_VERSION_MAJOR(props.apiVersion),
            VK_API_VERSION_MINOR(props.apiVersion),
            VK_API_VERSION_PATCH(props.apiVersion));
        HVE_CORE_INFO_TAG("Vulkan", "  Driver: {}.{}.{}",
            VK_API_VERSION_MAJOR(props.driverVersion),
            VK_API_VERSION_MINOR(props.driverVersion),
            VK_API_VERSION_PATCH(props.driverVersion));

        // --- Logical device creation ---
        vkb::DeviceBuilder deviceBuilder(physicalDevice);
        auto devResult = deviceBuilder.build();
        if (!devResult) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create logical device: {}", devResult.error().message());
            return;
        }

        vkb::Device vkbDevice = devResult.value();
        m_Device = vkbDevice.device;
        m_PhysicalDevice = physicalDevice.physical_device;

        // --- Queue retrieval ---
        auto graphicsQueueResult = vkbDevice.get_queue(vkb::QueueType::graphics);
        if (!graphicsQueueResult) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to get graphics queue: {}", graphicsQueueResult.error().message());
            return;
        }
        m_GraphicsQueue = graphicsQueueResult.value();

        auto graphicsQueueIndexResult = vkbDevice.get_queue_index(vkb::QueueType::graphics);
        if (!graphicsQueueIndexResult) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to get graphics queue family index: {}", graphicsQueueIndexResult.error().message());
            return;
        }
        m_GraphicsQueueFamily = graphicsQueueIndexResult.value();

        // --- VMA allocator ---
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = m_PhysicalDevice;
        allocatorInfo.device = m_Device;
        allocatorInfo.instance = VulkanContext::GetInstance();
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        vmaCreateAllocator(&allocatorInfo, &m_Allocator);

        // --- Immediate submit resources ---
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;
        vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_ImmediateCommandPool);

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = m_ImmediateCommandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_ImmediateCommandBuffer);

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(m_Device, &fenceInfo, nullptr, &m_ImmediateFence);

        // --- Debug names ---
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_DEVICE, (uint64_t)m_Device, "MainDevice");
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_QUEUE, (uint64_t)m_GraphicsQueue, "GraphicsQueue");
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)m_ImmediateCommandPool, "ImmediateCommandPool");
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_FENCE, (uint64_t)m_ImmediateFence, "ImmediateFence");

        HVE_CORE_INFO_TAG("Vulkan", "Vulkan device and VMA allocator created");
    }

    VulkanDevice::~VulkanDevice()
    {
        if (m_Device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_Device);

            if (m_ImmediateFence != VK_NULL_HANDLE)
                vkDestroyFence(m_Device, m_ImmediateFence, nullptr);

            if (m_ImmediateCommandPool != VK_NULL_HANDLE)
                vkDestroyCommandPool(m_Device, m_ImmediateCommandPool, nullptr);

            if (m_Allocator != VK_NULL_HANDLE)
                vmaDestroyAllocator(m_Allocator);

            vkDestroyDevice(m_Device, nullptr);
            HVE_CORE_INFO_TAG("Vulkan", "Vulkan device destroyed");
        }
    }

    void VulkanDevice::WaitIdle()
    {
        vkDeviceWaitIdle(m_Device);
    }

    void VulkanDevice::ImmediateSubmit(std::function<void(VkCommandBuffer)>&& function)
    {
        vkWaitForFences(m_Device, 1, &m_ImmediateFence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_Device, 1, &m_ImmediateFence);

        vkResetCommandBuffer(m_ImmediateCommandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(m_ImmediateCommandBuffer, &beginInfo);

        function(m_ImmediateCommandBuffer);

        vkEndCommandBuffer(m_ImmediateCommandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_ImmediateCommandBuffer;
        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_ImmediateFence);

        vkWaitForFences(m_Device, 1, &m_ImmediateFence, VK_TRUE, UINT64_MAX);
    }

    // --- Stub implementations ---

    Ref<RHITexture> VulkanDevice::CreateTexture(const TextureDesc& desc, const void* initialData)
    {
        HVE_CORE_WARN_TAG("Vulkan", "CreateTexture not yet implemented");
        return nullptr;
    }

    Ref<RHIBuffer> VulkanDevice::CreateBuffer(const BufferDesc& desc, const void* initialData)
    {
        return CreateRef<VulkanBuffer>(this, desc, initialData);
    }

    Ref<RHIShader> VulkanDevice::CreateShader(const ShaderDesc& desc)
    {
        return CreateRef<VulkanShader>(this, desc);
    }

    Ref<RHIPipeline> VulkanDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
    {
        HVE_CORE_WARN_TAG("Vulkan", "CreateGraphicsPipeline not yet implemented");
        return nullptr;
    }

    Ref<RHIPipeline> VulkanDevice::CreateComputePipeline(const ComputePipelineDesc& desc)
    {
        HVE_CORE_WARN_TAG("Vulkan", "CreateComputePipeline not yet implemented");
        return nullptr;
    }

    Ref<RHIRenderPass> VulkanDevice::CreateRenderPass(const RenderPassDesc& desc)
    {
        HVE_CORE_WARN_TAG("Vulkan", "CreateRenderPass not yet implemented");
        return nullptr;
    }

    Ref<RHIFramebuffer> VulkanDevice::CreateFramebuffer(const FramebufferDesc& desc)
    {
        HVE_CORE_WARN_TAG("Vulkan", "CreateFramebuffer not yet implemented");
        return nullptr;
    }

    Ref<RHIDescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
    {
        HVE_CORE_WARN_TAG("Vulkan", "CreateDescriptorSetLayout not yet implemented");
        return nullptr;
    }

    Ref<RHIDescriptorSet> VulkanDevice::AllocateDescriptorSet(RHIDescriptorSetLayout* layout)
    {
        HVE_CORE_WARN_TAG("Vulkan", "AllocateDescriptorSet not yet implemented");
        return nullptr;
    }

    void VulkanDevice::UpdateDescriptorSet(RHIDescriptorSet* set, const std::vector<DescriptorWrite>& writes)
    {
        HVE_CORE_WARN_TAG("Vulkan", "UpdateDescriptorSet not yet implemented");
    }

    Ref<RHICommandBuffer> VulkanDevice::CreateCommandBuffer()
    {
        return CreateRef<VulkanCommandBuffer>(this);
    }

    void VulkanDevice::SubmitCommandBuffer(RHICommandBuffer* cmd)
    {
        HVE_CORE_WARN_TAG("Vulkan", "SubmitCommandBuffer not yet implemented");
    }

}

#include "pch.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanShader.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/Vulkan/VulkanRenderPass.h"
#include "Renderer/Vulkan/VulkanFramebuffer.h"
#include "Renderer/Vulkan/VulkanPipeline.h"
#include "Renderer/Vulkan/VulkanTexture.h"
#include "Renderer/Vulkan/VulkanDescriptor.h"

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

        // --- Descriptor pool ---
        std::vector<VkDescriptorPoolSize> poolSizes = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         100 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           20 },
        };

        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.maxSets       = 200;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes    = poolSizes.data();
        descriptorPoolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        vkCreateDescriptorPool(m_Device, &descriptorPoolInfo, nullptr, &m_DescriptorPool);

        // --- Debug names ---
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_DEVICE, (uint64_t)m_Device, "MainDevice");
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_QUEUE, (uint64_t)m_GraphicsQueue, "GraphicsQueue");
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)m_ImmediateCommandPool, "ImmediateCommandPool");
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_FENCE, (uint64_t)m_ImmediateFence, "ImmediateFence");
        VulkanContext::SetDebugName(m_Device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)m_DescriptorPool, "MainDescriptorPool");

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

            if (m_DescriptorPool != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

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
        return CreateRef<VulkanTexture>(this, desc, initialData);
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
        return CreateRef<VulkanPipeline>(this, desc);
    }

    Ref<RHIPipeline> VulkanDevice::CreateComputePipeline(const ComputePipelineDesc& desc)
    {
        return CreateRef<VulkanPipeline>(this, desc);
    }

    Ref<RHIRenderPass> VulkanDevice::CreateRenderPass(const RenderPassDesc& desc)
    {
        return CreateRef<VulkanRenderPass>(this, desc);
    }

    Ref<RHIFramebuffer> VulkanDevice::CreateFramebuffer(const FramebufferDesc& desc)
    {
        return CreateRef<VulkanFramebuffer>(this, desc);
    }

    Ref<RHIDescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
    {
        return CreateRef<VulkanDescriptorSetLayout>(this, desc);
    }

    Ref<RHIDescriptorSet> VulkanDevice::AllocateDescriptorSet(RHIDescriptorSetLayout* layout)
    {
        auto* vkLayout = static_cast<VulkanDescriptorSetLayout*>(layout);
        VkDescriptorSetLayout dsl = vkLayout->GetVkLayout();

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &dsl;

        VkDescriptorSet set;
        VkResult result = vkAllocateDescriptorSets(m_Device, &allocInfo, &set);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to allocate descriptor set");
            return nullptr;
        }

        return CreateRef<VulkanDescriptorSet>(set, this);
    }

    void VulkanDevice::UpdateDescriptorSet(RHIDescriptorSet* set, const std::vector<DescriptorWrite>& writes)
    {
        auto* vkSet = static_cast<VulkanDescriptorSet*>(set);
        VkDescriptorSet ds = vkSet->GetVkSet();

        std::vector<VkWriteDescriptorSet> vkWrites;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkDescriptorImageInfo> imageInfos;
        vkWrites.reserve(writes.size());
        bufferInfos.reserve(writes.size());
        imageInfos.reserve(writes.size());

        for (const auto& write : writes) {
            VkWriteDescriptorSet vkWrite{};
            vkWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vkWrite.dstSet          = ds;
            vkWrite.dstBinding      = write.Binding;
            vkWrite.dstArrayElement = 0;
            vkWrite.descriptorCount = 1;

            switch (write.Type) {
                case DescriptorType::UniformBuffer:
                case DescriptorType::StorageBuffer: {
                    auto* vkBuffer = static_cast<VulkanBuffer*>(write.Buffer);
                    VkDescriptorBufferInfo bufInfo{};
                    bufInfo.buffer = vkBuffer->GetVkBuffer();
                    bufInfo.offset = write.Offset;
                    bufInfo.range  = write.Range > 0 ? write.Range : VK_WHOLE_SIZE;
                    bufferInfos.push_back(bufInfo);

                    vkWrite.descriptorType = (write.Type == DescriptorType::UniformBuffer)
                        ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                        : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    vkWrite.pBufferInfo = &bufferInfos.back();
                    break;
                }
                case DescriptorType::CombinedImageSampler: {
                    auto* vkTex = static_cast<VulkanTexture*>(write.Texture);
                    VkDescriptorImageInfo imgInfo{};
                    imgInfo.sampler     = vkTex->GetVkSampler();
                    imgInfo.imageView   = vkTex->GetVkImageView();
                    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfos.push_back(imgInfo);

                    vkWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    vkWrite.pImageInfo = &imageInfos.back();
                    break;
                }
                case DescriptorType::StorageImage: {
                    auto* vkTex = static_cast<VulkanTexture*>(write.Texture);
                    VkDescriptorImageInfo imgInfo{};
                    imgInfo.sampler     = VK_NULL_HANDLE;
                    imgInfo.imageView   = vkTex->GetVkImageView();
                    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    imageInfos.push_back(imgInfo);

                    vkWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    vkWrite.pImageInfo = &imageInfos.back();
                    break;
                }
            }

            vkWrites.push_back(vkWrite);
        }

        vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(vkWrites.size()),
                               vkWrites.data(), 0, nullptr);
    }

    Ref<RHICommandBuffer> VulkanDevice::CreateCommandBuffer()
    {
        return CreateRef<VulkanCommandBuffer>(this);
    }

    void VulkanDevice::SubmitCommandBuffer(RHICommandBuffer* cmd)
    {
        auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);
        VkCommandBuffer cmdBuffer = vkCmd->GetVkCommandBuffer();

        // TODO: Integrate proper synchronization with swapchain (fences/semaphores).
        // For now, submit with no sync and wait idle.
        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmdBuffer;

        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);
    }

}

#pragma once

#include "RHI/RHI.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Engine {
    class VulkanDevice : public RHIDevice {
    public:
        VulkanDevice(VkSurfaceKHR surface);
        ~VulkanDevice() override;

        // Vulkan handle accessors (used by other Vulkan backend classes)
        VkDevice GetDevice() const { return m_Device; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
        VmaAllocator GetAllocator() const { return m_Allocator; }
        VkDescriptorPool GetDescriptorPool() const { return m_DescriptorPool; }

        // RHIDevice interface -- stub all for now, implement in future tasks
        Ref<RHITexture> CreateTexture(const TextureDesc& desc, const void* initialData) override;
        Ref<RHIBuffer> CreateBuffer(const BufferDesc& desc, const void* initialData) override;
        Ref<RHIShader> CreateShader(const ShaderDesc& desc) override;
        Ref<RHIPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
        Ref<RHIPipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override;
        Ref<RHIRenderPass> CreateRenderPass(const RenderPassDesc& desc) override;
        Ref<RHIFramebuffer> CreateFramebuffer(const FramebufferDesc& desc) override;
        Ref<RHIDescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;
        Ref<RHIDescriptorSet> AllocateDescriptorSet(RHIDescriptorSetLayout* layout) override;
        void UpdateDescriptorSet(RHIDescriptorSet* set, const std::vector<DescriptorWrite>& writes) override;
        Ref<RHICommandBuffer> CreateCommandBuffer() override;
        void SubmitCommandBuffer(RHICommandBuffer* cmd) override;
        void WaitIdle() override;

        // Helper: submit a one-shot command buffer (for transfers, layout transitions)
        void ImmediateSubmit(std::function<void(VkCommandBuffer)>&& function);

    private:
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        uint32_t m_GraphicsQueueFamily = 0;
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        // Descriptor pool
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        // For immediate submit
        VkCommandPool m_ImmediateCommandPool = VK_NULL_HANDLE;
        VkCommandBuffer m_ImmediateCommandBuffer = VK_NULL_HANDLE;
        VkFence m_ImmediateFence = VK_NULL_HANDLE;
    };
}

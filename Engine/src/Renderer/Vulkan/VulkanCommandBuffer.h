#pragma once

#include "RHI/RHICommandBuffer.h"

#include <vulkan/vulkan.h>

namespace Engine {

    class VulkanDevice;

    class VulkanCommandBuffer : public RHICommandBuffer {
    public:
        VulkanCommandBuffer(VulkanDevice* device);
        ~VulkanCommandBuffer() override;

        // Recording
        void Begin() override;
        void End() override;

        // Render pass (Vulkan 1.3 dynamic rendering)
        void BeginRenderPass(RHIRenderPass* renderPass, RHIFramebuffer* framebuffer, const ClearValues& clear) override;
        void EndRenderPass() override;

        // Pipeline & state
        void BindPipeline(RHIPipeline* pipeline) override;
        void SetViewport(float x, float y, float width, float height) override;
        void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

        // Resources
        void BindVertexBuffer(RHIBuffer* buffer, uint32_t binding = 0) override;
        void BindIndexBuffer(RHIBuffer* buffer, IndexType type = IndexType::Uint32) override;
        void BindDescriptorSet(uint32_t set, RHIDescriptorSet* descriptorSet) override;
        void PushConstants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data) override;

        // Draw
        void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) override;

        // Compute
        void Dispatch(uint32_t x, uint32_t y, uint32_t z) override;

        // Synchronization
        void PipelineBarrier(const BarrierDesc& barrier) override;

        // Transfer
        void CopyBuffer(RHIBuffer* src, RHIBuffer* dst, uint32_t size) override;

        VkCommandBuffer GetVkCommandBuffer() const { return m_CommandBuffer; }

    private:
        VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        VulkanDevice* m_Device;

        // Track current state for pipeline layout (needed for push constants and descriptor binding)
        VkPipelineLayout m_CurrentPipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint m_CurrentBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    };

} // namespace Engine

#pragma once

#include "RHI/RHITypes.h"
#include "RHI/RHIResources.h"
#include "RHI/RHIDescriptor.h"

namespace Engine {

    class RHICommandBuffer {
    public:
        virtual ~RHICommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;

        // Render pass
        virtual void BeginRenderPass(RHIRenderPass* renderPass, RHIFramebuffer* framebuffer, const ClearValues& clear) = 0;
        virtual void EndRenderPass() = 0;

        // Pipeline & state
        virtual void BindPipeline(RHIPipeline* pipeline) = 0;
        virtual void SetViewport(float x, float y, float width, float height) = 0;
        virtual void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

        // Resources
        virtual void BindVertexBuffer(RHIBuffer* buffer, uint32_t binding = 0) = 0;
        virtual void BindIndexBuffer(RHIBuffer* buffer, IndexType type = IndexType::Uint32) = 0;
        virtual void BindDescriptorSet(uint32_t set, RHIDescriptorSet* descriptorSet) = 0;
        virtual void PushConstants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data) = 0;

        // Draw
        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) = 0;

        // Compute
        virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;

        // Synchronization
        virtual void PipelineBarrier(const BarrierDesc& barrier) = 0;

        // Transfer
        virtual void CopyBuffer(RHIBuffer* src, RHIBuffer* dst, uint32_t size) = 0;
    };

} // namespace Engine

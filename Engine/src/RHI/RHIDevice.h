#pragma once

#include "RHI/RHITypes.h"
#include "RHI/RHIResources.h"
#include "RHI/RHIDescriptor.h"
#include "RHI/RHICommandBuffer.h"
#include "Core/Base.h"

namespace Engine {

    class RHIDevice {
    public:
        virtual ~RHIDevice() = default;

        // Resource creation
        virtual Ref<RHITexture> CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) = 0;
        virtual Ref<RHIBuffer> CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
        virtual Ref<RHIShader> CreateShader(const ShaderDesc& desc) = 0;
        virtual Ref<RHIPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual Ref<RHIPipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
        virtual Ref<RHIRenderPass> CreateRenderPass(const RenderPassDesc& desc) = 0;
        virtual Ref<RHIFramebuffer> CreateFramebuffer(const FramebufferDesc& desc) = 0;

        // Descriptors
        virtual Ref<RHIDescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;
        virtual Ref<RHIDescriptorSet> AllocateDescriptorSet(RHIDescriptorSetLayout* layout) = 0;
        virtual void UpdateDescriptorSet(RHIDescriptorSet* set, const std::vector<DescriptorWrite>& writes) = 0;

        // Command buffers
        virtual Ref<RHICommandBuffer> CreateCommandBuffer() = 0;
        virtual void SubmitCommandBuffer(RHICommandBuffer* cmd) = 0;

        // Sync
        virtual void WaitIdle() = 0;
    };

} // namespace Engine

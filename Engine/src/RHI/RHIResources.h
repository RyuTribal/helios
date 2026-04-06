#pragma once

#include "RHI/RHITypes.h"
#include "Core/Base.h"

namespace Engine {

    // Forward declarations
    class RHIRenderPass;
    class RHITexture;

    class RHITexture {
    public:
        virtual ~RHITexture() = default;
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
    };

    class RHIBuffer {
    public:
        virtual ~RHIBuffer() = default;
        virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
        virtual void* Map() = 0;
        virtual void Unmap() = 0;
        virtual uint32_t GetSize() const = 0;
    };

    class RHIShader {
    public:
        virtual ~RHIShader() = default;
    };

    class RHIPipeline {
    public:
        virtual ~RHIPipeline() = default;
    };

    class RHIRenderPass {
    public:
        virtual ~RHIRenderPass() = default;
    };

    class RHIFramebuffer {
    public:
        virtual ~RHIFramebuffer() = default;
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
    };

    // Now define FramebufferDesc (needs RHITexture and RHIRenderPass)
    struct FramebufferDesc {
        RHIRenderPass* RenderPass = nullptr;
        std::vector<RHITexture*> Attachments;
        uint32_t Width = 0, Height = 0;
        std::string DebugName;
    };

    // Pipeline descs need RHIShader, RHIRenderPass, and descriptor layouts
    class RHIDescriptorSetLayout; // forward declare

    struct GraphicsPipelineDesc {
        RHIShader* VertexShader = nullptr;
        RHIShader* FragmentShader = nullptr;
        RHIShader* GeometryShader = nullptr;
        VertexLayout Layout;
        RenderState State;
        RHIRenderPass* RenderPass = nullptr;
        std::vector<RHIDescriptorSetLayout*> DescriptorLayouts;
        uint32_t PushConstantSize = 0;
        ShaderStage PushConstantStages = ShaderStage::Vertex;
        std::string DebugName;
    };

    struct ComputePipelineDesc {
        RHIShader* ComputeShader = nullptr;
        std::vector<RHIDescriptorSetLayout*> DescriptorLayouts;
        uint32_t PushConstantSize = 0;
        std::string DebugName;
    };

} // namespace Engine

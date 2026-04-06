#pragma once
#include "RHI/RHI.h"
#include "PipelineCache.h"

namespace Engine {

    class Camera;

    struct SkyboxRenderData {
        Ref<RHITexture> CubeTexture;
        float Brightness = 1.0f;
        Ref<RHITexture> IrradianceTexture;
        Ref<RHITexture> PrefilterMap;
        Ref<RHITexture> BRDFTexture;
    };

    class SkyboxRenderer {
    public:
        SkyboxRenderer() = default;

        void Init(RHIDevice* device, PipelineCache* cache, RHIRenderPass* renderPass, uint32_t width, uint32_t height);
        void Execute(RHICommandBuffer* cmd, RHIFramebuffer* framebuffer, Camera* camera, const SkyboxRenderData& settings);
        void Resize(uint32_t width, uint32_t height);

        // TODO: IBL generation (equirect-to-cube, irradiance convolution, prefilter)
        // will be implemented in a later pass

    private:
        RHIDevice* m_Device = nullptr;
        Ref<RHIPipeline> m_Pipeline;
        Ref<RHIDescriptorSetLayout> m_DescLayout;
        Ref<RHIDescriptorSet> m_DescSet;
        Ref<RHIBuffer> m_CameraUBO;
        Ref<RHIBuffer> m_CubeVBO;
        Ref<RHIBuffer> m_CubeIBO;
        uint32_t m_CubeIndexCount = 0;
    };

}

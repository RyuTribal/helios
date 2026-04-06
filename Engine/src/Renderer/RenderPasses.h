#pragma once
#include "RHI/RHI.h"
#include "PipelineCache.h"
#include "Mesh.h"
#include "Camera.h"
#include "Lights/PointLight.h"
#include "Lights/DirectionalLight.h"

namespace Engine {

    // ---- GPU data structures matching shader layouts ----

    struct GlobalUBOData {
        glm::mat4 CameraView;
        glm::mat4 CameraProjection;
        glm::vec3 CameraPos;
        float CameraFarPlane;
        int NumDirectionalLights;
        int NumberOfTilesX;
        float EnvironmentBrightness;
        float _padding;
    };

    struct PushConstantData {
        glm::mat4 Transform;
    };

    // ---- Vertex layout for Helios meshes ----

    inline VertexLayout GetMeshVertexLayout()
    {
        VertexLayout layout;
        layout.Stride = sizeof(Vertex);
        layout.Attributes = {
            { 0, 0, static_cast<uint32_t>(offsetof(Vertex, coordinates)),        ImageFormat::RGB32F  },
            { 1, 0, static_cast<uint32_t>(offsetof(Vertex, color)),              ImageFormat::RGBA32F },
            { 2, 0, static_cast<uint32_t>(offsetof(Vertex, texture_coordinates)),ImageFormat::RG32F   },
            { 3, 0, static_cast<uint32_t>(offsetof(Vertex, normal)),             ImageFormat::RGB32F  },
            { 4, 0, static_cast<uint32_t>(offsetof(Vertex, tangent)),            ImageFormat::RGB32F  },
            { 5, 0, static_cast<uint32_t>(offsetof(Vertex, bitangent)),          ImageFormat::RGB32F  },
        };
        return layout;
    }


    // ============================================================
    // DepthPrePass
    // ============================================================

    struct DepthPrePass {
        Ref<RHIRenderPass> RenderPass;
        Ref<RHIFramebuffer> Framebuffer;
        Ref<RHITexture> DepthTexture;
        Ref<RHIPipeline> Pipeline;
        Ref<RHIDescriptorSetLayout> DescLayout;
        Ref<RHIDescriptorSet> DescSet;
        Ref<RHIBuffer> CameraUBO;

        void Init(RHIDevice* device, PipelineCache* cache, uint32_t width, uint32_t height);
        void Execute(RHICommandBuffer* cmd, const std::vector<Ref<Mesh>>& meshes,
                     const glm::mat4& view, const glm::mat4& projection);
        void Resize(RHIDevice* device, uint32_t width, uint32_t height);

    private:
        uint32_t m_Width = 0, m_Height = 0;
    };


    // ============================================================
    // ShadowPass
    // ============================================================

    struct ShadowPass {
        Ref<RHIRenderPass> RenderPass;
        Ref<RHIFramebuffer> Framebuffer;
        Ref<RHITexture> ShadowMap;
        Ref<RHIPipeline> Pipeline;
        Ref<RHIDescriptorSetLayout> DescLayout;
        Ref<RHIDescriptorSet> DescSet;
        Ref<RHIBuffer> LightMatricesUBO;

        void Init(RHIDevice* device, PipelineCache* cache, uint32_t resolution, uint32_t cascadeCount);
        void Execute(RHICommandBuffer* cmd, const std::vector<Ref<Mesh>>& meshes,
                     const std::vector<glm::mat4>& lightMatrices);
        void Resize(RHIDevice* device, uint32_t resolution, uint32_t cascadeCount);

    private:
        uint32_t m_Resolution = 0;
        uint32_t m_CascadeCount = 0;
    };


    // ============================================================
    // LightCullingPass (compute)
    // ============================================================

    const int MAX_POINT_LIGHTS = 1000;
    const int MAX_DIR_LIGHTS = 2;

    struct PointLightInfo {
        float constantAttenuation;
        float linearAttenuation;
        float quadraticAttenuation;
        float intensity;
        glm::vec4 color;
        glm::vec4 position;
    };

    struct DirectionalLightInfo {
        glm::vec3 padding = { 1.f, 1.f, 1.f };
        float intensity;
        glm::vec4 color;
        glm::vec4 direction;
    };

    struct VisibleIndex {
        int index;
    };

    struct LightCullingPass {
        Ref<RHIPipeline> ComputePipeline;
        Ref<RHIDescriptorSetLayout> DescLayout;
        Ref<RHIDescriptorSet> DescSet;
        Ref<RHIBuffer> LightSSBO;
        Ref<RHIBuffer> DirLightSSBO;
        Ref<RHIBuffer> VisibleIndicesSSBO;
        Ref<RHIBuffer> ParamsUBO;

        void Init(RHIDevice* device, PipelineCache* cache);
        void Execute(RHICommandBuffer* cmd, RHITexture* depthTexture,
                     uint32_t lightCount, uint32_t width, uint32_t height);
        void UploadLights(RHIDevice* device, const std::vector<PointLight*>& lights,
                          const std::vector<DirectionalLight*>& dirLights,
                          const glm::mat4& view, const glm::mat4& projection,
                          uint32_t width, uint32_t height);
    };


    // ============================================================
    // ForwardPass
    // ============================================================

    struct ForwardPass {
        Ref<RHIRenderPass> RenderPass;
        Ref<RHIFramebuffer> Framebuffer;
        Ref<RHITexture> ColorTexture;
        Ref<RHITexture> DepthTexture;
        Ref<RHIDescriptorSetLayout> GlobalDescLayout;
        Ref<RHIDescriptorSetLayout> MaterialDescLayout;
        Ref<RHIDescriptorSet> GlobalDescSet;
        Ref<RHIBuffer> GlobalUBO;
        Ref<RHIPipeline> Pipeline;

        void Init(RHIDevice* device, PipelineCache* cache, uint32_t width, uint32_t height);
        void Execute(RHICommandBuffer* cmd, const std::vector<Ref<Mesh>>& meshes,
                     Camera* camera, float environmentBrightness,
                     int numDirLights, int numTilesX,
                     LightCullingPass* lightCulling,
                     RHITexture* shadowMap, RHIBuffer* lightMatricesUBO);
        void Resize(RHIDevice* device, uint32_t width, uint32_t height);

        // Ensure materials have descriptor sets allocated
        void PrepareMaterial(RHIDevice* device, Material* material);

    private:
        uint32_t m_Width = 0, m_Height = 0;
    };


    // ============================================================
    // TonemapPass
    // ============================================================

    struct TonemapPass {
        Ref<RHIRenderPass> RenderPass;
        Ref<RHIPipeline> Pipeline;
        Ref<RHIDescriptorSetLayout> DescLayout;
        Ref<RHIDescriptorSet> DescSet;
        Ref<RHIBuffer> ParamsUBO;

        void Init(RHIDevice* device, PipelineCache* cache, RHIRenderPass* swapchainRenderPass);
        void Execute(RHICommandBuffer* cmd, RHIFramebuffer* target, RHITexture* hdrTexture, float exposure);
    };

}

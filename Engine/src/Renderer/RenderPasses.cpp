#include "pch.h"
#include "RenderPasses.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/Vulkan/VulkanTexture.h"

namespace Engine {

    // ============================================================
    // DepthPrePass
    // ============================================================

    void DepthPrePass::Init(RHIDevice* device, PipelineCache* cache, uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        // Create depth texture
        TextureDesc depthDesc;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = ImageFormat::DEPTH32F;
        depthDesc.Usage = TextureUsage::DepthAttachment | TextureUsage::Sampled;
        depthDesc.DebugName = "DepthPrePassTexture";
        DepthTexture = device->CreateTexture(depthDesc);

        // Render pass (depth-only)
        RenderPassDesc rpDesc;
        rpDesc.HasDepth = true;
        rpDesc.DepthAttachment.Format = ImageFormat::DEPTH32F;
        rpDesc.DepthAttachment.Load = LoadOp::Clear;
        rpDesc.DepthAttachment.Store = StoreOp::Store;
        rpDesc.DebugName = "DepthPrePass";
        RenderPass = device->CreateRenderPass(rpDesc);

        // Framebuffer
        FramebufferDesc fbDesc;
        fbDesc.RenderPass = RenderPass.get();
        fbDesc.Attachments = { DepthTexture.get() };
        fbDesc.Width = width;
        fbDesc.Height = height;
        fbDesc.DebugName = "DepthPrePassFB";
        Framebuffer = device->CreateFramebuffer(fbDesc);

        // Camera UBO (view + projection)
        BufferDesc uboDesc;
        uboDesc.Size = sizeof(glm::mat4) * 2;
        uboDesc.Usage = BufferUsage::Uniform;
        uboDesc.Access = MemoryAccess::CPU_to_GPU;
        uboDesc.DebugName = "DepthPrePassCameraUBO";
        CameraUBO = device->CreateBuffer(uboDesc);

        // Descriptor layout
        DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex, 1 }
        };
        layoutDesc.DebugName = "DepthPrePassDescLayout";
        DescLayout = device->CreateDescriptorSetLayout(layoutDesc);
        DescSet = device->AllocateDescriptorSet(DescLayout.get());

        DescriptorWrite write;
        write.Binding = 0;
        write.Type = DescriptorType::UniformBuffer;
        write.Buffer = CameraUBO.get();
        write.Range = sizeof(glm::mat4) * 2;
        device->UpdateDescriptorSet(DescSet.get(), { write });

        // Load shaders
        auto vertShader = cache->LoadShader("Resources/Shaders/depth_pre_pass.vert.spv", ShaderStage::Vertex);
        auto fragShader = cache->LoadShader("Resources/Shaders/depth_pre_pass.frag.spv", ShaderStage::Fragment);

        if (vertShader && fragShader)
        {
            GraphicsPipelineDesc pipeDesc;
            pipeDesc.VertexShader = vertShader.get();
            pipeDesc.FragmentShader = fragShader.get();
            pipeDesc.Layout = GetMeshVertexLayout();
            pipeDesc.RenderPass = RenderPass.get();
            pipeDesc.DescriptorLayouts = { DescLayout.get() };
            pipeDesc.PushConstantSize = sizeof(PushConstantData);
            pipeDesc.PushConstantStages = ShaderStage::Vertex;
            pipeDesc.State.DepthWrite = true;
            pipeDesc.State.DepthTest = true;
            pipeDesc.State.Cull = CullMode::Back;
            pipeDesc.DebugName = "DepthPrePassPipeline";

            Pipeline = cache->GetOrCreateGraphicsPipeline("depth_prepass", pipeDesc);
        }
    }

    void DepthPrePass::Execute(RHICommandBuffer* cmd, const std::vector<Ref<Mesh>>& meshes,
                                const glm::mat4& view, const glm::mat4& projection)
    {
        if (!Pipeline) return;

        // Transition depth texture to DEPTH_STENCIL_ATTACHMENT_OPTIMAL before render pass
        auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);
        auto* vkDepthTex = static_cast<VulkanTexture*>(DepthTexture.get());
        VulkanTexture::TransitionLayout(vkCmd->GetVkCommandBuffer(),
            vkDepthTex->GetVkImage(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        // Upload camera matrices
        struct { glm::mat4 view, proj; } camData = { view, projection };
        CameraUBO->SetData(&camData, sizeof(camData));

        ClearValues clear;
        clear.Depth = 1.0f;
        cmd->BeginRenderPass(RenderPass.get(), Framebuffer.get(), clear);
        cmd->BindPipeline(Pipeline.get());
        cmd->SetViewport(0, 0, static_cast<float>(m_Width), static_cast<float>(m_Height));
        cmd->SetScissor(0, 0, m_Width, m_Height);
        cmd->BindDescriptorSet(0, DescSet.get());

        for (auto& mesh : meshes)
        {
            if (!mesh->GetMeshSource()) continue;
            auto& submeshes = mesh->GetMeshSource()->GetSubmeshes();
            for (auto& submesh : submeshes)
            {
                if (!submesh.VBO || !submesh.IBO) continue;

                PushConstantData pc;
                pc.Transform = mesh->GetTransform() * submesh.WorldTransform;
                cmd->PushConstants(ShaderStage::Vertex, 0, sizeof(PushConstantData), &pc);
                cmd->BindVertexBuffer(submesh.VBO->GetRHIBuffer());
                cmd->BindIndexBuffer(submesh.IBO->GetRHIBuffer());
                cmd->DrawIndexed(submesh.IndexCount);
            }
        }

        cmd->EndRenderPass();

        // Transition depth to SHADER_READ_ONLY_OPTIMAL so light culling can sample it
        VulkanTexture::TransitionLayout(vkCmd->GetVkCommandBuffer(),
            vkDepthTex->GetVkImage(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void DepthPrePass::Resize(RHIDevice* device, uint32_t width, uint32_t height)
    {
        if (width == m_Width && height == m_Height) return;
        m_Width = width;
        m_Height = height;

        TextureDesc depthDesc;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = ImageFormat::DEPTH32F;
        depthDesc.Usage = TextureUsage::DepthAttachment | TextureUsage::Sampled;
        depthDesc.DebugName = "DepthPrePassTexture";
        DepthTexture = device->CreateTexture(depthDesc);

        FramebufferDesc fbDesc;
        fbDesc.RenderPass = RenderPass.get();
        fbDesc.Attachments = { DepthTexture.get() };
        fbDesc.Width = width;
        fbDesc.Height = height;
        fbDesc.DebugName = "DepthPrePassFB";
        Framebuffer = device->CreateFramebuffer(fbDesc);
    }


    // ============================================================
    // ShadowPass
    // ============================================================

    void ShadowPass::Init(RHIDevice* device, PipelineCache* cache, uint32_t resolution, uint32_t cascadeCount)
    {
        m_Resolution = resolution;
        m_CascadeCount = cascadeCount;

        // Shadow map (depth-only array texture for CSM)
        TextureDesc smDesc;
        smDesc.Width = resolution;
        smDesc.Height = resolution;
        smDesc.Format = ImageFormat::DEPTH32F;
        smDesc.Type = TextureType::Texture2DArray;
        smDesc.ArrayLayers = cascadeCount;
        smDesc.Usage = TextureUsage::DepthAttachment | TextureUsage::Sampled;
        smDesc.DebugName = "ShadowMap";
        ShadowMap = device->CreateTexture(smDesc);

        // Render pass
        RenderPassDesc rpDesc;
        rpDesc.HasDepth = true;
        rpDesc.DepthAttachment.Format = ImageFormat::DEPTH32F;
        rpDesc.DepthAttachment.Load = LoadOp::Clear;
        rpDesc.DepthAttachment.Store = StoreOp::Store;
        rpDesc.DebugName = "ShadowPass";
        RenderPass = device->CreateRenderPass(rpDesc);

        // Framebuffer
        FramebufferDesc fbDesc;
        fbDesc.RenderPass = RenderPass.get();
        fbDesc.Attachments = { ShadowMap.get() };
        fbDesc.Width = resolution;
        fbDesc.Height = resolution;
        fbDesc.DebugName = "ShadowPassFB";
        Framebuffer = device->CreateFramebuffer(fbDesc);

        // Light matrices UBO
        BufferDesc uboDesc;
        uboDesc.Size = sizeof(glm::mat4) * 16; // up to 16 cascade matrices
        uboDesc.Usage = BufferUsage::Uniform;
        uboDesc.Access = MemoryAccess::CPU_to_GPU;
        uboDesc.DebugName = "LightMatricesUBO";
        LightMatricesUBO = device->CreateBuffer(uboDesc);

        // Descriptor layout
        DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Geometry, 1 }
        };
        layoutDesc.DebugName = "ShadowPassDescLayout";
        DescLayout = device->CreateDescriptorSetLayout(layoutDesc);
        DescSet = device->AllocateDescriptorSet(DescLayout.get());

        DescriptorWrite write;
        write.Binding = 0;
        write.Type = DescriptorType::UniformBuffer;
        write.Buffer = LightMatricesUBO.get();
        write.Range = sizeof(glm::mat4) * 16;
        device->UpdateDescriptorSet(DescSet.get(), { write });

        // Load shaders
        auto vertShader = cache->LoadShader("Resources/Shaders/dir_light_shadows.vert.spv", ShaderStage::Vertex);
        auto fragShader = cache->LoadShader("Resources/Shaders/dir_light_shadows.frag.spv", ShaderStage::Fragment);
        auto geomShader = cache->LoadShader("Resources/Shaders/dir_light_shadows.geo.spv", ShaderStage::Geometry);

        if (vertShader && fragShader)
        {
            GraphicsPipelineDesc pipeDesc;
            pipeDesc.VertexShader = vertShader.get();
            pipeDesc.FragmentShader = fragShader.get();
            pipeDesc.GeometryShader = geomShader ? geomShader.get() : nullptr;
            pipeDesc.Layout = GetMeshVertexLayout();
            pipeDesc.RenderPass = RenderPass.get();
            pipeDesc.DescriptorLayouts = { DescLayout.get() };
            pipeDesc.PushConstantSize = sizeof(PushConstantData);
            pipeDesc.PushConstantStages = ShaderStage::Vertex;
            pipeDesc.State.DepthWrite = true;
            pipeDesc.State.DepthTest = true;
            pipeDesc.State.Cull = CullMode::Front; // Front-face culling for shadow bias
            pipeDesc.DebugName = "ShadowPassPipeline";

            Pipeline = cache->GetOrCreateGraphicsPipeline("shadow_pass", pipeDesc);
        }
    }

    void ShadowPass::Execute(RHICommandBuffer* cmd, const std::vector<Ref<Mesh>>& meshes,
                              const std::vector<glm::mat4>& lightMatrices)
    {
        if (!Pipeline || lightMatrices.empty()) return;

        // Transition shadow map to DEPTH_STENCIL_ATTACHMENT_OPTIMAL before render pass
        auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);
        auto* vkShadowTex = static_cast<VulkanTexture*>(ShadowMap.get());
        VulkanTexture::TransitionLayout(vkCmd->GetVkCommandBuffer(),
            vkShadowTex->GetVkImage(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        // Upload light matrices
        LightMatricesUBO->SetData(lightMatrices.data(),
            static_cast<uint32_t>(lightMatrices.size() * sizeof(glm::mat4)));

        ClearValues clear;
        clear.Depth = 1.0f;
        cmd->BeginRenderPass(RenderPass.get(), Framebuffer.get(), clear);
        cmd->BindPipeline(Pipeline.get());
        cmd->SetViewport(0, 0, static_cast<float>(m_Resolution), static_cast<float>(m_Resolution));
        cmd->SetScissor(0, 0, m_Resolution, m_Resolution);
        cmd->BindDescriptorSet(0, DescSet.get());

        for (auto& mesh : meshes)
        {
            if (!mesh->GetMeshSource()) continue;
            auto& submeshes = mesh->GetMeshSource()->GetSubmeshes();
            for (auto& submesh : submeshes)
            {
                if (!submesh.VBO || !submesh.IBO) continue;

                PushConstantData pc;
                pc.Transform = mesh->GetTransform() * submesh.WorldTransform;
                cmd->PushConstants(ShaderStage::Vertex, 0, sizeof(PushConstantData), &pc);
                cmd->BindVertexBuffer(submesh.VBO->GetRHIBuffer());
                cmd->BindIndexBuffer(submesh.IBO->GetRHIBuffer());
                cmd->DrawIndexed(submesh.IndexCount);
            }
        }

        cmd->EndRenderPass();

        // Transition shadow map to SHADER_READ_ONLY_OPTIMAL for forward pass sampling
        VulkanTexture::TransitionLayout(vkCmd->GetVkCommandBuffer(),
            vkShadowTex->GetVkImage(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void ShadowPass::Resize(RHIDevice* device, uint32_t resolution, uint32_t cascadeCount)
    {
        if (resolution == m_Resolution && cascadeCount == m_CascadeCount) return;
        // Re-initialize with new resolution
        // Store existing pipeline cache pointer - we'd need to re-init fully
        // For now, just recreate textures and framebuffer
        m_Resolution = resolution;
        m_CascadeCount = cascadeCount;

        TextureDesc smDesc;
        smDesc.Width = resolution;
        smDesc.Height = resolution;
        smDesc.Format = ImageFormat::DEPTH32F;
        smDesc.Type = TextureType::Texture2DArray;
        smDesc.ArrayLayers = cascadeCount;
        smDesc.Usage = TextureUsage::DepthAttachment | TextureUsage::Sampled;
        smDesc.DebugName = "ShadowMap";
        ShadowMap = device->CreateTexture(smDesc);

        FramebufferDesc fbDesc;
        fbDesc.RenderPass = RenderPass.get();
        fbDesc.Attachments = { ShadowMap.get() };
        fbDesc.Width = resolution;
        fbDesc.Height = resolution;
        fbDesc.DebugName = "ShadowPassFB";
        Framebuffer = device->CreateFramebuffer(fbDesc);
    }


    // ============================================================
    // LightCullingPass
    // ============================================================

    void LightCullingPass::Init(RHIDevice* device, PipelineCache* cache)
    {
        // Light SSBO
        BufferDesc lightDesc;
        lightDesc.Size = sizeof(PointLightInfo) * MAX_POINT_LIGHTS;
        lightDesc.Usage = BufferUsage::Storage | BufferUsage::Transfer;
        lightDesc.Access = MemoryAccess::CPU_to_GPU;
        lightDesc.DebugName = "LightSSBO";
        LightSSBO = device->CreateBuffer(lightDesc);

        // Dir Light SSBO
        BufferDesc dirLightDesc;
        dirLightDesc.Size = sizeof(DirectionalLightInfo) * MAX_DIR_LIGHTS;
        dirLightDesc.Usage = BufferUsage::Storage | BufferUsage::Transfer;
        dirLightDesc.Access = MemoryAccess::CPU_to_GPU;
        dirLightDesc.DebugName = "DirLightSSBO";
        DirLightSSBO = device->CreateBuffer(dirLightDesc);

        // Visible indices SSBO — initialized to -1 so the shader's point light
        // loop terminates immediately when light culling hasn't run.
        uint32_t tilesX = (1920 + 15) / 16;
        uint32_t tilesY = (1080 + 15) / 16;
        uint32_t numTiles = tilesX * tilesY;
        uint32_t visSize = static_cast<uint32_t>(numTiles * sizeof(VisibleIndex) * 1024);
        BufferDesc visDesc;
        visDesc.Size = visSize;
        visDesc.Usage = BufferUsage::Storage | BufferUsage::Transfer;
        visDesc.Access = MemoryAccess::CPU_to_GPU;
        visDesc.DebugName = "VisibleIndicesSSBO";
        VisibleIndicesSSBO = device->CreateBuffer(visDesc);
        // Fill with -1 (sentinel value that stops the per-tile light loop)
        std::vector<int> sentinel(visSize / sizeof(int), -1);
        VisibleIndicesSSBO->SetData(sentinel.data(), visSize);

        // Params UBO (light count, screen size, view, projection)
        BufferDesc paramsDesc;
        paramsDesc.Size = 256; // enough for our params
        paramsDesc.Usage = BufferUsage::Uniform;
        paramsDesc.Access = MemoryAccess::CPU_to_GPU;
        paramsDesc.DebugName = "LightCullingParamsUBO";
        ParamsUBO = device->CreateBuffer(paramsDesc);

        // Descriptor layout
        DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer, ShaderStage::Compute, 1 },      // params
            { 1, DescriptorType::StorageBuffer, ShaderStage::Compute, 1 },      // lights
            { 2, DescriptorType::StorageBuffer, ShaderStage::Compute, 1 },      // visible indices
            { 3, DescriptorType::CombinedImageSampler, ShaderStage::Compute, 1 }, // depth texture
        };
        layoutDesc.DebugName = "LightCullingDescLayout";
        DescLayout = device->CreateDescriptorSetLayout(layoutDesc);
        DescSet = device->AllocateDescriptorSet(DescLayout.get());

        // Load compute shader
        auto compShader = cache->LoadShader("Resources/Shaders/light_culling_shader.comp.spv", ShaderStage::Compute);
        if (compShader)
        {
            ComputePipelineDesc pipeDesc;
            pipeDesc.ComputeShader = compShader.get();
            pipeDesc.DescriptorLayouts = { DescLayout.get() };
            pipeDesc.DebugName = "LightCullingPipeline";
            ComputePipeline = cache->GetOrCreateComputePipeline("light_culling", pipeDesc);
        }
    }

    void LightCullingPass::UploadLights(RHIDevice* device, const std::vector<PointLight*>& lights,
                                         const std::vector<DirectionalLight*>& dirLights,
                                         const glm::mat4& view, const glm::mat4& projection,
                                         uint32_t width, uint32_t height)
    {
        // Point lights
        std::vector<PointLightInfo> pointData(lights.size());
        for (size_t i = 0; i < lights.size(); ++i)
        {
            pointData[i].color = glm::vec4(lights[i]->GetColor(), 1.f);
            pointData[i].intensity = lights[i]->GetIntensity();
            pointData[i].position = glm::vec4(lights[i]->GetPosition(), 1.f);
            pointData[i].constantAttenuation = lights[i]->GetConstantAttenuation();
            pointData[i].linearAttenuation = lights[i]->GetLinearAttenuation();
            pointData[i].quadraticAttenuation = lights[i]->GetQuadraticAttenuation();
        }
        if (!pointData.empty())
            LightSSBO->SetData(pointData.data(), static_cast<uint32_t>(pointData.size() * sizeof(PointLightInfo)));

        // Dir lights
        std::vector<DirectionalLightInfo> dirData(dirLights.size());
        for (size_t i = 0; i < dirLights.size(); ++i)
        {
            dirData[i].color = glm::vec4(dirLights[i]->GetColor(), 1.f);
            dirData[i].direction = glm::vec4(dirLights[i]->GetDirection(), 1.f);
            dirData[i].intensity = dirLights[i]->GetIntensity();
        }
        if (!dirData.empty())
            DirLightSSBO->SetData(dirData.data(), static_cast<uint32_t>(dirData.size() * sizeof(DirectionalLightInfo)));

        // Params UBO
        struct LightCullingParams {
            glm::mat4 view;
            glm::mat4 projection;
            glm::ivec2 screenSize;
            int lightCount;
            int _pad;
        };
        LightCullingParams params;
        params.view = view;
        params.projection = projection;
        params.screenSize = glm::ivec2(width, height);
        params.lightCount = static_cast<int>(lights.size());
        ParamsUBO->SetData(&params, sizeof(LightCullingParams));
    }

    void LightCullingPass::Execute(RHICommandBuffer* cmd, RHITexture* /*depthTexture*/,
                                    uint32_t /*lightCount*/, uint32_t width, uint32_t height)
    {
        if (!ComputePipeline) return;

        // Descriptor set is updated by the Renderer before calling Execute
        // (all 4 bindings including depth texture are written from Renderer::BeginDrawing)
        cmd->BindPipeline(ComputePipeline.get());
        cmd->BindDescriptorSet(0, DescSet.get());

        uint32_t workGroupsX = (width + 15) / 16;
        uint32_t workGroupsY = (height + 15) / 16;
        cmd->Dispatch(workGroupsX, workGroupsY, 1);
    }


    // ============================================================
    // ForwardPass
    // ============================================================

    void ForwardPass::Init(RHIDevice* device, PipelineCache* cache, uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        // HDR color texture
        TextureDesc colorDesc;
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.Format = ImageFormat::RGBA16F;
        colorDesc.Usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
        colorDesc.DebugName = "ForwardPassColor";
        ColorTexture = device->CreateTexture(colorDesc);

        // Depth texture
        TextureDesc depthDesc;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = ImageFormat::DEPTH32F;
        depthDesc.Usage = TextureUsage::DepthAttachment | TextureUsage::Sampled;
        depthDesc.DebugName = "ForwardPassDepth";
        DepthTexture = device->CreateTexture(depthDesc);

        // Render pass
        RenderPassDesc rpDesc;
        rpDesc.ColorAttachments = {
            { ImageFormat::RGBA16F, 1, LoadOp::Clear, StoreOp::Store }
        };
        rpDesc.HasDepth = true;
        rpDesc.DepthAttachment.Format = ImageFormat::DEPTH32F;
        rpDesc.DepthAttachment.Load = LoadOp::Clear;
        rpDesc.DepthAttachment.Store = StoreOp::Store;
        rpDesc.DebugName = "ForwardPass";
        RenderPass = device->CreateRenderPass(rpDesc);

        // Framebuffer
        FramebufferDesc fbDesc;
        fbDesc.RenderPass = RenderPass.get();
        fbDesc.Attachments = { ColorTexture.get(), DepthTexture.get() };
        fbDesc.Width = width;
        fbDesc.Height = height;
        fbDesc.DebugName = "ForwardPassFB";
        Framebuffer = device->CreateFramebuffer(fbDesc);

        // Global UBO
        BufferDesc globalUBODesc;
        globalUBODesc.Size = sizeof(GlobalUBOData);
        globalUBODesc.Usage = BufferUsage::Uniform;
        globalUBODesc.Access = MemoryAccess::CPU_to_GPU;
        globalUBODesc.DebugName = "GlobalUBO";
        GlobalUBO = device->CreateBuffer(globalUBODesc);

        // Global descriptor set layout (set 0)
        // Binding 0: GlobalUBO
        // Binding 1: LightSSBO
        // Binding 2: DirLightSSBO
        // Binding 3: VisibleIndicesSSBO
        // Binding 4: LightMatricesUBO
        DescriptorSetLayoutDesc globalLayoutDesc;
        globalLayoutDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer,  ShaderStage::Vertex | ShaderStage::Fragment, 1 },
            { 1, DescriptorType::StorageBuffer,  ShaderStage::Fragment, 1 },
            { 2, DescriptorType::StorageBuffer,  ShaderStage::Fragment, 1 },
            { 3, DescriptorType::StorageBuffer,  ShaderStage::Fragment, 1 },
            { 4, DescriptorType::UniformBuffer,  ShaderStage::Fragment, 1 },
        };
        globalLayoutDesc.DebugName = "ForwardGlobalDescLayout";
        GlobalDescLayout = device->CreateDescriptorSetLayout(globalLayoutDesc);
        GlobalDescSet = device->AllocateDescriptorSet(GlobalDescLayout.get());

        // Material descriptor set layout (set 1)
        // Binding 0: MaterialUBO
        // Bindings 1-11: textures
        DescriptorSetLayoutDesc matLayoutDesc;
        matLayoutDesc.Bindings = {
            { 0,  DescriptorType::UniformBuffer,        ShaderStage::Fragment, 1 },
            { 1,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Normal
            { 2,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Roughness
            { 3,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Metalness
            { 4,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // reserved
            { 5,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Albedo
            { 6,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // AO
            { 7,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Emissive
            { 8,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Specular
            { 9,  DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // reserved
            { 10, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Irradiance
            { 11, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // Prefilter
            { 12, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // BrdfLUT
            { 13, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }, // ShadowMap
        };
        matLayoutDesc.DebugName = "ForwardMaterialDescLayout";
        MaterialDescLayout = device->CreateDescriptorSetLayout(matLayoutDesc);

        // Load shaders
        auto vertShader = cache->LoadShader("Resources/Shaders/default_static_shader.vert.spv", ShaderStage::Vertex);
        auto fragShader = cache->LoadShader("Resources/Shaders/default_static_shader.frag.spv", ShaderStage::Fragment);

        if (vertShader && fragShader)
        {
            GraphicsPipelineDesc pipeDesc;
            pipeDesc.VertexShader = vertShader.get();
            pipeDesc.FragmentShader = fragShader.get();
            pipeDesc.Layout = GetMeshVertexLayout();
            pipeDesc.RenderPass = RenderPass.get();
            pipeDesc.DescriptorLayouts = { GlobalDescLayout.get(), MaterialDescLayout.get() };
            pipeDesc.PushConstantSize = sizeof(PushConstantData);
            pipeDesc.PushConstantStages = ShaderStage::Vertex;
            pipeDesc.State.DepthWrite = true;
            pipeDesc.State.DepthTest = true;
            pipeDesc.State.Cull = CullMode::Back;
            pipeDesc.DebugName = "ForwardPassPipeline";

            Pipeline = cache->GetOrCreateGraphicsPipeline("forward_pass", pipeDesc);
        }
    }

    void ForwardPass::PrepareMaterial(RHIDevice* device, Material* material,
                                       RHITexture* default2D, RHITexture* defaultCube,
                                       RHITexture* defaultArray,
                                       RHITexture* irradianceTex, RHITexture* prefilterTex,
                                       RHITexture* brdfTex)
    {
        if (!material) return;

        if (!material->GetDescriptorSet())
        {
            auto descSet = device->AllocateDescriptorSet(MaterialDescLayout.get());
            material->SetDescriptorSet(descSet);

            BufferDesc uboDesc;
            uboDesc.Size = sizeof(MaterialData);
            uboDesc.Usage = BufferUsage::Uniform;
            uboDesc.Access = MemoryAccess::CPU_to_GPU;
            uboDesc.DebugName = "MaterialUBO";
            auto ubo = device->CreateBuffer(uboDesc);
            material->SetMaterialUBO(ubo);
        }

        if (material->IsDirty())
        {
            material->UpdateGPUData(device, default2D, defaultCube, defaultArray,
                                    irradianceTex, prefilterTex, brdfTex);
        }
    }

    void ForwardPass::Execute(RHICommandBuffer* cmd, const std::vector<Ref<Mesh>>& meshes,
                               Camera* camera, float environmentBrightness,
                               int numDirLights, int numTilesX,
                               LightCullingPass* lightCulling,
                               RHITexture* shadowMap, RHIBuffer* lightMatricesUBO)
    {
        if (!Pipeline || !camera) return;

        // Upload global UBO
        GlobalUBOData globalData;
        globalData.CameraView = camera->GetView();
        globalData.CameraProjection = camera->GetProjection();
        globalData.CameraPos = camera->CalculatePosition();
        globalData.CameraFarPlane = camera->GetFar();
        globalData.NumDirectionalLights = numDirLights;
        globalData.NumberOfTilesX = numTilesX;
        globalData.EnvironmentBrightness = environmentBrightness;
        globalData._padding = 0.0f;
        GlobalUBO->SetData(&globalData, sizeof(GlobalUBOData));

        // Note: render pass is already begun by the Renderer (to allow skybox/debug
        // rendering within the same render pass). Descriptor set is updated by the
        // Renderer before calling Execute.

        cmd->BindPipeline(Pipeline.get());
        cmd->SetViewport(0, 0, static_cast<float>(m_Width), static_cast<float>(m_Height));
        cmd->SetScissor(0, 0, m_Width, m_Height);
        cmd->BindDescriptorSet(0, GlobalDescSet.get());

        for (auto& mesh : meshes)
        {
            if (!mesh->GetMeshSource()) continue;
            auto& submeshes = mesh->GetMeshSource()->GetSubmeshes();
            auto& materials = mesh->GetMeshSource()->GetMaterials();

            for (auto& submesh : submeshes)
            {
                if (!submesh.VBO || !submesh.IBO) continue;

                // Bind material descriptor set (set 1)
                if (submesh.MaterialIndex < materials.size())
                {
                    Material* mat = materials[submesh.MaterialIndex].get();
                    if (mat && mat->GetDescriptorSet())
                    {
                        cmd->BindDescriptorSet(1, mat->GetDescriptorSet());
                    }
                }

                PushConstantData pc;
                pc.Transform = mesh->GetTransform() * submesh.WorldTransform;
                cmd->PushConstants(ShaderStage::Vertex, 0, sizeof(PushConstantData), &pc);
                cmd->BindVertexBuffer(submesh.VBO->GetRHIBuffer());
                cmd->BindIndexBuffer(submesh.IBO->GetRHIBuffer());
                cmd->DrawIndexed(submesh.IndexCount);
            }
        }

        // Note: render pass is NOT ended here — Renderer ends it after skybox/debug
    }

    void ForwardPass::Resize(RHIDevice* device, uint32_t width, uint32_t height)
    {
        if (width == m_Width && height == m_Height) return;
        m_Width = width;
        m_Height = height;

        TextureDesc colorDesc;
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.Format = ImageFormat::RGBA16F;
        colorDesc.Usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
        colorDesc.DebugName = "ForwardPassColor";
        ColorTexture = device->CreateTexture(colorDesc);

        TextureDesc depthDesc;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = ImageFormat::DEPTH32F;
        depthDesc.Usage = TextureUsage::DepthAttachment | TextureUsage::Sampled;
        depthDesc.DebugName = "ForwardPassDepth";
        DepthTexture = device->CreateTexture(depthDesc);

        FramebufferDesc fbDesc;
        fbDesc.RenderPass = RenderPass.get();
        fbDesc.Attachments = { ColorTexture.get(), DepthTexture.get() };
        fbDesc.Width = width;
        fbDesc.Height = height;
        fbDesc.DebugName = "ForwardPassFB";
        Framebuffer = device->CreateFramebuffer(fbDesc);
    }


    // ============================================================
    // TonemapPass
    // ============================================================

    void TonemapPass::Init(RHIDevice* device, PipelineCache* cache, RHIRenderPass* swapchainRenderPass)
    {
        // Params UBO
        BufferDesc uboDesc;
        uboDesc.Size = 16; // exposure + padding
        uboDesc.Usage = BufferUsage::Uniform;
        uboDesc.Access = MemoryAccess::CPU_to_GPU;
        uboDesc.DebugName = "TonemapParamsUBO";
        ParamsUBO = device->CreateBuffer(uboDesc);

        // Descriptor layout
        DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer,        ShaderStage::Fragment, 1 },
            { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 },
        };
        layoutDesc.DebugName = "TonemapDescLayout";
        DescLayout = device->CreateDescriptorSetLayout(layoutDesc);
        DescSet = device->AllocateDescriptorSet(DescLayout.get());

        DescriptorWrite uboWrite;
        uboWrite.Binding = 0;
        uboWrite.Type = DescriptorType::UniformBuffer;
        uboWrite.Buffer = ParamsUBO.get();
        uboWrite.Range = 16;
        device->UpdateDescriptorSet(DescSet.get(), { uboWrite });

        // Use the provided render pass (swapchain's render pass)
        RenderPass.reset();

        // Load shaders
        auto vertShader = cache->LoadShader("Resources/Shaders/hdr_shader.vert.spv", ShaderStage::Vertex);
        auto fragShader = cache->LoadShader("Resources/Shaders/hdr_shader.frag.spv", ShaderStage::Fragment);

        if (vertShader && fragShader && swapchainRenderPass)
        {
            // Fullscreen triangle - no vertex input needed (positions generated from gl_VertexIndex)
            VertexLayout emptyLayout;
            emptyLayout.Stride = 0;

            GraphicsPipelineDesc pipeDesc;
            pipeDesc.VertexShader = vertShader.get();
            pipeDesc.FragmentShader = fragShader.get();
            pipeDesc.Layout = emptyLayout;
            pipeDesc.RenderPass = swapchainRenderPass;
            pipeDesc.DescriptorLayouts = { DescLayout.get() };
            pipeDesc.State.DepthWrite = false;
            pipeDesc.State.DepthTest = false;
            pipeDesc.State.Cull = CullMode::None;
            pipeDesc.DebugName = "TonemapPipeline";

            Pipeline = cache->GetOrCreateGraphicsPipeline("tonemap", pipeDesc);
        }
    }

    void TonemapPass::Execute(RHICommandBuffer* cmd, RHIFramebuffer* target, RHITexture* hdrTexture, float exposure)
    {
        if (!Pipeline || !target) return;

        // Upload exposure
        struct { float exposure; float _pad[3]; } params = { exposure, {0, 0, 0} };
        ParamsUBO->SetData(&params, sizeof(params));

        // Update HDR texture binding
        if (hdrTexture)
        {
            DescriptorWrite texWrite;
            texWrite.Binding = 1;
            texWrite.Type = DescriptorType::CombinedImageSampler;
            texWrite.Texture = hdrTexture;
            // Note: we'd need device to update. Done from Renderer before execute.
        }

        cmd->BindPipeline(Pipeline.get());
        cmd->SetViewport(0, 0, static_cast<float>(target->GetWidth()), static_cast<float>(target->GetHeight()));
        cmd->SetScissor(0, 0, target->GetWidth(), target->GetHeight());
        cmd->BindDescriptorSet(0, DescSet.get());
        cmd->Draw(3); // Fullscreen triangle
    }

}

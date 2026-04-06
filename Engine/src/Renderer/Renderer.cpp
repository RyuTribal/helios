#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"

namespace Engine
{
    Renderer* Renderer::s_Instance = nullptr;

    // Default texture wrappers for backward compat with ModelImporter etc.
    Ref<Texture2D> Renderer::s_WhiteTexWrap;
    Ref<Texture2D> Renderer::s_BlackTexWrap;
    Ref<Texture2D> Renderer::s_GrayTexWrap;
    Ref<Texture2D> Renderer::s_BlueTexWrap;

    // Free function used by Texture.cpp and Buffer.cpp to access the device without
    // including the full Renderer header (avoids circular dependency).
    RHIDevice* GetRendererDevice()
    {
        return Renderer::GetDevice();
    }

    // ---- Cascade shadow helpers (ported from old Renderer) ----

    glm::mat4 Renderer::ComputeLightSpaceMatrix(Camera* camera, float nearPlane, float farPlane, const glm::vec3& lightDir)
    {
        const auto corners = camera->GetFrustumCornersWorldSpace();

        glm::vec3 minC(std::numeric_limits<float>::max());
        glm::vec3 maxC(std::numeric_limits<float>::lowest());
        for (const auto& v : corners)
        {
            glm::vec3 v3 = glm::vec3(v);
            minC = glm::min(minC, v3);
            maxC = glm::max(maxC, v3);
        }
        glm::vec3 center = (minC + maxC) / 2.f;

        const auto lightView = glm::lookAt(center + lightDir, center, glm::vec3(0.0f, 1.0f, 0.0f));

        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();
        for (const auto& v : corners)
        {
            const auto trf = lightView * v;
            minX = std::min(minX, trf.x);
            maxX = std::max(maxX, trf.x);
            minY = std::min(minY, trf.y);
            maxY = std::max(maxY, trf.y);
            minZ = std::min(minZ, trf.z);
            maxZ = std::max(maxZ, trf.z);
        }

        constexpr float zMult = 100.0f;
        if (minZ < 0) minZ *= zMult; else minZ /= zMult;
        if (maxZ < 0) maxZ /= zMult; else maxZ *= zMult;

        const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
        return lightProjection * lightView;
    }

    std::vector<glm::mat4> Renderer::ComputeCascadeLightMatrices(const glm::vec3& lightDir)
    {
        auto& shadow = m_Settings.ShadowSettings;
        std::vector<glm::mat4> matrices;

        for (size_t i = 0; i < shadow.ShadowCascadeLevels.size() + 1; ++i)
        {
            float near_p, far_p;
            if (i == 0)
            {
                near_p = m_CurrentCamera->GetNear();
                far_p = shadow.ShadowCascadeLevels[0];
            }
            else if (i < shadow.ShadowCascadeLevels.size())
            {
                near_p = shadow.ShadowCascadeLevels[i - 1];
                far_p = shadow.ShadowCascadeLevels[i];
            }
            else
            {
                near_p = shadow.ShadowCascadeLevels[i - 1];
                far_p = m_CurrentCamera->GetFar();
            }
            matrices.push_back(ComputeLightSpaceMatrix(m_CurrentCamera, near_p, far_p, lightDir));
        }
        return matrices;
    }

    // ---- Constructor / Destructor ----

    Renderer::Renderer(RHIDevice* device, RHISwapchain* swapchain)
        : m_Device(device), m_Swapchain(swapchain), m_PipelineCache(device)
    {
        HVE_CORE_INFO_TAG("Renderer", "Initializing modular Vulkan renderer");

        // Initialize default textures
        DefaultTextures::Init(device);

        // Create wrapper Texture2D objects for backward compatibility
        // These wrap the RHI textures from DefaultTextures into Texture2D objects
        // that existing code (ModelImporter, etc.) can use.
        TextureSpecification defaultSpec;
        defaultSpec.Format = ImageFormat::RGBA8;
        defaultSpec.Width = 1;
        defaultSpec.Height = 1;

        uint32_t whiteData = 0xFFFFFFFF;
        s_WhiteTexWrap = Texture2D::Create(defaultSpec, Buffer(&whiteData, sizeof(uint32_t)));

        uint32_t blackData = 0xFF000000;
        s_BlackTexWrap = Texture2D::Create(defaultSpec, Buffer(&blackData, sizeof(uint32_t)));

        uint32_t grayData = 0xFF808080;
        s_GrayTexWrap = Texture2D::Create(defaultSpec, Buffer(&grayData, sizeof(uint32_t)));

        uint32_t blueData = 0xFFFF8080;
        s_BlueTexWrap = Texture2D::Create(defaultSpec, Buffer(&blueData, sizeof(uint32_t)));

        // Get initial viewport size
        m_Width = static_cast<float>(swapchain->GetWidth());
        m_Height = static_cast<float>(swapchain->GetHeight());
        uint32_t w = static_cast<uint32_t>(m_Width);
        uint32_t h = static_cast<uint32_t>(m_Height);

        // Command buffer
        m_CommandBuffer = device->CreateCommandBuffer();

        // Initialize all render passes
        m_DepthPrePass.Init(device, &m_PipelineCache, w, h);
        m_ShadowPass.Init(device, &m_PipelineCache,
                           m_Settings.ShadowSettings.Resolution,
                           static_cast<uint32_t>(m_Settings.ShadowSettings.ShadowCascadeLevels.size() + 1));
        m_LightCulling.Init(device, &m_PipelineCache);
        m_ForwardPass.Init(device, &m_PipelineCache, w, h);

        // TonemapPass needs a swapchain render pass - we'll create a simple one
        // TODO: get render pass from swapchain or create one matching swapchain format
        RenderPassDesc sceneRPDesc;
        sceneRPDesc.ColorAttachments = {
            { swapchain->GetFormat(), 1, LoadOp::Clear, StoreOp::Store }
        };
        sceneRPDesc.DebugName = "SwapchainRenderPass";
        auto swapchainRP = device->CreateRenderPass(sceneRPDesc);
        m_TonemapPass.Init(device, &m_PipelineCache, swapchainRP.get());

        // Initialize skybox and debug renderers using the forward pass render pass
        m_SkyboxRenderer.Init(device, &m_PipelineCache, m_ForwardPass.RenderPass.get(), w, h);
        m_DebugRenderer.Init(device, &m_PipelineCache, m_ForwardPass.RenderPass.get(), w, h);

        HVE_CORE_INFO_TAG("Renderer", "Modular Vulkan renderer initialized ({}x{})", w, h);
    }

    Renderer::~Renderer()
    {
        if (m_Device)
            m_Device->WaitIdle();

        s_WhiteTexWrap.reset();
        s_BlackTexWrap.reset();
        s_GrayTexWrap.reset();
        s_BlueTexWrap.reset();

        DefaultTextures::Shutdown();
    }

    void Renderer::CreateRenderer(RHIDevice* device, RHISwapchain* swapchain)
    {
        if (!s_Instance)
        {
            s_Instance = new Renderer(device, swapchain);
        }
    }

    // ---- Submission ----

    void Renderer::SubmitObject(Ref<Mesh> mesh)
    {
        HVE_PROFILE_FUNC();
        if (!mesh->GetMeshSource())
            return;
        m_Meshes.push_back(mesh);
        m_Stats.vertices_count += mesh->GetMeshSource()->VertexSize();
        m_Stats.index_count += mesh->GetMeshSource()->IndexSize();
    }

    void Renderer::SubmitDebugLine(Line line) { m_DebugLines.push_back(line); }
    void Renderer::SubmitDebugBox(DebugBox box) { m_DebugBoxes.push_back(box); }
    void Renderer::SubmitDebugSphere(DebugSphere sphere) { m_DebugSpheres.push_back(sphere); }
    void Renderer::SubmitDebugCapsule(DebugCapsule capsule) { m_DebugCapsules.push_back(capsule); }

    // ---- Frame lifecycle ----

    void Renderer::BeginFrame(Camera* camera)
    {
        HVE_PROFILE_FUNC();

        if (camera && m_CurrentCamera)
            camera->SetAspectRatio(m_CurrentCamera->GetAspectRatio());

        SetCamera(camera);
        if (camera)
            camera->UpdateCamera();

        ResetStats();
    }

    void Renderer::BeginDrawing()
    {
        if (!m_CurrentCamera)
            return;

        auto* cmd = m_CommandBuffer.get();
        cmd->Begin();

        // 1. Depth pre-pass
        m_DepthPrePass.Execute(cmd, m_Meshes,
                                m_CurrentCamera->GetView(),
                                m_CurrentCamera->GetProjection());

        // 2. Shadow mapping
        if (!m_DirectionalLights.empty() && m_DirectionalLights[0]->IsCastingShadows())
        {
            glm::vec3 lightDir = -glm::normalize(m_DirectionalLights[0]->GetDirection());
            auto lightMatrices = ComputeCascadeLightMatrices(lightDir);
            m_ShadowPass.Execute(cmd, m_Meshes, lightMatrices);
        }

        // 3. Upload light data and cull
        m_LightCulling.UploadLights(m_Device, m_PointLights, m_DirectionalLights,
                                     m_CurrentCamera->GetView(), m_CurrentCamera->GetProjection(),
                                     static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));
        m_LightCulling.Execute(cmd, m_DepthPrePass.DepthTexture.get(),
                                static_cast<uint32_t>(m_PointLights.size()),
                                static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));

        // Barrier: compute -> fragment
        BarrierDesc barrier;
        barrier.SrcStage = ShaderStage::Compute;
        barrier.DstStage = ShaderStage::Fragment;
        cmd->PipelineBarrier(barrier);

        // 4. Prepare materials (allocate descriptor sets, upload UBO data)
        for (auto& mesh : m_Meshes)
        {
            if (!mesh->GetMeshSource()) continue;
            for (auto& mat : mesh->GetMeshSource()->GetMaterials())
            {
                m_ForwardPass.PrepareMaterial(m_Device, mat.get());
            }
        }

        // Update global descriptor set for forward pass
        {
            std::vector<DescriptorWrite> globalWrites;

            DescriptorWrite uboWrite;
            uboWrite.Binding = 0;
            uboWrite.Type = DescriptorType::UniformBuffer;
            uboWrite.Buffer = m_ForwardPass.GlobalUBO.get();
            uboWrite.Range = sizeof(GlobalUBOData);
            globalWrites.push_back(uboWrite);

            DescriptorWrite lightWrite;
            lightWrite.Binding = 1;
            lightWrite.Type = DescriptorType::StorageBuffer;
            lightWrite.Buffer = m_LightCulling.LightSSBO.get();
            lightWrite.Range = sizeof(PointLightInfo) * MAX_POINT_LIGHTS;
            globalWrites.push_back(lightWrite);

            DescriptorWrite dirWrite;
            dirWrite.Binding = 2;
            dirWrite.Type = DescriptorType::StorageBuffer;
            dirWrite.Buffer = m_LightCulling.DirLightSSBO.get();
            dirWrite.Range = sizeof(DirectionalLightInfo) * MAX_DIR_LIGHTS;
            globalWrites.push_back(dirWrite);

            DescriptorWrite visWrite;
            visWrite.Binding = 3;
            visWrite.Type = DescriptorType::StorageBuffer;
            visWrite.Buffer = m_LightCulling.VisibleIndicesSSBO.get();
            visWrite.Range = m_LightCulling.VisibleIndicesSSBO->GetSize();
            globalWrites.push_back(visWrite);

            DescriptorWrite lmWrite;
            lmWrite.Binding = 4;
            lmWrite.Type = DescriptorType::UniformBuffer;
            lmWrite.Buffer = m_ShadowPass.LightMatricesUBO.get();
            lmWrite.Range = sizeof(glm::mat4) * 16;
            globalWrites.push_back(lmWrite);

            m_Device->UpdateDescriptorSet(m_ForwardPass.GlobalDescSet.get(), globalWrites);
        }

        // 5. Forward shading
        uint32_t tilesX = (static_cast<uint32_t>(m_Width) + 15) / 16;
        m_ForwardPass.Execute(cmd, m_Meshes, m_CurrentCamera,
                               m_Settings.Skybox.Brightness,
                               static_cast<int>(m_DirectionalLights.size()),
                               static_cast<int>(tilesX),
                               &m_LightCulling,
                               m_ShadowPass.ShadowMap.get(),
                               m_ShadowPass.LightMatricesUBO.get());

        // 6. Skybox (rendered inside forward pass render pass - need to begin again or integrate)
        // For now skybox is a separate pass after forward
        // TODO: integrate skybox into forward pass for proper depth testing

        // 7. Debug rendering
        // TODO: debug renderer needs its own render pass begin/end within the forward framebuffer

        // 8. Tonemapping -> swapchain
        // TODO: need swapchain framebuffer integration
        // m_TonemapPass.Execute(cmd, swapchainFramebuffer, m_ForwardPass.ColorTexture.get(), m_Exposure);

        cmd->End();

        // Submit
        m_Device->SubmitCommandBuffer(cmd);

        // Present
        m_Swapchain->Present();
    }

    void Renderer::EndFrame()
    {
        m_Meshes.clear();
        m_PointLights.clear();
        m_DirectionalLights.clear();
        m_DebugLines.clear();
        m_DebugBoxes.clear();
        m_DebugSpheres.clear();
        m_DebugCapsules.clear();
    }

    // ---- Default textures ----

    Ref<Texture2D> Renderer::GetWhiteTexture() { return s_WhiteTexWrap; }
    Ref<Texture2D> Renderer::GetBlackTexture() { return s_BlackTexWrap; }
    Ref<Texture2D> Renderer::GetGrayTexture() { return s_GrayTexWrap; }
    Ref<Texture2D> Renderer::GetBlueTexture() { return s_BlueTexWrap; }

    // ---- Settings ----

    void Renderer::SetAntiAliasing(AntiAliasingSettings& settings)
    {
        m_Settings.AntiAliasing = settings;
        // TODO: recreate framebuffers with new sample count
    }

    void Renderer::SetSkybox(SkyboxSettings& settings)
    {
        m_Settings.Skybox = settings;
        // TODO: IBL generation via compute shaders
    }

    // ---- Viewport ----

    void Renderer::ResizeViewport(int width, int height)
    {
        HVE_PROFILE_FUNC();
        if (width == static_cast<int>(m_Width) && height == static_cast<int>(m_Height))
            return;

        m_Width = static_cast<float>(width);
        m_Height = static_cast<float>(height ? height : 1);

        if (m_CurrentCamera)
            m_CurrentCamera->SetAspectRatio(m_Width / m_Height);

        uint32_t w = static_cast<uint32_t>(m_Width);
        uint32_t h = static_cast<uint32_t>(m_Height);

        m_DepthPrePass.Resize(m_Device, w, h);
        m_ForwardPass.Resize(m_Device, w, h);
    }

    void Renderer::SetViewport(int width, int height)
    {
        // In Vulkan, viewport is set per-command buffer, not globally
        // This is a no-op; each pass sets its own viewport
    }

    void Renderer::ResetStats()
    {
        m_Stats.vertices_count = 0;
        m_Stats.draw_calls = 0;
        m_Stats.index_count = 0;
    }
}

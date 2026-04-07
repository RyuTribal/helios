#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/Vulkan/VulkanTexture.h"
#include "Renderer/Vulkan/VulkanPipeline.h"
#include "Renderer/Vulkan/VulkanDescriptor.h"
#include "Renderer/Vulkan/VulkanUtils.h"
#include "ImGui/imgui_impl_vulkan.h"
#include <imgui.h>

extern VkRenderPass GetImGuiRenderPass();

namespace Engine
{
    // Global device accessor used by Texture/Buffer Create methods
    RHIDevice* GetRendererDevice() { return Renderer::GetDevice(); }

    Renderer* Renderer::s_Instance = nullptr;

    // Default texture wrappers for backward compat with ModelImporter etc.
    Ref<Texture2D> Renderer::s_WhiteTexWrap;
    Ref<Texture2D> Renderer::s_BlackTexWrap;
    Ref<Texture2D> Renderer::s_GrayTexWrap;
    Ref<Texture2D> Renderer::s_BlueTexWrap;

    Renderer::Renderer(RHIDevice* device, RHISwapchain* swapchain)
        : m_Device(device), m_Swapchain(swapchain), m_PipelineCache(device)
    {
        // Default textures
        DefaultTextures::Init(device);

        // Command buffers (one per frame in flight)
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            m_CommandBuffers[i] = device->CreateCommandBuffer();

        // Initialize render passes
        m_DepthPrePass.Init(device, &m_PipelineCache,
                            static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));
        m_ShadowPass.Init(device, &m_PipelineCache,
                          m_Settings.ShadowSettings.Resolution,
                          static_cast<uint32_t>(m_Settings.ShadowSettings.ShadowCascadeLevels.size()) + 1);
        m_LightCulling.Init(device, &m_PipelineCache);

        auto* vkSwap = static_cast<VulkanSwapchain*>(swapchain);

        m_ForwardPass.Init(device, &m_PipelineCache,
                           static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));

        // Tonemap pass needs swapchain render pass
        RenderPassDesc swapRpDesc;
        AttachmentDesc swapColor;
        swapColor.Format = swapchain->GetFormat();
        swapColor.Load = LoadOp::Clear;
        swapColor.Store = StoreOp::Store;
        swapRpDesc.ColorAttachments.push_back(swapColor);
        swapRpDesc.HasDepth = false;
        swapRpDesc.DebugName = "SwapchainRenderPass";
        auto swapchainRP = device->CreateRenderPass(swapRpDesc);
        m_TonemapPass.Init(device, &m_PipelineCache, swapchainRP.get());

        m_SkyboxRenderer.Init(device, &m_PipelineCache,
                              m_ForwardPass.RenderPass.get(),
                              static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));

        m_DebugRenderer.Init(device, &m_PipelineCache,
                             m_ForwardPass.RenderPass.get(),
                             static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));

        // Tonemap compute pass (HDR forward pass → LDR for ImGui)
        {
            TextureDesc ldrDesc;
            ldrDesc.Width = static_cast<uint32_t>(m_Width);
            ldrDesc.Height = static_cast<uint32_t>(m_Height);
            ldrDesc.Format = ImageFormat::RGBA8;
            ldrDesc.Type = TextureType::Texture2D;
            ldrDesc.MipLevels = 1;
            ldrDesc.Usage = TextureUsage::Sampled | TextureUsage::Storage | TextureUsage::Transfer;
            ldrDesc.DebugName = "TonemapLDR";
            m_TonemapOutput = device->CreateTexture(ldrDesc);

            auto compShader = m_PipelineCache.LoadShader(
                "Resources/Shaders/tonemap.comp.spv", ShaderStage::Compute);

            DescriptorSetLayoutDesc layoutDesc;
            layoutDesc.Bindings = {
                { 0, DescriptorType::CombinedImageSampler, ShaderStage::Compute, 1 },
                { 1, DescriptorType::StorageImage, ShaderStage::Compute, 1 },
            };
            layoutDesc.DebugName = "TonemapComputeDescLayout";
            m_TonemapDescLayout = device->CreateDescriptorSetLayout(layoutDesc);
            m_TonemapDescSet = device->AllocateDescriptorSet(m_TonemapDescLayout.get());

            ComputePipelineDesc cpDesc;
            cpDesc.ComputeShader = compShader.get();
            cpDesc.DescriptorLayouts = { m_TonemapDescLayout.get() };
            cpDesc.PushConstantSize = sizeof(float); // exposure
            cpDesc.DebugName = "TonemapComputePipeline";
            m_TonemapComputePipeline = m_PipelineCache.GetOrCreateComputePipeline("tonemap_compute", cpDesc);
        }

        // Create ImGui framebuffers for each swapchain image
        RecreateImGuiFramebuffers();

        HVE_CORE_INFO_TAG("Renderer", "Modular Vulkan renderer initialized ({}x{})",
                          (int)m_Width, (int)m_Height);
    }

    void Renderer::RecreateImGuiFramebuffers()
    {
        auto* vkSwapchain = static_cast<VulkanSwapchain*>(m_Swapchain);
        VkDevice device = static_cast<VulkanDevice*>(m_Device)->GetDevice();
        VkRenderPass imguiRP = ::GetImGuiRenderPass();

        // Destroy old framebuffers
        for (auto fb : m_ImGuiFramebuffers)
            vkDestroyFramebuffer(device, fb, nullptr);
        m_ImGuiFramebuffers.clear();

        // Create one per swapchain image
        const auto& imageViews = vkSwapchain->GetImageViews();
        for (uint32_t i = 0; i < imageViews.size(); i++)
        {
            VkImageView view = imageViews[i];
            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = imguiRP;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments = &view;
            fbInfo.width = vkSwapchain->GetWidth();
            fbInfo.height = vkSwapchain->GetHeight();
            fbInfo.layers = 1;

            VkFramebuffer fb;
            vkCreateFramebuffer(device, &fbInfo, nullptr, &fb);
            m_ImGuiFramebuffers.push_back(fb);
        }
    }

    Renderer::~Renderer()
    {
        if (m_Device)
        {
            m_Device->WaitIdle();

            // Release ImGui scene texture descriptor
            if (m_SceneImGuiDescriptor != VK_NULL_HANDLE)
            {
                ImGui_ImplVulkan_RemoveTexture(m_SceneImGuiDescriptor);
                m_SceneImGuiDescriptor = VK_NULL_HANDLE;
            }

            VkDevice device = static_cast<VulkanDevice*>(m_Device)->GetDevice();
            for (auto fb : m_ImGuiFramebuffers)
                vkDestroyFramebuffer(device, fb, nullptr);
            m_ImGuiFramebuffers.clear();

            // Release IBL textures
            m_IrradianceRHI.reset();
            m_PrefilterRHI.reset();
            m_BrdfRHI.reset();

            // Release tonemap resources
            m_TonemapOutput.reset();
            m_TonemapComputePipeline.reset();
            m_TonemapDescLayout.reset();
            m_TonemapDescSet.reset();

            // Release command buffers
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
                m_CommandBuffers[i].reset();

            // Release default texture wrappers
            s_WhiteTexWrap.reset();
            s_BlackTexWrap.reset();
            s_GrayTexWrap.reset();
            s_BlueTexWrap.reset();
        }
        DefaultTextures::Shutdown();
    }

    void Renderer::CreateRenderer(RHIDevice* device, RHISwapchain* swapchain)
    {
        if (!s_Instance)
        {
            s_Instance = new Renderer(device, swapchain);
        }
    }

    void Renderer::SubmitObject(Ref<Mesh> mesh)
    {
        if (!mesh || !mesh->GetMeshSource()) return;
        m_Meshes.push_back(mesh);
        m_Stats.vertices_count += mesh->GetMeshSource()->VertexSize();
        m_Stats.index_count += mesh->GetMeshSource()->IndexSize();
    }

    void Renderer::SubmitDebugLine(Line line) { m_DebugLines.push_back(line); }
    void Renderer::SubmitDebugBox(DebugBox box) { m_DebugBoxes.push_back(box); }
    void Renderer::SubmitDebugSphere(DebugSphere sphere) { m_DebugSpheres.push_back(sphere); }
    void Renderer::SubmitDebugCapsule(DebugCapsule capsule) { m_DebugCapsules.push_back(capsule); }

    Ref<Texture2D> Renderer::GetWhiteTexture() { return s_WhiteTexWrap; }
    Ref<Texture2D> Renderer::GetBlackTexture() { return s_BlackTexWrap; }
    Ref<Texture2D> Renderer::GetGrayTexture() { return s_GrayTexWrap; }
    Ref<Texture2D> Renderer::GetBlueTexture() { return s_BlueTexWrap; }

    void Renderer::BeginFrame(Camera* camera)
    {
        // Apply deferred viewport resize BEFORE ImGui render, so any descriptor
        // created in GetSceneTextureID points to the current (not destroyed) texture.
        if (m_ViewportResizePending)
        {
            m_Device->WaitIdle();
            m_Width = static_cast<float>(m_PendingViewportW);
            m_Height = static_cast<float>(m_PendingViewportH);
            m_DepthPrePass.Resize(m_Device, m_PendingViewportW, m_PendingViewportH);
            m_ForwardPass.Resize(m_Device, m_PendingViewportW, m_PendingViewportH);
            // Recreate tonemap LDR output at new size
            TextureDesc ldrDesc;
            ldrDesc.Width = m_PendingViewportW;
            ldrDesc.Height = m_PendingViewportH;
            ldrDesc.Format = ImageFormat::RGBA8;
            ldrDesc.Type = TextureType::Texture2D;
            ldrDesc.MipLevels = 1;
            ldrDesc.Usage = TextureUsage::Sampled | TextureUsage::Storage | TextureUsage::Transfer;
            ldrDesc.DebugName = "TonemapLDR";
            m_TonemapOutput = m_Device->CreateTexture(ldrDesc);
            m_SceneImGuiDescriptor = VK_NULL_HANDLE;
            m_ViewportResizePending = false;
        }

        m_CurrentCamera = camera;
        if (camera && m_CurrentCamera)
        {
            camera->SetAspectRatio(m_Width / m_Height);
            camera->UpdateCamera();
        }
        ResetStats();
    }

    void Renderer::BeginDrawing()
    {
        // Acquire next swapchain image
        if (!m_Swapchain->AcquireNextImage())
        {
            auto& window = Application::Get().GetWindow();
            uint32_t w = window.GetWidth();
            uint32_t h = window.GetHeight();
            if (w > 0 && h > 0)
            {
                m_Device->WaitIdle();
                m_Swapchain->Resize(w, h);
                RecreateImGuiFramebuffers();
            }
            return;
        }

        auto* vkSwapchain = static_cast<VulkanSwapchain*>(m_Swapchain);
        auto* cmd = m_CommandBuffers[vkSwapchain->GetCurrentFrame()].get();
        auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);
        VkCommandBuffer vkCmdBuf = vkCmd->GetVkCommandBuffer();

        cmd->Begin();

        // ---- 3D Render Passes ----
        if (m_CurrentCamera)
        {
            const glm::mat4& view = m_CurrentCamera->GetView();
            const glm::mat4& proj = m_CurrentCamera->GetProjection();
            uint32_t vpW = static_cast<uint32_t>(m_Width);
            uint32_t vpH = static_cast<uint32_t>(m_Height);

            // Prepare materials (default textures must match shader sampler types)
            RHITexture* def2D    = DefaultTextures::White().get();
            RHITexture* defCube  = DefaultTextures::BlackCube().get();
            RHITexture* defArray = DefaultTextures::WhiteArray().get();

            // Use proper IBL textures from generated compute results
            RHITexture* irradianceTex = defCube;
            RHITexture* prefilterTex  = defCube;
            RHITexture* brdfTex       = DefaultTextures::BrdfLUT().get();

            if (m_IrradianceRHI)
            {
                auto* vkTex = static_cast<VulkanTexture*>(m_IrradianceRHI.get());
                if (vkTex->GetCurrentLayout() != VK_IMAGE_LAYOUT_UNDEFINED)
                    irradianceTex = m_IrradianceRHI.get();
            }
            if (m_PrefilterRHI)
            {
                auto* vkTex = static_cast<VulkanTexture*>(m_PrefilterRHI.get());
                if (vkTex->GetCurrentLayout() != VK_IMAGE_LAYOUT_UNDEFINED)
                    prefilterTex = m_PrefilterRHI.get();
            }
            if (m_BrdfRHI)
            {
                auto* vkTex = static_cast<VulkanTexture*>(m_BrdfRHI.get());
                if (vkTex->GetCurrentLayout() != VK_IMAGE_LAYOUT_UNDEFINED)
                    brdfTex = m_BrdfRHI.get();
            }

            // Force all materials dirty when IBL textures just became available
            if (m_IBLJustGenerated)
            {
                for (auto& mesh : m_Meshes)
                {
                    if (!mesh->GetMeshSource()) continue;
                    auto& materials = mesh->GetMeshSource()->GetMaterials();
                    for (auto& mat : materials)
                        mat->Set("_IBLRefresh", true); // triggers m_Dirty = true
                }
                m_IBLJustGenerated = false;
            }

            for (auto& mesh : m_Meshes)
            {
                if (!mesh->GetMeshSource()) continue;
                auto& materials = mesh->GetMeshSource()->GetMaterials();
                for (auto& mat : materials)
                {
                    m_ForwardPass.PrepareMaterial(m_Device, mat.get(), def2D, defCube, defArray,
                                                  irradianceTex, prefilterTex, brdfTex);
                }
            }

            // Update forward pass global descriptor set
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

            // Upload light data to SSBOs (needed by forward pass shader even without compute culling)
            m_LightCulling.UploadLights(m_Device, m_PointLights, m_DirectionalLights,
                                         view, proj, vpW, vpH);

            // 1. Depth pre-pass (self-contained render pass + transitions)
            m_DepthPrePass.Execute(cmd, m_Meshes, view, proj);

            // 2. Shadow pass (only when directional lights are present)
            if (!m_DirectionalLights.empty())
            {
                glm::vec3 lightDir = m_DirectionalLights[0]->GetDirection();
                auto cascadeMatrices = ComputeCascadeLightMatrices(lightDir);
                if (!cascadeMatrices.empty())
                    m_ShadowPass.Execute(cmd, m_Meshes, cascadeMatrices);
            }

            // 3. Light culling compute (depth texture is already in SHADER_READ_ONLY from depth pre-pass)
            if (!m_PointLights.empty() || !m_DirectionalLights.empty())
            {
                // Update light culling descriptor set with all bindings including depth texture
                {
                    std::vector<DescriptorWrite> writes;

                    DescriptorWrite paramsWrite;
                    paramsWrite.Binding = 0;
                    paramsWrite.Type = DescriptorType::UniformBuffer;
                    paramsWrite.Buffer = m_LightCulling.ParamsUBO.get();
                    paramsWrite.Range = 256;
                    writes.push_back(paramsWrite);

                    DescriptorWrite lightWrite;
                    lightWrite.Binding = 1;
                    lightWrite.Type = DescriptorType::StorageBuffer;
                    lightWrite.Buffer = m_LightCulling.LightSSBO.get();
                    lightWrite.Range = sizeof(PointLightInfo) * MAX_POINT_LIGHTS;
                    writes.push_back(lightWrite);

                    DescriptorWrite visWrite;
                    visWrite.Binding = 2;
                    visWrite.Type = DescriptorType::StorageBuffer;
                    visWrite.Buffer = m_LightCulling.VisibleIndicesSSBO.get();
                    visWrite.Range = m_LightCulling.VisibleIndicesSSBO->GetSize();
                    writes.push_back(visWrite);

                    DescriptorWrite depthWrite;
                    depthWrite.Binding = 3;
                    depthWrite.Type = DescriptorType::CombinedImageSampler;
                    depthWrite.Texture = m_DepthPrePass.DepthTexture.get();
                    writes.push_back(depthWrite);

                    m_Device->UpdateDescriptorSet(m_LightCulling.DescSet.get(), writes);
                }

                m_LightCulling.Execute(cmd, m_DepthPrePass.DepthTexture.get(),
                    static_cast<uint32_t>(m_PointLights.size()), vpW, vpH);

                // Barrier: compute SSBO writes → fragment shader reads
                VkMemoryBarrier2 memBarrier{};
                memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                memBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
                memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                memBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

                VkDependencyInfo depInfo{};
                depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                depInfo.memoryBarrierCount = 1;
                depInfo.pMemoryBarriers = &memBarrier;
                vkCmdPipelineBarrier2(vkCmdBuf, &depInfo);
            }

            // 4. Forward pass — Renderer manages render pass for skybox/debug interleaving
            auto* vkColorTex = static_cast<VulkanTexture*>(m_ForwardPass.ColorTexture.get());

            ClearValues fwdClear;
            fwdClear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };
            fwdClear.Depth = 1.0f;
            cmd->BeginRenderPass(m_ForwardPass.RenderPass.get(),
                                 m_ForwardPass.Framebuffer.get(), fwdClear);

            float envBrightness = m_Settings.Skybox.Brightness;

            int numTilesX = (vpW + 15) / 16;
            m_ForwardPass.Execute(cmd, m_Meshes, m_CurrentCamera,
                envBrightness,
                static_cast<int>(m_DirectionalLights.size()),
                numTilesX,
                &m_LightCulling,
                m_ShadowPass.ShadowMap.get(),
                m_ShadowPass.LightMatricesUBO.get());

            // Skybox (inside same render pass) — only if cubemap has actual data
            // (equirect-to-cube conversion not yet ported to Vulkan)
            auto* skyboxRHI = m_Settings.Skybox.Texture ? m_Settings.Skybox.Texture->GetRHITexture() : nullptr;
            bool skyboxReady = skyboxRHI && static_cast<VulkanTexture*>(skyboxRHI)->GetCurrentLayout()
                               != VK_IMAGE_LAYOUT_UNDEFINED;
            if (skyboxReady)
            {
                SkyboxRenderData skyboxData;
                skyboxData.CubeTexture = m_Settings.Skybox.Texture->GetRHITexture();
                skyboxData.Brightness = m_Settings.Skybox.Brightness;
                if (m_IrradianceRHI)
                    skyboxData.IrradianceTexture = m_IrradianceRHI.get();
                if (m_PrefilterRHI)
                    skyboxData.PrefilterMap = m_PrefilterRHI.get();
                if (m_Settings.Skybox.BRDFTexture)
                    skyboxData.BRDFTexture = m_Settings.Skybox.BRDFTexture.get();
                m_SkyboxRenderer.Execute(cmd, m_ForwardPass.Framebuffer.get(),
                                         m_CurrentCamera, skyboxData);
            }

            // Debug primitives (inside same render pass)
            if (!m_DebugLines.empty() || !m_DebugBoxes.empty() ||
                !m_DebugSpheres.empty() || !m_DebugCapsules.empty())
            {
                m_DebugRenderer.Execute(cmd, m_ForwardPass.Framebuffer.get(),
                    m_DebugLines, m_DebugBoxes, m_DebugSpheres, m_DebugCapsules,
                    m_CurrentCamera);
            }

            cmd->EndRenderPass();

            // Transition forward color to SHADER_READ_ONLY for tonemap input
            VulkanTexture::TransitionLayout(vkCmdBuf, vkColorTex->GetVkImage(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // Tonemap compute pass: HDR (RGBA16F) → LDR (RGBA8)
            if (m_TonemapComputePipeline && m_TonemapOutput)
            {
                auto* vkLDR = static_cast<VulkanTexture*>(m_TonemapOutput.get());

                // Update tonemap descriptor set with current textures
                DescriptorWrite hdrIn;
                hdrIn.Binding = 0;
                hdrIn.Type = DescriptorType::CombinedImageSampler;
                hdrIn.Texture = m_ForwardPass.ColorTexture.get();
                DescriptorWrite ldrOut;
                ldrOut.Binding = 1;
                ldrOut.Type = DescriptorType::StorageImage;
                ldrOut.Texture = m_TonemapOutput.get();
                m_Device->UpdateDescriptorSet(m_TonemapDescSet.get(), { hdrIn, ldrOut });

                // Transition LDR output to GENERAL for storage write
                VulkanTexture::TransitionLayout(vkCmdBuf, vkLDR->GetVkImage(),
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                auto* vkPipe = static_cast<VulkanPipeline*>(m_TonemapComputePipeline.get());
                auto* vkDesc = static_cast<VulkanDescriptorSet*>(m_TonemapDescSet.get());
                vkCmdBindPipeline(vkCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipe->GetVkPipeline());
                VkDescriptorSet ds = vkDesc->GetVkSet();
                vkCmdBindDescriptorSets(vkCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                    vkPipe->GetLayout(), 0, 1, &ds, 0, nullptr);

                float exposure = m_Exposure;
                vkCmdPushConstants(vkCmdBuf, vkPipe->GetLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &exposure);

                uint32_t gx = (vpW + 15) / 16;
                uint32_t gy = (vpH + 15) / 16;
                vkCmdDispatch(vkCmdBuf, gx, gy, 1);

                // Transition LDR to SHADER_READ_ONLY for ImGui sampling
                VulkanTexture::TransitionLayout(vkCmdBuf, vkLDR->GetVkImage(),
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // ---- ImGui Pass (on swapchain) ----
        VkRenderPass imguiRP = ::GetImGuiRenderPass();
        uint32_t imageIndex = vkSwapchain->GetCurrentImageIndex();
        VkFramebuffer imguiFB = m_ImGuiFramebuffers[imageIndex];

        VulkanTexture::TransitionLayout(vkCmdBuf,
            vkSwapchain->GetCurrentVkImage(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderPassBeginInfo rpBeginInfo{};
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass = imguiRP;
        rpBeginInfo.framebuffer = imguiFB;
        rpBeginInfo.renderArea = {{0, 0}, {vkSwapchain->GetWidth(), vkSwapchain->GetHeight()}};
        VkClearValue clearValue{};
        clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};
        rpBeginInfo.clearValueCount = 1;
        rpBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(vkCmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->TotalVtxCount > 0 && drawData->CmdListsCount > 0)
            ImGui_ImplVulkan_RenderDrawData(drawData, vkCmdBuf);

        vkCmdEndRenderPass(vkCmdBuf);

        cmd->End();

        // Submit with swapchain sync
        VkSemaphore waitSem = vkSwapchain->GetImageAvailableSemaphore();
        VkSemaphore signalSem = vkSwapchain->GetRenderFinishedSemaphore();
        VkFence fence = vkSwapchain->GetInFlightFence();
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSem;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &vkCmdBuf;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSem;
        vkQueueSubmit(static_cast<VulkanDevice*>(m_Device)->GetGraphicsQueue(), 1, &submitInfo, fence);

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

    void Renderer::SetAntiAliasing(AntiAliasingSettings& settings)
    {
        m_Settings.AntiAliasing = settings;
    }

    void Renderer::SetSkybox(SkyboxSettings& settings)
    {
        m_Settings.Skybox = settings;

        // Convert equirectangular map → cubemap if the cubemap has no data yet
        if (settings.Texture && settings.Texture->GetFlatTexture())
        {
            auto* cubeRHI = settings.Texture->GetRHITexture();
            auto* flatRHI = settings.Texture->GetFlatTexture()->GetRHITexture();
            if (cubeRHI && flatRHI)
            {
                auto* vkCube = static_cast<VulkanTexture*>(cubeRHI);
                if (vkCube->GetCurrentLayout() == VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    ConvertEquirectToCube(flatRHI, cubeRHI, settings.Texture->GetWidth());
                    GenerateIBL();
                }
            }
        }
    }

    void Renderer::ConvertEquirectToCube(RHITexture* equirect, RHITexture* cubemap, uint32_t cubeSize)
    {
        // Load compute shader
        auto compShader = m_PipelineCache.LoadShader(
            "Resources/Shaders/equirect_to_cube.comp.spv", ShaderStage::Compute);
        if (!compShader) {
            HVE_CORE_ERROR_TAG("Renderer", "Failed to load equirect_to_cube compute shader");
            return;
        }

        // Descriptor layout: binding 0 = sampler2D, binding 1 = imageCube
        DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Bindings = {
            { 0, DescriptorType::CombinedImageSampler, ShaderStage::Compute, 1 },
            { 1, DescriptorType::StorageImage, ShaderStage::Compute, 1 },
        };
        layoutDesc.DebugName = "EquirectToCubeDescLayout";
        auto descLayout = m_Device->CreateDescriptorSetLayout(layoutDesc);
        auto descSet = m_Device->AllocateDescriptorSet(descLayout.get());

        // Create pipeline
        ComputePipelineDesc pipeDesc;
        pipeDesc.ComputeShader = compShader.get();
        pipeDesc.DescriptorLayouts = { descLayout.get() };
        pipeDesc.DebugName = "EquirectToCubePipeline";
        auto pipeline = m_PipelineCache.GetOrCreateComputePipeline("equirect_to_cube", pipeDesc);
        if (!pipeline) {
            HVE_CORE_ERROR_TAG("Renderer", "Failed to create equirect_to_cube pipeline");
            return;
        }

        // Update descriptor set
        DescriptorWrite samplerWrite;
        samplerWrite.Binding = 0;
        samplerWrite.Type = DescriptorType::CombinedImageSampler;
        samplerWrite.Texture = equirect;

        DescriptorWrite storageWrite;
        storageWrite.Binding = 1;
        storageWrite.Type = DescriptorType::StorageImage;
        storageWrite.Texture = cubemap;

        m_Device->UpdateDescriptorSet(descSet.get(), { samplerWrite, storageWrite });

        // Dispatch via ImmediateSubmit
        auto* vkDevice = static_cast<VulkanDevice*>(m_Device);
        auto* vkCube = static_cast<VulkanTexture*>(cubemap);
        auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline.get());
        auto* vkDescSet = static_cast<VulkanDescriptorSet*>(descSet.get());

        vkDevice->ImmediateSubmit([&](VkCommandBuffer cmd) {
            // Transition cubemap UNDEFINED → GENERAL for storage write
            VulkanTexture::TransitionLayout(cmd, vkCube->GetVkImage(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->GetVkPipeline());
            VkDescriptorSet vkDS = vkDescSet->GetVkSet();
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                vkPipeline->GetLayout(), 0, 1, &vkDS, 0, nullptr);

            uint32_t groupsX = (cubeSize + 15) / 16;
            uint32_t groupsY = (cubeSize + 15) / 16;
            vkCmdDispatch(cmd, groupsX, groupsY, 6); // 6 faces

            // Transition cubemap GENERAL → SHADER_READ_ONLY
            VulkanTexture::TransitionLayout(cmd, vkCube->GetVkImage(),
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

        vkCube->SetCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        HVE_CORE_INFO_TAG("Renderer", "Converted equirectangular map to cubemap ({}x{})", cubeSize, cubeSize);
    }

    void Renderer::GenerateIBL()
    {
        if (!m_Settings.Skybox.Texture || !m_Settings.Skybox.Texture->GetRHITexture())
            return;

        auto* envCubeRHI = m_Settings.Skybox.Texture->GetRHITexture();
        auto* vkEnvCube = static_cast<VulkanTexture*>(envCubeRHI);
        if (vkEnvCube->GetCurrentLayout() == VK_IMAGE_LAYOUT_UNDEFINED)
            return;

        auto* vkDevice = static_cast<VulkanDevice*>(m_Device);
        VkDevice device = vkDevice->GetDevice();

        // ================================================================
        // 1. IRRADIANCE CONVOLUTION
        // ================================================================
        {
            uint32_t irradSize = static_cast<uint32_t>(m_Settings.Skybox.IrradianceResolution);

            // Create irradiance cubemap texture
            TextureDesc irradDesc;
            irradDesc.Width = irradSize;
            irradDesc.Height = irradSize;
            irradDesc.Format = ImageFormat::RGBA16F;
            irradDesc.Type = TextureType::TextureCube;
            irradDesc.MipLevels = 1;
            irradDesc.ArrayLayers = 6;
            irradDesc.Usage = TextureUsage::Sampled | TextureUsage::Storage | TextureUsage::Transfer;
            irradDesc.ClampToEdge = true;
            irradDesc.DebugName = "IrradianceMap";

            auto irradRHI = m_Device->CreateTexture(irradDesc);

            // Load shader and create pipeline
            auto compShader = m_PipelineCache.LoadShader(
                "Resources/Shaders/irradiance_convolution.comp.spv", ShaderStage::Compute);
            if (!compShader) {
                HVE_CORE_ERROR_TAG("Renderer", "Failed to load irradiance_convolution compute shader");
                return;
            }

            DescriptorSetLayoutDesc layoutDesc;
            layoutDesc.Bindings = {
                { 0, DescriptorType::CombinedImageSampler, ShaderStage::Compute, 1 },
                { 1, DescriptorType::StorageImage, ShaderStage::Compute, 1 },
            };
            layoutDesc.DebugName = "IrradianceConvDescLayout";
            auto descLayout = m_Device->CreateDescriptorSetLayout(layoutDesc);
            auto descSet = m_Device->AllocateDescriptorSet(descLayout.get());

            ComputePipelineDesc pipeDesc;
            pipeDesc.ComputeShader = compShader.get();
            pipeDesc.DescriptorLayouts = { descLayout.get() };
            pipeDesc.DebugName = "IrradianceConvPipeline";
            auto pipeline = m_PipelineCache.GetOrCreateComputePipeline("irradiance_conv", pipeDesc);
            if (!pipeline) {
                HVE_CORE_ERROR_TAG("Renderer", "Failed to create irradiance convolution pipeline");
                return;
            }

            // Update descriptor set
            DescriptorWrite samplerWrite;
            samplerWrite.Binding = 0;
            samplerWrite.Type = DescriptorType::CombinedImageSampler;
            samplerWrite.Texture = envCubeRHI;

            DescriptorWrite storageWrite;
            storageWrite.Binding = 1;
            storageWrite.Type = DescriptorType::StorageImage;
            storageWrite.Texture = irradRHI.get();

            m_Device->UpdateDescriptorSet(descSet.get(), { samplerWrite, storageWrite });

            auto* vkIrrad = static_cast<VulkanTexture*>(irradRHI.get());
            auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline.get());
            auto* vkDescSet = static_cast<VulkanDescriptorSet*>(descSet.get());

            vkDevice->ImmediateSubmit([&](VkCommandBuffer cmd) {
                // Transition irradiance map UNDEFINED → GENERAL
                VulkanTexture::TransitionLayout(cmd, vkIrrad->GetVkImage(),
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->GetVkPipeline());
                VkDescriptorSet vkDS = vkDescSet->GetVkSet();
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    vkPipeline->GetLayout(), 0, 1, &vkDS, 0, nullptr);

                uint32_t groupsX = (irradSize + 15) / 16;
                uint32_t groupsY = (irradSize + 15) / 16;
                vkCmdDispatch(cmd, groupsX, groupsY, 6);

                // Transition irradiance map GENERAL → SHADER_READ_ONLY
                VulkanTexture::TransitionLayout(cmd, vkIrrad->GetVkImage(),
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            });

            vkIrrad->SetCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // Store in SkyboxSettings via a TextureCube wrapper
            if (!m_Settings.Skybox.IrradianceTexture)
            {
                // Create a TextureCube that wraps our generated RHI texture
                m_Settings.Skybox.IrradianceTexture = TextureCube::Create(nullptr, irradSize, ImageFormat::RGBA16F);
            }
            // Replace the RHI texture inside (the TextureCube constructor created a blank one)
            // We store the generated texture directly
            m_IrradianceRHI = irradRHI;

            HVE_CORE_INFO_TAG("Renderer", "Generated irradiance map ({}x{})", irradSize, irradSize);
        }

        // ================================================================
        // 2. PREFILTER ENVIRONMENT MAP (with mip levels)
        // ================================================================
        {
            uint32_t prefilterSize = static_cast<uint32_t>(m_Settings.Skybox.PrefilterResolution);
            uint32_t maxMipLevels = static_cast<uint32_t>(std::floor(std::log2(double(prefilterSize)))) + 1;

            // Create prefilter cubemap with mip levels
            TextureDesc prefDesc;
            prefDesc.Width = prefilterSize;
            prefDesc.Height = prefilterSize;
            prefDesc.Format = ImageFormat::RGBA16F;
            prefDesc.Type = TextureType::TextureCube;
            prefDesc.MipLevels = maxMipLevels;
            prefDesc.ArrayLayers = 6;
            prefDesc.Usage = TextureUsage::Sampled | TextureUsage::Storage | TextureUsage::Transfer;
            prefDesc.ClampToEdge = true;
            prefDesc.DebugName = "PrefilterMap";

            auto prefRHI = m_Device->CreateTexture(prefDesc);

            // Load shader and create pipeline (with push constants for roughness/mip size)
            auto compShader = m_PipelineCache.LoadShader(
                "Resources/Shaders/prefilter_envmap.comp.spv", ShaderStage::Compute);
            if (!compShader) {
                HVE_CORE_ERROR_TAG("Renderer", "Failed to load prefilter_envmap compute shader");
                return;
            }

            DescriptorSetLayoutDesc layoutDesc;
            layoutDesc.Bindings = {
                { 0, DescriptorType::CombinedImageSampler, ShaderStage::Compute, 1 },
                { 1, DescriptorType::StorageImage, ShaderStage::Compute, 1 },
            };
            layoutDesc.DebugName = "PrefilterDescLayout";
            auto descLayout = m_Device->CreateDescriptorSetLayout(layoutDesc);

            struct PrefilterPushConstants {
                float roughness;
                uint32_t mipSize;
            };

            ComputePipelineDesc pipeDesc;
            pipeDesc.ComputeShader = compShader.get();
            pipeDesc.DescriptorLayouts = { descLayout.get() };
            pipeDesc.PushConstantSize = sizeof(PrefilterPushConstants);
            pipeDesc.DebugName = "PrefilterPipeline";
            auto pipeline = m_PipelineCache.GetOrCreateComputePipeline("prefilter_envmap", pipeDesc);
            if (!pipeline) {
                HVE_CORE_ERROR_TAG("Renderer", "Failed to create prefilter pipeline");
                return;
            }

            auto* vkPref = static_cast<VulkanTexture*>(prefRHI.get());
            auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline.get());
            VkFormat vkFormat = ToVkFormat(ImageFormat::RGBA16F);

            // Create per-mip image views for storage writes
            std::vector<VkImageView> mipViews(maxMipLevels);
            for (uint32_t mip = 0; mip < maxMipLevels; mip++)
            {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = vkPref->GetVkImage();
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
                viewInfo.format = vkFormat;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel = mip;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 6;

                vkCreateImageView(device, &viewInfo, nullptr, &mipViews[mip]);
            }

            // Allocate one descriptor set per mip level
            std::vector<Ref<RHIDescriptorSet>> descSets(maxMipLevels);
            for (uint32_t mip = 0; mip < maxMipLevels; mip++)
            {
                descSets[mip] = m_Device->AllocateDescriptorSet(descLayout.get());

                // Write environment map sampler (binding 0)
                DescriptorWrite samplerWrite;
                samplerWrite.Binding = 0;
                samplerWrite.Type = DescriptorType::CombinedImageSampler;
                samplerWrite.Texture = envCubeRHI;

                // For storage image (binding 1), we need to use the per-mip view
                // We'll write this manually via raw Vulkan
                m_Device->UpdateDescriptorSet(descSets[mip].get(), { samplerWrite });

                // Now manually update the storage image binding with per-mip view
                auto* vkDescSet = static_cast<VulkanDescriptorSet*>(descSets[mip].get());
                VkDescriptorImageInfo imgInfo{};
                imgInfo.sampler = VK_NULL_HANDLE;
                imgInfo.imageView = mipViews[mip];
                imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                VkWriteDescriptorSet vkWrite{};
                vkWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                vkWrite.dstSet = vkDescSet->GetVkSet();
                vkWrite.dstBinding = 1;
                vkWrite.dstArrayElement = 0;
                vkWrite.descriptorCount = 1;
                vkWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                vkWrite.pImageInfo = &imgInfo;

                vkUpdateDescriptorSets(device, 1, &vkWrite, 0, nullptr);
            }

            vkDevice->ImmediateSubmit([&](VkCommandBuffer cmd) {
                // Transition entire prefilter image UNDEFINED → GENERAL
                VulkanTexture::TransitionLayout(cmd, vkPref->GetVkImage(),
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->GetVkPipeline());

                for (uint32_t mip = 0; mip < maxMipLevels; mip++)
                {
                    uint32_t mipSize = prefilterSize >> mip;
                    if (mipSize < 1) mipSize = 1;
                    float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);

                    PrefilterPushConstants pc;
                    pc.roughness = roughness;
                    pc.mipSize = mipSize;
                    vkCmdPushConstants(cmd, vkPipeline->GetLayout(),
                        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PrefilterPushConstants), &pc);

                    auto* vkDescSet = static_cast<VulkanDescriptorSet*>(descSets[mip].get());
                    VkDescriptorSet vkDS = vkDescSet->GetVkSet();
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                        vkPipeline->GetLayout(), 0, 1, &vkDS, 0, nullptr);

                    uint32_t groupsX = (mipSize + 15) / 16;
                    uint32_t groupsY = (mipSize + 15) / 16;
                    vkCmdDispatch(cmd, groupsX, groupsY, 6);

                    // Barrier between mip dispatches
                    if (mip < maxMipLevels - 1)
                    {
                        VkMemoryBarrier2 memBarrier{};
                        memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                        memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                        memBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
                        memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                        memBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

                        VkDependencyInfo depInfo{};
                        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                        depInfo.memoryBarrierCount = 1;
                        depInfo.pMemoryBarriers = &memBarrier;
                        vkCmdPipelineBarrier2(cmd, &depInfo);
                    }
                }

                // Transition prefilter map GENERAL → SHADER_READ_ONLY
                VulkanTexture::TransitionLayout(cmd, vkPref->GetVkImage(),
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            });

            vkPref->SetCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // Clean up per-mip image views
            for (auto& view : mipViews)
                vkDestroyImageView(device, view, nullptr);

            // Store result
            if (!m_Settings.Skybox.PrefilterMap)
            {
                m_Settings.Skybox.PrefilterMap = TextureCube::Create(nullptr, prefilterSize, ImageFormat::RGBA16F);
            }
            m_PrefilterRHI = prefRHI;

            HVE_CORE_INFO_TAG("Renderer", "Generated prefilter map ({}x{}, {} mips)", prefilterSize, prefilterSize, maxMipLevels);
        }

        // ================================================================
        // 3. BRDF LUT
        // ================================================================
        {
            uint32_t brdfSize = 512;

            TextureDesc brdfDesc;
            brdfDesc.Width = brdfSize;
            brdfDesc.Height = brdfSize;
            brdfDesc.Format = ImageFormat::RG16F;
            brdfDesc.Type = TextureType::Texture2D;
            brdfDesc.MipLevels = 1;
            brdfDesc.Usage = TextureUsage::Sampled | TextureUsage::Storage | TextureUsage::Transfer;
            brdfDesc.ClampToEdge = true;
            brdfDesc.DebugName = "BRDF_LUT";

            auto brdfRHI = m_Device->CreateTexture(brdfDesc);

            auto compShader = m_PipelineCache.LoadShader(
                "Resources/Shaders/brdf_lut.comp.spv", ShaderStage::Compute);
            if (!compShader) {
                HVE_CORE_ERROR_TAG("Renderer", "Failed to load brdf_lut compute shader");
                return;
            }

            DescriptorSetLayoutDesc layoutDesc;
            layoutDesc.Bindings = {
                { 0, DescriptorType::StorageImage, ShaderStage::Compute, 1 },
            };
            layoutDesc.DebugName = "BrdfLutDescLayout";
            auto descLayout = m_Device->CreateDescriptorSetLayout(layoutDesc);
            auto descSet = m_Device->AllocateDescriptorSet(descLayout.get());

            ComputePipelineDesc pipeDesc;
            pipeDesc.ComputeShader = compShader.get();
            pipeDesc.DescriptorLayouts = { descLayout.get() };
            pipeDesc.DebugName = "BrdfLutPipeline";
            auto pipeline = m_PipelineCache.GetOrCreateComputePipeline("brdf_lut", pipeDesc);
            if (!pipeline) {
                HVE_CORE_ERROR_TAG("Renderer", "Failed to create BRDF LUT pipeline");
                return;
            }

            DescriptorWrite storageWrite;
            storageWrite.Binding = 0;
            storageWrite.Type = DescriptorType::StorageImage;
            storageWrite.Texture = brdfRHI.get();

            m_Device->UpdateDescriptorSet(descSet.get(), { storageWrite });

            auto* vkBrdf = static_cast<VulkanTexture*>(brdfRHI.get());
            auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline.get());
            auto* vkDescSet = static_cast<VulkanDescriptorSet*>(descSet.get());

            vkDevice->ImmediateSubmit([&](VkCommandBuffer cmd) {
                VulkanTexture::TransitionLayout(cmd, vkBrdf->GetVkImage(),
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->GetVkPipeline());
                VkDescriptorSet vkDS = vkDescSet->GetVkSet();
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    vkPipeline->GetLayout(), 0, 1, &vkDS, 0, nullptr);

                uint32_t groupsX = (brdfSize + 15) / 16;
                uint32_t groupsY = (brdfSize + 15) / 16;
                vkCmdDispatch(cmd, groupsX, groupsY, 1);

                VulkanTexture::TransitionLayout(cmd, vkBrdf->GetVkImage(),
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            });

            vkBrdf->SetCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            m_BrdfRHI = brdfRHI;

            HVE_CORE_INFO_TAG("Renderer", "Generated BRDF LUT ({}x{})", brdfSize, brdfSize);
        }

        m_IBLJustGenerated = true;
        HVE_CORE_INFO_TAG("Renderer", "IBL pipeline complete");
    }

    void Renderer::ResizeViewport(int width, int height)
    {
        if (width <= 0 || height <= 0) return;
        m_Width = static_cast<float>(width);
        m_Height = static_cast<float>(height);

        if (m_CurrentCamera)
            m_CurrentCamera->SetAspectRatio(m_Width / m_Height);

        uint32_t w = static_cast<uint32_t>(m_Width);
        uint32_t h = static_cast<uint32_t>(m_Height);

        m_DepthPrePass.Resize(m_Device, w, h);
        m_ForwardPass.Resize(m_Device, w, h);

        // Recreate ImGui framebuffers for new swapchain images (called from Application resize)
        RecreateImGuiFramebuffers();

        m_SceneImGuiDescriptor = VK_NULL_HANDLE;
    }

    void Renderer::RequestViewportResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) return;
        if (width == static_cast<uint32_t>(m_Width) && height == static_cast<uint32_t>(m_Height)) return;
        m_ViewportResizePending = true;
        m_PendingViewportW = width;
        m_PendingViewportH = height;
    }

    void Renderer::SetViewport(int width, int height)
    {
        // In Vulkan, viewport is set per-command buffer, not globally
    }

    void Renderer::ResetStats()
    {
        m_Stats.vertices_count = 0;
        m_Stats.draw_calls = 0;
        m_Stats.index_count = 0;
    }

    ImTextureID Renderer::GetSceneTextureID()
    {
        // Use tonemapped LDR output if available, else fall back to raw HDR
        RHITexture* viewportTex = m_TonemapOutput ? m_TonemapOutput.get()
                                                   : m_ForwardPass.ColorTexture.get();
        if (!m_CurrentCamera || !viewportTex)
            return nullptr;

        if (!m_SceneImGuiDescriptor)
        {
            auto* vkTex = static_cast<VulkanTexture*>(viewportTex);
            m_SceneImGuiDescriptor = ImGui_ImplVulkan_AddTexture(
                vkTex->GetVkSampler(),
                vkTex->GetVkImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        return m_SceneImGuiDescriptor;
    }

    std::vector<glm::mat4> Renderer::ComputeCascadeLightMatrices(const glm::vec3& lightDir)
    {
        if (!m_CurrentCamera) return {};

        const auto& cascadeLevels = m_Settings.ShadowSettings.ShadowCascadeLevels;
        float cameraNear = m_CurrentCamera->GetNear();
        std::vector<glm::mat4> matrices;

        float lastSplit = cameraNear;
        for (size_t i = 0; i <= cascadeLevels.size(); i++)
        {
            float splitEnd = (i < cascadeLevels.size()) ? cascadeLevels[i] : m_CurrentCamera->GetFar();
            matrices.push_back(ComputeLightSpaceMatrix(m_CurrentCamera, lastSplit, splitEnd, lightDir));
            lastSplit = splitEnd;
        }
        return matrices;
    }

    glm::mat4 Renderer::ComputeLightSpaceMatrix(Camera* camera, float nearPlane, float farPlane, const glm::vec3& lightDir)
    {
        float aspect = camera->GetAspectRatio();
        float fov = camera->GetFOVY();
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        glm::mat4 view = camera->GetView();
        glm::mat4 inv = glm::inverse(proj * view);

        // Frustum corners in world space
        std::vector<glm::vec4> corners;
        for (int x = 0; x < 2; x++)
            for (int y = 0; y < 2; y++)
                for (int z = 0; z < 2; z++)
                {
                    glm::vec4 pt = inv * glm::vec4(
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        2.0f * z - 1.0f,
                        1.0f);
                    corners.push_back(pt / pt.w);
                }

        // Frustum center
        glm::vec3 center(0.0f);
        for (auto& c : corners)
            center += glm::vec3(c);
        center /= static_cast<float>(corners.size());

        glm::mat4 lightView = glm::lookAt(center - glm::normalize(lightDir), center, glm::vec3(0.0f, 1.0f, 0.0f));

        // AABB in light space
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();

        for (auto& c : corners)
        {
            glm::vec4 lc = lightView * c;
            minX = std::min(minX, lc.x); maxX = std::max(maxX, lc.x);
            minY = std::min(minY, lc.y); maxY = std::max(maxY, lc.y);
            minZ = std::min(minZ, lc.z); maxZ = std::max(maxZ, lc.z);
        }

        // Extend Z range to capture shadow casters behind the frustum
        float zMult = 10.0f;
        minZ = (minZ < 0) ? minZ * zMult : minZ / zMult;
        maxZ = (maxZ < 0) ? maxZ / zMult : maxZ * zMult;

        glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
        return lightProj * lightView;
    }
}

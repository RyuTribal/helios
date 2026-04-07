#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/Vulkan/VulkanTexture.h"
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

        HVE_CORE_INFO_TAG("Renderer", "Modular Vulkan renderer initialized ({}x{})",
                          (int)m_Width, (int)m_Height);
    }

    Renderer::~Renderer()
    {
        if (m_Device)
            m_Device->WaitIdle();
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
        m_CurrentCamera = camera;
        if (camera && m_CurrentCamera)
            camera->UpdateCamera();
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
                m_Swapchain->Resize(w, h);
            return;
        }

        auto* vkSwapchain = static_cast<VulkanSwapchain*>(m_Swapchain);
        auto* cmd = m_CommandBuffers[vkSwapchain->GetCurrentFrame()].get();
        auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);

        static uint64_t frameCount = 0;
        if (frameCount < 5 || frameCount % 1000 == 0)
            HVE_CORE_TRACE_TAG("Renderer", "Frame {}: acquired image {}, frame-in-flight {}",
                frameCount, vkSwapchain->GetCurrentImageIndex(), vkSwapchain->GetCurrentFrame());
        frameCount++;

        cmd->Begin();

        // TODO: 3D render passes disabled until GPU hang on Intel Mesa is debugged.
        // The render passes need proper Vulkan validation layer debugging to find
        // the exact image layout transition issue.

        // Use legacy render pass for ImGui (dynamic rendering caused GPU hang on Intel Mesa)
        // Create per-swapchain-image framebuffer on the fly
        // (This is not ideal for perf but ensures correctness)
        VkImageView swapImageView = vkSwapchain->GetCurrentImageView();

        // Get or create the ImGui render pass (created by ImGuiLayer, stored globally)
        VkRenderPass imguiRP = ::GetImGuiRenderPass();

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = imguiRP;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &swapImageView;
        fbInfo.width = vkSwapchain->GetWidth();
        fbInfo.height = vkSwapchain->GetHeight();
        fbInfo.layers = 1;

        VkFramebuffer imguiFB;
        vkCreateFramebuffer(static_cast<VulkanDevice*>(m_Device)->GetDevice(), &fbInfo, nullptr, &imguiFB);

        // Transition swapchain image
        VulkanTexture::TransitionLayout(vkCmd->GetVkCommandBuffer(),
            vkSwapchain->GetCurrentVkImage(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Begin legacy render pass
        VkRenderPassBeginInfo rpBeginInfo{};
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass = imguiRP;
        rpBeginInfo.framebuffer = imguiFB;
        rpBeginInfo.renderArea = {{0, 0}, {vkSwapchain->GetWidth(), vkSwapchain->GetHeight()}};
        VkClearValue clearValue{};
        clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};
        rpBeginInfo.clearValueCount = 1;
        rpBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(vkCmd->GetVkCommandBuffer(), &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Render ImGui
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData)
            ImGui_ImplVulkan_RenderDrawData(drawData, vkCmd->GetVkCommandBuffer());

        vkCmdEndRenderPass(vkCmd->GetVkCommandBuffer());

        // Destroy temporary framebuffer (deferred — we're still recording, but it'll be freed after submit)
        // Actually we need to defer destruction. For now just leak it (TODO: proper cleanup)
        // The render pass transitions the image to PRESENT_SRC_KHR via finalLayout

        cmd->End();

        // Submit with swapchain sync
        VkCommandBuffer vkCmdBuf = vkCmd->GetVkCommandBuffer();
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

        m_SceneImGuiDescriptor = VK_NULL_HANDLE;
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
        if (!m_ForwardPass.ColorTexture)
            return nullptr;

        if (!m_SceneImGuiDescriptor)
        {
            auto* vkTex = static_cast<VulkanTexture*>(m_ForwardPass.ColorTexture.get());
            m_SceneImGuiDescriptor = ImGui_ImplVulkan_AddTexture(
                vkTex->GetVkSampler(),
                vkTex->GetVkImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        return m_SceneImGuiDescriptor;
    }

    std::vector<glm::mat4> Renderer::ComputeCascadeLightMatrices(const glm::vec3& lightDir)
    {
        // TODO: implement cascade shadow map matrix computation
        return {};
    }

    glm::mat4 Renderer::ComputeLightSpaceMatrix(Camera* camera, float nearPlane, float farPlane, const glm::vec3& lightDir)
    {
        // TODO: implement
        return glm::mat4(1.0f);
    }
}

#include "pch.h"
#include "SkyboxRenderer.h"
#include "Camera.h"

namespace Engine {

    // Unit cube vertices (position only)
    static float s_CubeVertices[] = {
        // Back face
        -1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        // Front face
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // Left face
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        // Right face
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        // Bottom face
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        // Top face
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
    };

    struct SkyboxUBO {
        glm::mat4 View;
        glm::mat4 Projection;
        float Brightness;
        float _pad[3];
    };

    void SkyboxRenderer::Init(RHIDevice* device, PipelineCache* cache, RHIRenderPass* renderPass, uint32_t width, uint32_t height)
    {
        m_Device = device;

        // Cube VBO
        BufferDesc vboDesc;
        vboDesc.Size = sizeof(s_CubeVertices);
        vboDesc.Usage = BufferUsage::Vertex | BufferUsage::Transfer;
        vboDesc.Access = MemoryAccess::GPU_Only;
        vboDesc.DebugName = "SkyboxCubeVBO";
        m_CubeVBO = device->CreateBuffer(vboDesc, s_CubeVertices);

        // Camera UBO
        BufferDesc uboDesc;
        uboDesc.Size = sizeof(SkyboxUBO);
        uboDesc.Usage = BufferUsage::Uniform;
        uboDesc.Access = MemoryAccess::CPU_to_GPU;
        uboDesc.DebugName = "SkyboxCameraUBO";
        m_CameraUBO = device->CreateBuffer(uboDesc);

        // Descriptor layout: binding 0 = UBO, binding 1 = cubemap sampler
        DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, 1 },
            { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1 }
        };
        layoutDesc.DebugName = "SkyboxDescLayout";
        m_DescLayout = device->CreateDescriptorSetLayout(layoutDesc);
        m_DescSet = device->AllocateDescriptorSet(m_DescLayout.get());

        // Update UBO binding
        DescriptorWrite write;
        write.Binding = 0;
        write.Type = DescriptorType::UniformBuffer;
        write.Buffer = m_CameraUBO.get();
        write.Range = sizeof(SkyboxUBO);
        device->UpdateDescriptorSet(m_DescSet.get(), { write });

        // Load shaders and create pipeline
        auto vertShader = cache->LoadShader("Resources/Shaders/skybox.vert.spv", ShaderStage::Vertex);
        auto fragShader = cache->LoadShader("Resources/Shaders/skybox.frag.spv", ShaderStage::Fragment);

        if (vertShader && fragShader)
        {
            VertexLayout skyboxLayout;
            skyboxLayout.Stride = sizeof(float) * 3;
            skyboxLayout.Attributes = {
                { 0, 0, 0, ImageFormat::RGB32F }
            };

            GraphicsPipelineDesc pipeDesc;
            pipeDesc.VertexShader = vertShader.get();
            pipeDesc.FragmentShader = fragShader.get();
            pipeDesc.Layout = skyboxLayout;
            pipeDesc.RenderPass = renderPass;
            pipeDesc.DescriptorLayouts = { m_DescLayout.get() };
            pipeDesc.State.DepthWrite = false;
            pipeDesc.State.DepthTest = true;
            pipeDesc.State.Depth = DepthCompare::LessEqual;
            pipeDesc.State.Cull = CullMode::None;
            pipeDesc.DebugName = "SkyboxPipeline";

            m_Pipeline = cache->GetOrCreateGraphicsPipeline("skybox", pipeDesc);
        }
    }

    void SkyboxRenderer::Execute(RHICommandBuffer* cmd, RHIFramebuffer* framebuffer, Camera* camera, const SkyboxRenderData& settings)
    {
        if (!m_Pipeline || !camera || !settings.CubeTexture)
            return;

        // Upload camera data (strip translation from view matrix)
        SkyboxUBO ubo;
        ubo.View = glm::mat4(glm::mat3(camera->GetView()));
        ubo.Projection = camera->GetProjection();
        ubo.Brightness = settings.Brightness;
        m_CameraUBO->SetData(&ubo, sizeof(SkyboxUBO));

        // Update cubemap texture binding
        DescriptorWrite texWrite;
        texWrite.Binding = 1;
        texWrite.Type = DescriptorType::CombinedImageSampler;
        texWrite.Texture = settings.CubeTexture.get();
        m_Device->UpdateDescriptorSet(m_DescSet.get(), { texWrite });

        // Draw
        cmd->BindPipeline(m_Pipeline.get());
        cmd->SetViewport(0, 0, static_cast<float>(framebuffer->GetWidth()), static_cast<float>(framebuffer->GetHeight()));
        cmd->SetScissor(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());
        cmd->BindVertexBuffer(m_CubeVBO.get());
        cmd->BindDescriptorSet(0, m_DescSet.get());
        cmd->Draw(36); // 6 faces * 2 triangles * 3 vertices
    }

    void SkyboxRenderer::Resize(uint32_t width, uint32_t height)
    {
        // Nothing to resize for skybox - it uses the forward pass framebuffer
    }

}

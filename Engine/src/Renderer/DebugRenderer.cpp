#include "pch.h"
#include "DebugRenderer.h"
#include "Camera.h"

namespace Engine {

    struct LineVertex {
        glm::vec3 Position;
        glm::vec4 Color;
    };

    void DebugRenderer::Init(RHIDevice* device, PipelineCache* cache, RHIRenderPass* renderPass, uint32_t width, uint32_t height)
    {
        m_Device = device;

        // Line vertex buffer (dynamic, re-uploaded each frame)
        BufferDesc vboDesc;
        vboDesc.Size = m_MaxLines * 2 * sizeof(LineVertex);
        vboDesc.Usage = BufferUsage::Vertex | BufferUsage::Transfer;
        vboDesc.Access = MemoryAccess::CPU_to_GPU;
        vboDesc.DebugName = "DebugLineVBO";
        m_LineVBO = device->CreateBuffer(vboDesc);

        // Camera UBO for debug lines
        BufferDesc uboDesc;
        uboDesc.Size = sizeof(glm::mat4) * 2; // view + projection
        uboDesc.Usage = BufferUsage::Uniform;
        uboDesc.Access = MemoryAccess::CPU_to_GPU;
        uboDesc.DebugName = "DebugCameraUBO";
        m_CameraUBO = device->CreateBuffer(uboDesc);

        // Descriptor set layout for camera UBO
        DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex, 1 }
        };
        layoutDesc.DebugName = "DebugLineDescLayout";
        m_DescLayout = device->CreateDescriptorSetLayout(layoutDesc);
        m_DescSet = device->AllocateDescriptorSet(m_DescLayout.get());

        // Update descriptor with camera UBO
        DescriptorWrite write;
        write.Binding = 0;
        write.Type = DescriptorType::UniformBuffer;
        write.Buffer = m_CameraUBO.get();
        write.Range = sizeof(glm::mat4) * 2;
        device->UpdateDescriptorSet(m_DescSet.get(), { write });

        // Load shaders and create pipeline
        auto vertShader = cache->LoadShader("Resources/Shaders/line.vert.spv", ShaderStage::Vertex);
        auto fragShader = cache->LoadShader("Resources/Shaders/line.frag.spv", ShaderStage::Fragment);

        if (vertShader && fragShader)
        {
            VertexLayout lineLayout;
            lineLayout.Stride = sizeof(LineVertex);
            lineLayout.Attributes = {
                { 0, 0, 0,                         ImageFormat::RGB32F },  // position
                { 1, 0, sizeof(glm::vec3),          ImageFormat::RGBA32F }, // color
            };

            GraphicsPipelineDesc pipeDesc;
            pipeDesc.VertexShader = vertShader.get();
            pipeDesc.FragmentShader = fragShader.get();
            pipeDesc.Layout = lineLayout;
            pipeDesc.RenderPass = renderPass;
            pipeDesc.DescriptorLayouts = { m_DescLayout.get() };
            pipeDesc.State.DepthWrite = false;
            pipeDesc.State.DepthTest = true;
            pipeDesc.State.Depth = DepthCompare::LessEqual;
            pipeDesc.State.Cull = CullMode::None;
            pipeDesc.DebugName = "DebugLinePipeline";

            m_LinePipeline = cache->GetOrCreateGraphicsPipeline("debug_line", pipeDesc);
        }
    }

    void DebugRenderer::Execute(RHICommandBuffer* cmd, RHIFramebuffer* framebuffer,
                                 const std::vector<Line>& lines,
                                 const std::vector<DebugBox>& boxes,
                                 const std::vector<DebugSphere>& spheres,
                                 const std::vector<DebugCapsule>& capsules,
                                 Camera* camera)
    {
        if (!m_LinePipeline || !camera)
            return;

        // Collect all lines from debug primitives
        std::vector<Line> allLines = lines;
        ExpandBoxesToLines(boxes, allLines);
        ExpandSpheresToLines(spheres, allLines);
        ExpandCapsulesToLines(capsules, allLines);

        if (allLines.empty())
            return;

        // Upload camera data
        struct CameraData {
            glm::mat4 View;
            glm::mat4 Projection;
        };
        CameraData camData;
        camData.View = camera->GetView();
        camData.Projection = camera->GetProjection();
        m_CameraUBO->SetData(&camData, sizeof(CameraData));

        // Build line vertex data
        uint32_t lineCount = static_cast<uint32_t>(std::min(allLines.size(), (size_t)m_MaxLines));
        std::vector<LineVertex> vertices(lineCount * 2);
        for (uint32_t i = 0; i < lineCount; i++)
        {
            vertices[i * 2 + 0] = { allLines[i].Start, allLines[i].Color };
            vertices[i * 2 + 1] = { allLines[i].End, allLines[i].Color };
        }
        m_LineVBO->SetData(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(LineVertex)));

        // Draw
        cmd->BindPipeline(m_LinePipeline.get());
        cmd->SetViewport(0, 0, static_cast<float>(framebuffer->GetWidth()), static_cast<float>(framebuffer->GetHeight()));
        cmd->SetScissor(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());
        cmd->BindVertexBuffer(m_LineVBO.get());
        cmd->BindDescriptorSet(0, m_DescSet.get());
        cmd->Draw(lineCount * 2);
    }

    void DebugRenderer::ExpandBoxesToLines(const std::vector<DebugBox>& boxes, std::vector<Line>& outLines)
    {
        for (auto& box : boxes)
        {
            glm::vec3 verts[] = {
                glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, -1.0f, -1.0f),
                glm::vec3(-1.0f, 1.0f, -1.0f),  glm::vec3(1.0f, 1.0f, -1.0f),
                glm::vec3(-1.0f, -1.0f, 1.0f),  glm::vec3(1.0f, -1.0f, 1.0f),
                glm::vec3(-1.0f, 1.0f, 1.0f),   glm::vec3(1.0f, 1.0f, 1.0f)
            };

            glm::mat4 scaledTransform = glm::scale(box.Transform, box.Size);
            for (auto& v : verts)
                v = glm::vec3(scaledTransform * glm::vec4(v, 1.f));

            int edges[][2] = {
                {0, 1}, {1, 3}, {3, 2}, {2, 0},
                {4, 5}, {5, 7}, {7, 6}, {6, 4},
                {0, 4}, {1, 5}, {2, 6}, {3, 7}
            };

            for (auto& edge : edges)
                outLines.push_back(Line{ verts[edge[0]], verts[edge[1]], box.Color, glm::mat4(1.0f) });
        }
    }

    void DebugRenderer::ExpandSpheresToLines(const std::vector<DebugSphere>& spheres, std::vector<Line>& outLines)
    {
        const int latDiv = 12;
        const int lonDiv = 12;

        for (auto& sphere : spheres)
        {
            std::vector<glm::vec3> vertices;
            for (int i = 0; i <= latDiv; ++i)
            {
                float lat = glm::pi<float>() * i / latDiv;
                for (int j = 0; j <= lonDiv; ++j)
                {
                    float lon = 2.0f * glm::pi<float>() * j / lonDiv;
                    float x = sphere.Radius * sin(lat) * cos(lon);
                    float y = sphere.Radius * sin(lat) * sin(lon);
                    float z = sphere.Radius * cos(lat);
                    glm::vec3 pos = glm::vec3(sphere.Transform * glm::vec4(x, y, z, 1.0f));
                    vertices.push_back(pos);
                }
            }

            for (int i = 0; i < latDiv; ++i)
            {
                for (int j = 0; j < lonDiv; ++j)
                {
                    outLines.push_back(Line{ vertices[i * (lonDiv + 1) + j], vertices[i * (lonDiv + 1) + j + 1], sphere.Color, glm::mat4(1.0f) });
                    outLines.push_back(Line{ vertices[i * (lonDiv + 1) + j], vertices[(i + 1) * (lonDiv + 1) + j], sphere.Color, glm::mat4(1.0f) });
                }
            }
        }
    }

    void DebugRenderer::ExpandCapsulesToLines(const std::vector<DebugCapsule>& capsules, std::vector<Line>& outLines)
    {
        const int latDiv = 12;
        const int lonDiv = 12;

        for (auto& capsule : capsules)
        {
            std::vector<glm::vec3> vertices;

            // Top cap
            for (int i = 0; i <= latDiv / 2; ++i)
            {
                float lat = glm::pi<float>() * i / latDiv;
                for (int j = 0; j <= lonDiv; ++j)
                {
                    float lon = 2.0f * glm::pi<float>() * j / lonDiv;
                    float x = capsule.Radius * sin(lat) * cos(lon);
                    float y = capsule.Radius * cos(lat) + capsule.HalfHeight;
                    float z = capsule.Radius * sin(lat) * sin(lon);
                    vertices.push_back(glm::vec3(capsule.Transform * glm::vec4(x, y, z, 1.0f)));
                }
            }

            // Bottom cap
            for (int i = latDiv / 2; i <= latDiv; ++i)
            {
                float lat = glm::pi<float>() * i / latDiv;
                for (int j = 0; j <= lonDiv; ++j)
                {
                    float lon = 2.0f * glm::pi<float>() * j / lonDiv;
                    float x = capsule.Radius * sin(lat) * cos(lon);
                    float y = capsule.Radius * cos(lat) - capsule.HalfHeight;
                    float z = capsule.Radius * sin(lat) * sin(lon);
                    vertices.push_back(glm::vec3(capsule.Transform * glm::vec4(x, y, z, 1.0f)));
                }
            }

            for (int i = 0; i < latDiv; ++i)
            {
                for (int j = 0; j < lonDiv; ++j)
                {
                    int index1 = i * (lonDiv + 1) + j;
                    int index2 = index1 + 1;
                    int index3 = index1 + (lonDiv + 1);

                    if (index3 < (int)vertices.size() && index2 < (int)vertices.size())
                    {
                        outLines.push_back(Line{ vertices[index1], vertices[index2], capsule.Color, glm::mat4(1.0f) });
                        outLines.push_back(Line{ vertices[index1], vertices[index3], capsule.Color, glm::mat4(1.0f) });
                    }
                }
            }
        }
    }

}

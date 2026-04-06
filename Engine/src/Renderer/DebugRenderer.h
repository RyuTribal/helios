#pragma once
#include "RHI/RHI.h"
#include "PipelineCache.h"

namespace Engine {

    class Camera;

    struct Line {
        glm::vec3 Start;
        glm::vec3 End;
        glm::vec4 Color;
        glm::mat4 Transform;
    };

    struct DebugBox {
        glm::vec3 Size;
        glm::mat4 Transform;
        glm::vec4 Color;
    };

    struct DebugSphere {
        float Radius;
        glm::mat4 Transform;
        glm::vec4 Color;
    };

    struct DebugCapsule {
        float Radius;
        float HalfHeight;
        glm::mat4 Transform;
        glm::vec4 Color;
    };

    class DebugRenderer {
    public:
        DebugRenderer() = default;

        void Init(RHIDevice* device, PipelineCache* cache, RHIRenderPass* renderPass, uint32_t width, uint32_t height);
        void Execute(RHICommandBuffer* cmd, RHIFramebuffer* framebuffer,
                     const std::vector<Line>& lines,
                     const std::vector<DebugBox>& boxes,
                     const std::vector<DebugSphere>& spheres,
                     const std::vector<DebugCapsule>& capsules,
                     Camera* camera);

    private:
        void ExpandBoxesToLines(const std::vector<DebugBox>& boxes, std::vector<Line>& outLines);
        void ExpandSpheresToLines(const std::vector<DebugSphere>& spheres, std::vector<Line>& outLines);
        void ExpandCapsulesToLines(const std::vector<DebugCapsule>& capsules, std::vector<Line>& outLines);

        RHIDevice* m_Device = nullptr;
        Ref<RHIPipeline> m_LinePipeline;
        Ref<RHIBuffer> m_LineVBO;
        Ref<RHIDescriptorSetLayout> m_DescLayout;
        Ref<RHIDescriptorSet> m_DescSet;
        Ref<RHIBuffer> m_CameraUBO;
        uint32_t m_MaxLines = 65536;
    };

}

#pragma once
#include "RHI/RHI.h"

namespace Engine {

    class PipelineCache {
    public:
        PipelineCache() = default;
        explicit PipelineCache(RHIDevice* device);

        Ref<RHIPipeline> GetOrCreateGraphicsPipeline(const std::string& name, const GraphicsPipelineDesc& desc);
        Ref<RHIPipeline> GetOrCreateComputePipeline(const std::string& name, const ComputePipelineDesc& desc);

        Ref<RHIShader> LoadShader(const std::string& path, ShaderStage stage);

        RHIShader* GetShader(const std::string& path) const;
        RHIPipeline* GetGraphicsPipeline(const std::string& name) const;
        RHIPipeline* GetComputePipeline(const std::string& name) const;

    private:
        RHIDevice* m_Device = nullptr;
        std::unordered_map<std::string, Ref<RHIShader>> m_Shaders;
        std::unordered_map<std::string, Ref<RHIPipeline>> m_GraphicsPipelines;
        std::unordered_map<std::string, Ref<RHIPipeline>> m_ComputePipelines;
    };

}

#include "pch.h"
#include "PipelineCache.h"

namespace Engine {

    PipelineCache::PipelineCache(RHIDevice* device)
        : m_Device(device)
    {
    }

    Ref<RHIPipeline> PipelineCache::GetOrCreateGraphicsPipeline(const std::string& name, const GraphicsPipelineDesc& desc)
    {
        auto it = m_GraphicsPipelines.find(name);
        if (it != m_GraphicsPipelines.end())
            return it->second;

        auto pipeline = m_Device->CreateGraphicsPipeline(desc);
        if (!pipeline)
        {
            HVE_CORE_ERROR_TAG("PipelineCache", "Failed to create graphics pipeline: {}", name);
            return nullptr;
        }
        m_GraphicsPipelines[name] = pipeline;
        HVE_CORE_INFO_TAG("PipelineCache", "Created graphics pipeline: {}", name);
        return pipeline;
    }

    Ref<RHIPipeline> PipelineCache::GetOrCreateComputePipeline(const std::string& name, const ComputePipelineDesc& desc)
    {
        auto it = m_ComputePipelines.find(name);
        if (it != m_ComputePipelines.end())
            return it->second;

        auto pipeline = m_Device->CreateComputePipeline(desc);
        if (!pipeline)
        {
            HVE_CORE_ERROR_TAG("PipelineCache", "Failed to create compute pipeline: {}", name);
            return nullptr;
        }
        m_ComputePipelines[name] = pipeline;
        HVE_CORE_INFO_TAG("PipelineCache", "Created compute pipeline: {}", name);
        return pipeline;
    }

    Ref<RHIShader> PipelineCache::LoadShader(const std::string& path, ShaderStage stage)
    {
        auto it = m_Shaders.find(path);
        if (it != m_Shaders.end())
            return it->second;

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            HVE_CORE_ERROR_TAG("PipelineCache", "Failed to open SPIR-V file: {}", path);
            return nullptr;
        }

        auto fileSize = file.tellg();
        file.seekg(0);
        std::vector<uint8_t> spirvCode(fileSize);
        file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize);

        ShaderDesc desc;
        desc.Stage = stage;
        desc.SpirVCode = std::move(spirvCode);
        desc.EntryPoint = "main";
        desc.DebugName = path;

        auto shader = m_Device->CreateShader(desc);
        if (!shader)
        {
            HVE_CORE_ERROR_TAG("PipelineCache", "Failed to create shader from: {}", path);
            return nullptr;
        }

        m_Shaders[path] = shader;
        HVE_CORE_INFO_TAG("PipelineCache", "Loaded shader: {}", path);
        return shader;
    }

    RHIShader* PipelineCache::GetShader(const std::string& path) const
    {
        auto it = m_Shaders.find(path);
        return (it != m_Shaders.end()) ? it->second.get() : nullptr;
    }

    RHIPipeline* PipelineCache::GetGraphicsPipeline(const std::string& name) const
    {
        auto it = m_GraphicsPipelines.find(name);
        return (it != m_GraphicsPipelines.end()) ? it->second.get() : nullptr;
    }

    RHIPipeline* PipelineCache::GetComputePipeline(const std::string& name) const
    {
        auto it = m_ComputePipelines.find(name);
        return (it != m_ComputePipelines.end()) ? it->second.get() : nullptr;
    }

}

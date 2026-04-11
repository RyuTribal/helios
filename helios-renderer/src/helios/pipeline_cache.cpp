#include "helios/pipeline_cache.h"
#include "helios/rhi/rhi_device.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <fstream>

namespace helios::rhi {

PipelineCache::PipelineCache(Device& device)
    : m_device(&device)
{
}

Shader* PipelineCache::load_shader(
    const std::string& name,
    const std::filesystem::path& spirv_path,
    ShaderStage stage)
{
    // Return cached if exists
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second.get();
    }

    // Read SPIR-V file
    std::ifstream file(spirv_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        HELIOS_LOG_ERROR(Renderer, "[PipelineCache] Failed to open SPIR-V file: {}",
                          spirv_path.string());
        return nullptr;
    }

    auto file_size = file.tellg();
    std::vector<uint8_t> spirv_code(static_cast<size_t>(file_size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirv_code.data()), file_size);

    ShaderDesc desc;
    desc.stage       = stage;
    desc.spirv_code  = std::move(spirv_code);
    desc.entry_point = "main";
    desc.debug_name  = name;

    auto shader = m_device->create_shader(desc);
    if (!shader) {
        HELIOS_LOG_ERROR(Renderer, "[PipelineCache] Failed to create shader '{}'", name);
        return nullptr;
    }

    auto [inserted_it, success] = m_shaders.emplace(name, std::move(shader));
    if (!success) {
        HELIOS_LOG_ERROR(Renderer, "[PipelineCache] Failed to cache shader '{}'", name);
        return nullptr;
    }

    return inserted_it->second.get();
}

Pipeline* PipelineCache::get_or_create_graphics_pipeline(
    const std::string& name,
    const GraphicsPipelineDesc& desc)
{
    auto it = m_pipelines.find(name);
    if (it != m_pipelines.end()) {
        return it->second.get();
    }

    auto pipeline = m_device->create_graphics_pipeline(desc);
    if (!pipeline) {
        HELIOS_LOG_ERROR(Renderer, "[PipelineCache] Failed to create graphics pipeline '{}'", name);
        return nullptr;
    }

    auto [inserted_it, success] = m_pipelines.emplace(name, std::move(pipeline));
    if (!success) {
        HELIOS_LOG_ERROR(Renderer, "[PipelineCache] Failed to cache graphics pipeline '{}'", name);
        return nullptr;
    }

    return inserted_it->second.get();
}

Pipeline* PipelineCache::get_or_create_compute_pipeline(
    const std::string& name,
    const ComputePipelineDesc& desc)
{
    auto it = m_pipelines.find(name);
    if (it != m_pipelines.end()) {
        return it->second.get();
    }

    auto pipeline = m_device->create_compute_pipeline(desc);
    if (!pipeline) {
        HELIOS_LOG_ERROR(Renderer, "[PipelineCache] Failed to create compute pipeline '{}'", name);
        return nullptr;
    }

    auto [inserted_it, success] = m_pipelines.emplace(name, std::move(pipeline));
    if (!success) {
        HELIOS_LOG_ERROR(Renderer, "[PipelineCache] Failed to cache compute pipeline '{}'", name);
        return nullptr;
    }

    return inserted_it->second.get();
}

Shader* PipelineCache::get_shader(const std::string& name)
{
    auto it = m_shaders.find(name);
    return it != m_shaders.end() ? it->second.get() : nullptr;
}

Pipeline* PipelineCache::get_pipeline(const std::string& name)
{
    auto it = m_pipelines.find(name);
    return it != m_pipelines.end() ? it->second.get() : nullptr;
}

void PipelineCache::clear()
{
    m_pipelines.clear();
    m_shaders.clear();
}

} // namespace helios::rhi

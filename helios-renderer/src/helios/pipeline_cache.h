#pragma once

#include "helios/rhi/rhi_types.h"
#include "helios/rhi/rhi_shader.h"
#include "helios/rhi/rhi_pipeline.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

namespace helios::rhi {
class Device;
}

namespace helios::rhi {

// Caches compiled pipelines and loaded shaders by name.
// Avoids redundant GPU resource creation for the same pipeline/shader.
class PipelineCache {
public:
    explicit PipelineCache(Device& device);
    ~PipelineCache() = default;

    PipelineCache(PipelineCache&&) noexcept = default;
    PipelineCache& operator=(PipelineCache&&) noexcept = default;
    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;

    // Load a SPIR-V shader from disk. Caches by name.
    // Returns pointer to cached shader (non-owning, valid for cache lifetime).
    Shader* load_shader(const std::string& name,
                        const std::filesystem::path& spirv_path,
                        ShaderStage stage);

    // Get or create a graphics pipeline. Caches by name.
    Pipeline* get_or_create_graphics_pipeline(
        const std::string& name,
        const GraphicsPipelineDesc& desc);

    // Get or create a compute pipeline. Caches by name.
    Pipeline* get_or_create_compute_pipeline(
        const std::string& name,
        const ComputePipelineDesc& desc);

    // Retrieve cached resources (returns nullptr if not found)
    Shader* get_shader(const std::string& name);
    Pipeline* get_pipeline(const std::string& name);

    // Clear all cached resources (GPU objects destroyed via RAII)
    void clear();

private:
    Device* m_device;
    std::unordered_map<std::string, std::unique_ptr<Shader>> m_shaders;
    std::unordered_map<std::string, std::unique_ptr<Pipeline>> m_pipelines;
};

} // namespace helios::rhi

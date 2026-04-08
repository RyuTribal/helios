// helios-renderer/src/helios/forward_plus/skybox_state.h
//
// Skybox rendering state: shaders, pipeline, descriptor set, cube VBO, cubemap.
// Split from the old SimpleRenderState god-struct.
#pragma once

#include "helios/rhi/rhi.h"
#include <memory>

namespace helios {

struct SkyboxState {
    std::unique_ptr<rhi::Shader> vert_shader;
    std::unique_ptr<rhi::Shader> frag_shader;
    std::unique_ptr<rhi::Pipeline> pipeline;
    std::unique_ptr<rhi::DescriptorSetLayout> layout;
    std::unique_ptr<rhi::DescriptorSet> ds;
    std::unique_ptr<rhi::Buffer> ubo;
    std::unique_ptr<rhi::Buffer> cube_vbo;
    std::unique_ptr<rhi::Texture> env_cubemap;
    uint32_t vertex_count = 36;

    bool valid = false;
};

} // namespace helios

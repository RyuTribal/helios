// helios-renderer/src/helios/forward_plus/simple_render_state.h
//
// Minimal render state for MVP rendering: flat-shaded cubes using the
// swapchain's dynamic rendering path.  This is the "get something on screen"
// path that will eventually be replaced by the full Forward+ pipeline once
// the pass execute bodies and pipeline_init are implemented.
#pragma once

#include "helios/rhi/rhi.h"
#include <memory>

namespace helios {

struct SimpleRenderState {
    // GPU resources (owned)
    std::unique_ptr<rhi::Shader>              vert_shader;
    std::unique_ptr<rhi::Shader>              frag_shader;
    std::unique_ptr<rhi::Pipeline>            pipeline;
    std::unique_ptr<rhi::DescriptorSetLayout> camera_layout;
    std::unique_ptr<rhi::DescriptorSet>       camera_ds;
    std::unique_ptr<rhi::Buffer>              camera_ubo;
    std::unique_ptr<rhi::Buffer>              cube_vbo;
    std::unique_ptr<rhi::Buffer>              cube_ibo;
    uint32_t cube_index_count = 0;

    bool valid = false;
};

/// Create the SimpleRenderState from a device + shader directory.
/// Returns a ready-to-use state, or one with valid==false on failure.
SimpleRenderState create_simple_render_state(
    rhi::Device& device,
    const char* shader_dir);

} // namespace helios

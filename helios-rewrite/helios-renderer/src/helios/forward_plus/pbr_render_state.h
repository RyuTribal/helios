// helios-renderer/src/helios/forward_plus/pbr_render_state.h
//
// PBR rendering state: shaders, pipeline, descriptor sets, textures, mesh buffers.
// Split from the old SimpleRenderState god-struct.
#pragma once

#include "helios/rhi/rhi.h"
#include <memory>

namespace helios {

struct PBRRenderState {
    std::unique_ptr<rhi::Shader> vert_shader;
    std::unique_ptr<rhi::Shader> frag_shader;
    std::unique_ptr<rhi::Pipeline> pipeline;
    std::unique_ptr<rhi::DescriptorSetLayout> camera_layout;   // set 0: CameraUBO
    std::unique_ptr<rhi::DescriptorSetLayout> material_layout;  // set 1: textures
    std::unique_ptr<rhi::DescriptorSet> camera_ds;
    std::unique_ptr<rhi::DescriptorSet> material_ds;
    std::unique_ptr<rhi::Buffer> camera_ubo;

    // Mesh
    std::unique_ptr<rhi::Buffer> mesh_vbo;
    std::unique_ptr<rhi::Buffer> mesh_ibo;
    uint32_t index_count = 0;

    // PBR textures
    std::unique_ptr<rhi::Texture> albedo_tex;
    std::unique_ptr<rhi::Texture> normal_tex;
    std::unique_ptr<rhi::Texture> metallic_roughness_tex;
    std::unique_ptr<rhi::Texture> emissive_tex;

    bool valid = false;
};

} // namespace helios

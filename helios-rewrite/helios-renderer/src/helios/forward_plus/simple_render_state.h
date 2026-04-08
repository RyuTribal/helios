// helios-renderer/src/helios/forward_plus/simple_render_state.h
//
// Render state for the PBR sandbox demo: DamagedHelmet with PBR shading
// and an HDR skybox. Also retains the old flat-color cube path as fallback.
#pragma once

#include "helios/rhi/rhi.h"
#include <memory>

namespace helios {

struct SimpleRenderState {
    // PBR mesh rendering
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

    // Skybox
    std::unique_ptr<rhi::Shader> skybox_vert;
    std::unique_ptr<rhi::Shader> skybox_frag;
    std::unique_ptr<rhi::Pipeline> skybox_pipeline;
    std::unique_ptr<rhi::DescriptorSetLayout> skybox_layout;
    std::unique_ptr<rhi::DescriptorSet> skybox_ds;
    std::unique_ptr<rhi::Buffer> skybox_ubo;
    std::unique_ptr<rhi::Buffer> skybox_cube_vbo;
    std::unique_ptr<rhi::Texture> env_cubemap;

    // Depth buffer
    std::unique_ptr<rhi::Texture> depth_texture;

    bool valid = false;
    bool has_skybox = false;
};

} // namespace helios

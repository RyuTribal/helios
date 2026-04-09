// helios-renderer/src/helios/forward_plus/pbr_render_state.h
//
// PBR rendering state: shaders, pipeline, descriptor set layouts, camera UBO.
// Shared rendering infrastructure -- per-mesh/per-material data lives in
// GPUResourceCache.
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

    // Per-camera UBOs and descriptor sets (one per active camera view).
    // Grown on demand by the draw system.
    struct CameraSlot {
        std::unique_ptr<rhi::Buffer> ubo;
        std::unique_ptr<rhi::DescriptorSet> ds;
    };
    std::vector<CameraSlot> camera_slots;

    // Legacy single-camera (kept for backward compat, used when camera_slots is empty)
    std::unique_ptr<rhi::DescriptorSet> camera_ds;
    std::unique_ptr<rhi::Buffer> camera_ubo;

    // Legacy per-mesh/per-material fields (kept for backwards compat during transition).
    // New code uses GPUResourceCache instead.
    std::unique_ptr<rhi::DescriptorSet> material_ds;
    std::unique_ptr<rhi::Buffer> mesh_vbo;
    std::unique_ptr<rhi::Buffer> mesh_ibo;
    uint32_t index_count = 0;
    std::unique_ptr<rhi::Texture> albedo_tex;
    std::unique_ptr<rhi::Texture> normal_tex;
    std::unique_ptr<rhi::Texture> metallic_roughness_tex;
    std::unique_ptr<rhi::Texture> emissive_tex;

    bool valid = false;
};

} // namespace helios

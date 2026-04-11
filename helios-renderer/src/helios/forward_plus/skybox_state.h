#pragma once

#include "helios/rhi/rhi.h"
#include "helios/assets/handle.h"
#include "helios/assets/cubemap_asset.h"
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
    rhi::Texture* env_cubemap = nullptr;  // Non-owning, owned by GPUResourceCache
    Handle<CubeMapAsset> env_cubemap_handle; // Keeps refcount alive so GC doesn't collect
    uint32_t vertex_count = 36;

    bool valid = false;

    // Per-camera UBO/DS slots for multi-camera rendering.
    // Without these, the single shared UBO gets overwritten by the last
    // camera before the GPU executes any draws.
    struct CameraSlot {
        std::unique_ptr<rhi::Buffer> ubo;
        std::unique_ptr<rhi::DescriptorSet> ds;
    };
    std::vector<CameraSlot> camera_slots;
};

} // namespace helios

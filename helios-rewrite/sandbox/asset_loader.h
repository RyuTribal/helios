#pragma once

#include <helios/rhi/rhi.h>
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>

namespace sandbox {

struct PBRVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent;  // w = handedness
};

struct LoadedMesh {
    std::unique_ptr<helios::rhi::Buffer> vbo;
    std::unique_ptr<helios::rhi::Buffer> ibo;
    uint32_t index_count = 0;
};

// Load a glTF mesh, returns vertex buffer + index buffer + index count
LoadedMesh load_gltf_mesh(helios::rhi::Device& device, const char* gltf_path);

// Load a 2D texture from file (JPEG/PNG), returns RGBA8 texture
std::unique_ptr<helios::rhi::Texture> load_texture_2d(
    helios::rhi::Device& device, const char* path, const char* debug_name);

// Load an HDR equirectangular image, returns RGBA32F texture
std::unique_ptr<helios::rhi::Texture> load_hdr_texture(
    helios::rhi::Device& device, const char* path, const char* debug_name);

// Convert equirectangular HDR to cubemap via compute shader
std::unique_ptr<helios::rhi::Texture> convert_equirect_to_cubemap(
    helios::rhi::Device& device, helios::rhi::CommandBuffer& cmd,
    const helios::rhi::Texture& equirect, uint32_t cube_size);

} // namespace sandbox

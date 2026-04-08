#pragma once

#include <helios/rhi/rhi.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <memory>
#include <cstdint>
#include <vector>

namespace sandbox {

// Load an HDR equirectangular image, returns RGBA32F texture
std::unique_ptr<helios::rhi::Texture> load_hdr_texture(
    helios::rhi::Device& device, const char* path, const char* debug_name);

// Convert equirectangular HDR to cubemap via compute shader
std::unique_ptr<helios::rhi::Texture> convert_equirect_to_cubemap(
    helios::rhi::Device& device, helios::rhi::CommandBuffer& cmd,
    const helios::rhi::Texture& equirect, uint32_t cube_size);

// Read a SPIR-V file from disk
std::vector<uint8_t> read_spirv(const std::filesystem::path& path);

// Build a unit skybox cube (36 vertices, position only)
std::vector<glm::vec3> build_skybox_cube();

} // namespace sandbox

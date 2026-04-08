#pragma once

#include <any>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace helios {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 texcoord{0.0f};
    glm::vec4 tangent{0.0f};   // xyz = tangent, w = bitangent sign
};

struct SubMesh {
    uint32_t index_offset = 0;
    uint32_t index_count = 0;
    uint32_t vertex_offset = 0;
    uint32_t vertex_count = 0;
    int material_index = -1;     // index into MeshData::materials
};

struct MaterialData {
    std::string name;
    glm::vec3 base_color{1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;

    // Relative paths to texture files (empty if not present).
    std::string albedo_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
    std::string ao_texture;
    std::string emissive_texture;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> submeshes;
    std::vector<MaterialData> materials;
    std::string source_path;

    bool is_valid() const { return !vertices.empty() && !indices.empty(); }
};

class MeshImporter {
public:
    // Load GLTF/GLB mesh file. Returns MeshData in std::any.
    // Throws std::runtime_error on failure.
    static std::any import(const std::filesystem::path& path);
};

} // namespace helios

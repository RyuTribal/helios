#include "primitive_meshes.h"

#include <helios/assets/asset_binary.h>
#include <helios/assets/binary_writer.h>
#include <helios/assets/mesh_asset.h>
#include <helios/core/uuid.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace helios::editor {

// ---- Internal mesh data for generation ----

struct PrimitiveMesh {
    std::vector<helios::PBRVertex> vertices;
    std::vector<uint32_t> indices;
};

// Write an optimized .hvemesh Helios binary.
// Payload format:
//   [uint32] vertex_count
//   [uint32] index_count
//   [vertex_count * sizeof(PBRVertex)] raw vertex data
//   [index_count  * sizeof(uint32)]    raw index data
static void write_hvemesh(const std::filesystem::path& path,
                          const PrimitiveMesh& mesh) {
    helios::BinaryWriter payload;
    payload.write<uint32_t>(static_cast<uint32_t>(mesh.vertices.size()));
    payload.write<uint32_t>(static_cast<uint32_t>(mesh.indices.size()));
    payload.write_bytes(reinterpret_cast<const uint8_t*>(mesh.vertices.data()),
                        mesh.vertices.size() * sizeof(helios::PBRVertex));
    payload.write_bytes(reinterpret_cast<const uint8_t*>(mesh.indices.data()),
                        mesh.indices.size() * sizeof(uint32_t));

    // Default material (white, non-metallic)
    payload.write<float>(1.0f); // base_color.r
    payload.write<float>(1.0f); // base_color.g
    payload.write<float>(1.0f); // base_color.b
    payload.write<float>(0.0f); // metallic
    payload.write<float>(0.5f); // roughness
    // No embedded textures (albedo, normal, metallic_roughness, emissive)
    for (int i = 0; i < 4; ++i) {
        payload.write<uint32_t>(0); // width
        payload.write<uint32_t>(0); // height
        payload.write<uint32_t>(0); // size
    }

    helios::AssetBinaryHeader header;
    header.guid = helios::UUID::generate();
    header.type = helios::AssetBinaryType::Mesh;
    header.version = 1;
    header.metadata["_source_path"] = path.filename().string();

    auto data = payload.take();
    auto binary = helios::write_asset_binary(header, data);

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(binary.data()),
              static_cast<std::streamsize>(binary.size()));
}

// Helper: create a PBRVertex with default tangent (1,0,0,1).
static helios::PBRVertex make_vertex(float px, float py, float pz,
                                     float nx, float ny, float nz,
                                     float u, float v) {
    helios::PBRVertex vtx{};
    vtx.position = {px, py, pz};
    vtx.normal   = {nx, ny, nz};
    vtx.uv       = {u, v};
    vtx.tangent  = {1.0f, 0.0f, 0.0f, 1.0f};
    return vtx;
}

// ---- Primitive generators ----

static PrimitiveMesh generate_cube() {
    PrimitiveMesh m;
    const float P = 0.5f, N = -0.5f;
    helios::PBRVertex verts[] = {
        // +Z face
        make_vertex(N,N,P, 0,0,1, 0,0), make_vertex(P,N,P, 0,0,1, 1,0),
        make_vertex(P,P,P, 0,0,1, 1,1), make_vertex(N,P,P, 0,0,1, 0,1),
        // -Z face
        make_vertex(P,N,N, 0,0,-1, 0,0), make_vertex(N,N,N, 0,0,-1, 1,0),
        make_vertex(N,P,N, 0,0,-1, 1,1), make_vertex(P,P,N, 0,0,-1, 0,1),
        // +X face
        make_vertex(P,N,P, 1,0,0, 0,0), make_vertex(P,N,N, 1,0,0, 1,0),
        make_vertex(P,P,N, 1,0,0, 1,1), make_vertex(P,P,P, 1,0,0, 0,1),
        // -X face
        make_vertex(N,N,N, -1,0,0, 0,0), make_vertex(N,N,P, -1,0,0, 1,0),
        make_vertex(N,P,P, -1,0,0, 1,1), make_vertex(N,P,N, -1,0,0, 0,1),
        // +Y face
        make_vertex(N,P,P, 0,1,0, 0,0), make_vertex(P,P,P, 0,1,0, 1,0),
        make_vertex(P,P,N, 0,1,0, 1,1), make_vertex(N,P,N, 0,1,0, 0,1),
        // -Y face
        make_vertex(N,N,N, 0,-1,0, 0,0), make_vertex(P,N,N, 0,-1,0, 1,0),
        make_vertex(P,N,P, 0,-1,0, 1,1), make_vertex(N,N,P, 0,-1,0, 0,1),
    };
    m.vertices.assign(std::begin(verts), std::end(verts));
    uint32_t idx[] = {
        0,1,2, 2,3,0,  4,5,6, 6,7,4,  8,9,10, 10,11,8,
        12,13,14, 14,15,12,  16,17,18, 18,19,16,  20,21,22, 22,23,20
    };
    m.indices.assign(std::begin(idx), std::end(idx));
    return m;
}

static PrimitiveMesh generate_sphere(int stacks = 16, int slices = 32) {
    PrimitiveMesh m;
    const float PI = 3.14159265358979f;
    for (int i = 0; i <= stacks; i++) {
        float phi = PI * static_cast<float>(i) / static_cast<float>(stacks);
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * PI * static_cast<float>(j) / static_cast<float>(slices);
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            float u = static_cast<float>(j) / static_cast<float>(slices);
            float v = static_cast<float>(i) / static_cast<float>(stacks);
            m.vertices.push_back(make_vertex(x * 0.5f, y * 0.5f, z * 0.5f,
                                             x, y, z, u, v));
        }
    }
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            uint32_t a = static_cast<uint32_t>(i * (slices + 1) + j);
            uint32_t b = a + static_cast<uint32_t>(slices + 1);
            m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(a + 1);
            m.indices.push_back(a + 1); m.indices.push_back(b); m.indices.push_back(b + 1);
        }
    }
    return m;
}

static PrimitiveMesh generate_plane() {
    PrimitiveMesh m;
    helios::PBRVertex verts[] = {
        make_vertex(-5,0,-5, 0,1,0, 0,0), make_vertex( 5,0,-5, 0,1,0, 1,0),
        make_vertex( 5,0, 5, 0,1,0, 1,1), make_vertex(-5,0, 5, 0,1,0, 0,1),
    };
    m.vertices.assign(std::begin(verts), std::end(verts));
    uint32_t idx[] = {0, 1, 2, 2, 3, 0};
    m.indices.assign(std::begin(idx), std::end(idx));
    return m;
}

void generate_primitive_meshes(const std::filesystem::path& meshes_dir) {
    std::filesystem::create_directories(meshes_dir);
    write_hvemesh(meshes_dir / "Cube.hvemesh", generate_cube());
    write_hvemesh(meshes_dir / "Sphere.hvemesh", generate_sphere());
    write_hvemesh(meshes_dir / "Plane.hvemesh", generate_plane());
}

} // namespace helios::editor

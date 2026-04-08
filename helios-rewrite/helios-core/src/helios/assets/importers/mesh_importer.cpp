#include "helios/assets/importers/mesh_importer.h"
#include "helios/assets/asset_server.h"
#include "helios/assets/texture_asset.h"
#include "helios/assets/material_asset.h"
#include "helios/assets/mesh_asset.h"

#include <stdexcept>
#include <cstring>

// cgltf implementation is compiled here
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// stb_image for texture loading within mesh importer
#include <stb_image.h>

namespace helios {

namespace {

// Read accessor data as float array
void read_accessor_floats(const cgltf_accessor* accessor,
                          float* out, size_t float_count) {
    for (size_t i = 0; i < accessor->count; ++i) {
        size_t num_components = cgltf_num_components(accessor->type);
        cgltf_accessor_read_float(accessor, i,
                                  out + i * num_components,
                                  num_components);
    }
}

MaterialData extract_material(const cgltf_material* mat,
                              const std::filesystem::path& model_dir) {
    MaterialData result;

    if (mat->name) {
        result.name = mat->name;
    }

    if (mat->has_pbr_metallic_roughness) {
        auto& pbr = mat->pbr_metallic_roughness;
        result.base_color = {
            pbr.base_color_factor[0],
            pbr.base_color_factor[1],
            pbr.base_color_factor[2]
        };
        result.metallic = pbr.metallic_factor;
        result.roughness = pbr.roughness_factor;

        if (pbr.base_color_texture.texture &&
            pbr.base_color_texture.texture->image &&
            pbr.base_color_texture.texture->image->uri) {
            result.albedo_texture =
                (model_dir / pbr.base_color_texture.texture->image->uri).string();
        }

        if (pbr.metallic_roughness_texture.texture &&
            pbr.metallic_roughness_texture.texture->image &&
            pbr.metallic_roughness_texture.texture->image->uri) {
            result.metallic_roughness_texture =
                (model_dir / pbr.metallic_roughness_texture.texture->image->uri).string();
        }
    }

    if (mat->normal_texture.texture &&
        mat->normal_texture.texture->image &&
        mat->normal_texture.texture->image->uri) {
        result.normal_texture =
            (model_dir / mat->normal_texture.texture->image->uri).string();
    }

    if (mat->occlusion_texture.texture &&
        mat->occlusion_texture.texture->image &&
        mat->occlusion_texture.texture->image->uri) {
        result.ao_texture =
            (model_dir / mat->occlusion_texture.texture->image->uri).string();
    }

    if (mat->emissive_texture.texture &&
        mat->emissive_texture.texture->image &&
        mat->emissive_texture.texture->image->uri) {
        result.emissive_texture =
            (model_dir / mat->emissive_texture.texture->image->uri).string();
    }

    return result;
}

void process_primitive(const cgltf_primitive* prim,
                       const cgltf_data* data,
                       MeshData& out) {
    if (prim->type != cgltf_primitive_type_triangles) return;

    SubMesh submesh;
    submesh.vertex_offset = static_cast<uint32_t>(out.vertices.size());
    submesh.index_offset = static_cast<uint32_t>(out.indices.size());

    // Find material index
    if (prim->material) {
        for (cgltf_size i = 0; i < data->materials_count; ++i) {
            if (&data->materials[i] == prim->material) {
                submesh.material_index = static_cast<int>(i);
                break;
            }
        }
    }

    // Find position, normal, texcoord, tangent accessors
    const cgltf_accessor* pos_accessor = nullptr;
    const cgltf_accessor* norm_accessor = nullptr;
    const cgltf_accessor* uv_accessor = nullptr;
    const cgltf_accessor* tan_accessor = nullptr;

    for (cgltf_size i = 0; i < prim->attributes_count; ++i) {
        switch (prim->attributes[i].type) {
        case cgltf_attribute_type_position:
            pos_accessor = prim->attributes[i].data;
            break;
        case cgltf_attribute_type_normal:
            norm_accessor = prim->attributes[i].data;
            break;
        case cgltf_attribute_type_texcoord:
            if (!uv_accessor) uv_accessor = prim->attributes[i].data;
            break;
        case cgltf_attribute_type_tangent:
            tan_accessor = prim->attributes[i].data;
            break;
        default:
            break;
        }
    }

    if (!pos_accessor) return;

    uint32_t vertex_count = static_cast<uint32_t>(pos_accessor->count);
    submesh.vertex_count = vertex_count;

    // Read vertices
    for (uint32_t i = 0; i < vertex_count; ++i) {
        Vertex v;

        float pos[3] = {};
        cgltf_accessor_read_float(pos_accessor, i, pos, 3);
        v.position = {pos[0], pos[1], pos[2]};

        if (norm_accessor) {
            float norm[3] = {};
            cgltf_accessor_read_float(norm_accessor, i, norm, 3);
            v.normal = {norm[0], norm[1], norm[2]};
        }

        if (uv_accessor) {
            float uv[2] = {};
            cgltf_accessor_read_float(uv_accessor, i, uv, 2);
            v.texcoord = {uv[0], uv[1]};
        }

        if (tan_accessor) {
            float tan[4] = {};
            cgltf_accessor_read_float(tan_accessor, i, tan, 4);
            v.tangent = {tan[0], tan[1], tan[2], tan[3]};
        }

        out.vertices.push_back(v);
    }

    // Read indices
    if (prim->indices) {
        for (cgltf_size i = 0; i < prim->indices->count; ++i) {
            uint32_t idx = static_cast<uint32_t>(
                cgltf_accessor_read_index(prim->indices, i));
            out.indices.push_back(submesh.vertex_offset + idx);
        }
        submesh.index_count = static_cast<uint32_t>(prim->indices->count);
    } else {
        // Non-indexed geometry: generate sequential indices
        for (uint32_t i = 0; i < vertex_count; ++i) {
            out.indices.push_back(submesh.vertex_offset + i);
        }
        submesh.index_count = vertex_count;
    }

    out.submeshes.push_back(submesh);
}

} // anonymous namespace

std::any MeshImporter::import(const std::filesystem::path& path, AssetServer& /*server*/) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Mesh file not found: " + path.string());
    }

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.string().c_str(), &data);
    if (result != cgltf_result_success) {
        throw std::runtime_error(
            "cgltf failed to parse '" + path.string() + "'");
    }

    result = cgltf_load_buffers(&options, data, path.string().c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error(
            "cgltf failed to load buffers for '" + path.string() + "'");
    }

    MeshData mesh_data;
    mesh_data.source_path = path.string();

    // Extract materials
    std::filesystem::path model_dir = path.parent_path();
    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        mesh_data.materials.push_back(
            extract_material(&data->materials[i], model_dir));
    }

    // Process all meshes and their primitives
    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& mesh = data->meshes[m];
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            process_primitive(&mesh.primitives[p], data, mesh_data);
        }
    }

    cgltf_free(data);

    return std::any(std::move(mesh_data));
}

// ---------------------------------------------------------------------------
// Helper: load a texture file into a TextureAsset and store it in the server
// ---------------------------------------------------------------------------
namespace {

AssetHandle load_texture_sub_asset(const std::string& texture_path,
                                   AssetServer& server) {
    // Use the absolute path as a storage key for deduplication.
    // store() checks the cache first, so repeated loads of the same
    // texture return the same handle.
    std::string store_key = "texture:" + texture_path;

    // Load pixels from disk
    int w, h, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* raw = stbi_load(texture_path.c_str(), &w, &h, &channels, 4);
    if (!raw) {
        return AssetHandle{};
    }

    TextureAsset tex;
    tex.width = static_cast<uint32_t>(w);
    tex.height = static_cast<uint32_t>(h);
    tex.hdr = false;
    size_t byte_count = static_cast<size_t>(w) * h * 4;
    tex.pixels.assign(raw, raw + byte_count);
    stbi_image_free(raw);

    auto handle = server.store<TextureAsset>(store_key, std::move(tex));
    server.acquire(handle);  // held by the parent material
    return handle;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// import_mesh_asset: full asset pipeline -- loads textures, creates materials
// ---------------------------------------------------------------------------

std::any MeshImporter::import_mesh_asset(const std::filesystem::path& path,
                                         AssetServer& server) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Mesh file not found: " + path.string());
    }

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.string().c_str(), &data);
    if (result != cgltf_result_success) {
        throw std::runtime_error(
            "cgltf failed to parse '" + path.string() + "'");
    }

    result = cgltf_load_buffers(&options, data, path.string().c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error(
            "cgltf failed to load buffers for '" + path.string() + "'");
    }

    std::filesystem::path model_dir = path.parent_path();
    std::string path_prefix = path.string();

    // --- Load materials as sub-assets ---
    std::vector<AssetHandle> material_handles;
    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        const cgltf_material* mat = &data->materials[i];
        MaterialAsset mat_asset;

        if (mat->has_pbr_metallic_roughness) {
            auto& pbr = mat->pbr_metallic_roughness;
            mat_asset.base_color = {
                pbr.base_color_factor[0],
                pbr.base_color_factor[1],
                pbr.base_color_factor[2]
            };
            mat_asset.metallic = pbr.metallic_factor;
            mat_asset.roughness = pbr.roughness_factor;

            // Albedo texture
            if (pbr.base_color_texture.texture &&
                pbr.base_color_texture.texture->image &&
                pbr.base_color_texture.texture->image->uri) {
                auto tex_path = (model_dir / pbr.base_color_texture.texture->image->uri).string();
                auto h = load_texture_sub_asset(tex_path, server);
                if (h) mat_asset.albedo = Handle<TextureAsset>::from(h);
            }

            // Metallic-roughness texture
            if (pbr.metallic_roughness_texture.texture &&
                pbr.metallic_roughness_texture.texture->image &&
                pbr.metallic_roughness_texture.texture->image->uri) {
                auto tex_path = (model_dir / pbr.metallic_roughness_texture.texture->image->uri).string();
                auto h = load_texture_sub_asset(tex_path, server);
                if (h) mat_asset.metallic_roughness = Handle<TextureAsset>::from(h);
            }
        }

        // Normal texture
        if (mat->normal_texture.texture &&
            mat->normal_texture.texture->image &&
            mat->normal_texture.texture->image->uri) {
            auto tex_path = (model_dir / mat->normal_texture.texture->image->uri).string();
            auto h = load_texture_sub_asset(tex_path, server);
            if (h) mat_asset.normal = Handle<TextureAsset>::from(h);
        }

        // Emissive texture
        if (mat->emissive_texture.texture &&
            mat->emissive_texture.texture->image &&
            mat->emissive_texture.texture->image->uri) {
            auto tex_path = (model_dir / mat->emissive_texture.texture->image->uri).string();
            auto h = load_texture_sub_asset(tex_path, server);
            if (h) mat_asset.emissive = Handle<TextureAsset>::from(h);
        }

        // Store the material
        std::string mat_name = mat->name ? mat->name : ("material_" + std::to_string(i));
        std::string mat_path = path_prefix + "#material/" + mat_name;
        auto mat_handle = server.store<MaterialAsset>(mat_path, std::move(mat_asset));
        server.acquire(mat_handle);  // held by the parent mesh

        // Register texture dependencies on the material
        const auto* stored_mat = server.get<MaterialAsset>(mat_handle);
        if (stored_mat) {
            if (stored_mat->albedo)
                server.add_dependency(mat_handle, stored_mat->albedo.untyped());
            if (stored_mat->normal)
                server.add_dependency(mat_handle, stored_mat->normal.untyped());
            if (stored_mat->metallic_roughness)
                server.add_dependency(mat_handle, stored_mat->metallic_roughness.untyped());
            if (stored_mat->emissive)
                server.add_dependency(mat_handle, stored_mat->emissive.untyped());
        }

        material_handles.push_back(mat_handle);
    }

    // --- Build MeshAsset from all primitives ---
    MeshAsset mesh_asset;

    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& mesh = data->meshes[m];
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_primitive& prim = mesh.primitives[p];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            uint32_t vertex_offset = static_cast<uint32_t>(mesh_asset.vertices.size());

            // Find accessors
            const cgltf_accessor* pos_acc = nullptr;
            const cgltf_accessor* norm_acc = nullptr;
            const cgltf_accessor* uv_acc = nullptr;
            const cgltf_accessor* tan_acc = nullptr;

            for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                switch (prim.attributes[a].type) {
                case cgltf_attribute_type_position: pos_acc = prim.attributes[a].data; break;
                case cgltf_attribute_type_normal:   norm_acc = prim.attributes[a].data; break;
                case cgltf_attribute_type_texcoord:
                    if (!uv_acc) uv_acc = prim.attributes[a].data;
                    break;
                case cgltf_attribute_type_tangent:  tan_acc = prim.attributes[a].data; break;
                default: break;
                }
            }

            if (!pos_acc) continue;

            uint32_t vert_count = static_cast<uint32_t>(pos_acc->count);

            for (uint32_t v = 0; v < vert_count; ++v) {
                PBRVertex vtx{};
                float tmp[4] = {};

                cgltf_accessor_read_float(pos_acc, v, tmp, 3);
                vtx.position = {tmp[0], tmp[1], tmp[2]};

                if (norm_acc) {
                    cgltf_accessor_read_float(norm_acc, v, tmp, 3);
                    vtx.normal = {tmp[0], tmp[1], tmp[2]};
                } else {
                    vtx.normal = {0.0f, 1.0f, 0.0f};
                }

                if (uv_acc) {
                    cgltf_accessor_read_float(uv_acc, v, tmp, 2);
                    vtx.uv = {tmp[0], tmp[1]};
                }

                if (tan_acc) {
                    cgltf_accessor_read_float(tan_acc, v, tmp, 4);
                    vtx.tangent = {tmp[0], tmp[1], tmp[2], tmp[3]};
                } else {
                    vtx.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                }

                mesh_asset.vertices.push_back(vtx);
            }

            // Indices
            if (prim.indices) {
                for (cgltf_size idx = 0; idx < prim.indices->count; ++idx) {
                    uint32_t i = static_cast<uint32_t>(
                        cgltf_accessor_read_index(prim.indices, idx));
                    mesh_asset.indices.push_back(vertex_offset + i);
                }
            } else {
                for (uint32_t v = 0; v < vert_count; ++v) {
                    mesh_asset.indices.push_back(vertex_offset + v);
                }
            }

            // Set default material from the first primitive's material
            if (!mesh_asset.default_material && prim.material) {
                for (cgltf_size mi = 0; mi < data->materials_count; ++mi) {
                    if (&data->materials[mi] == prim.material && mi < material_handles.size()) {
                        mesh_asset.default_material = Handle<MaterialAsset>::from(material_handles[mi]);
                        break;
                    }
                }
            }
        }
    }

    // If no material was assigned but materials exist, use the first one
    if (!mesh_asset.default_material && !material_handles.empty()) {
        mesh_asset.default_material = Handle<MaterialAsset>::from(material_handles[0]);
    }

    cgltf_free(data);

    return std::any(std::move(mesh_asset));
}

} // namespace helios

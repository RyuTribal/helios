#include "helios/assets/asset_plugin.h"

#include "helios/assets/asset_binary.h"
#include "helios/assets/asset_server.h"
#include "helios/assets/binary_writer.h"
#include "helios/core/assert.h"
#include "helios/core/engine_log_channels.h"
#include "helios/assets/material_asset.h"
#include "helios/assets/mesh_asset.h"
#include "helios/assets/shader_asset.h"
#include "helios/assets/texture_asset.h"
#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/importers/mesh_importer.h"
#include "helios/assets/importers/audio_importer.h"
#include "helios/assets/importers/shader_importer.h"
#include "helios/assets/cubemap_asset.h"
#include "helios/ecs/app.h"
#include "helios/ecs/thread_pool.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/system_params.h"
#include "helios/ecs/event_storage.h"

#include <stb_image.h>

#include <filesystem>
#include <fstream>

namespace helios {

void AssetPlugin::build(App& app) {
    HELIOS_ASSERT(!config.asset_root.empty(),
        "AssetPlugin requires an asset_root path. Pass AssetPluginConfig{.asset_root = \"path/to/assets\"}");

    // 1. Insert AssetServer as a World resource.
    //    AssetServer is non-movable, so we heap-allocate via a shared_ptr wrapper
    //    stored inside a helper struct that IS movable.
    std::shared_ptr<ThreadPool> pool;
    if (app.world().has_resource<std::shared_ptr<ThreadPool>>())
        pool = app.world().resource<std::shared_ptr<ThreadPool>>();
    app.world().insert_resource<std::shared_ptr<AssetServer>>(
        std::make_shared<AssetServer>(config.asset_root, std::move(pool)));

    // 2. Register default importers.
    auto& server = *app.world().resource<std::shared_ptr<AssetServer>>();
    server.register_importer<TextureData>(TextureImporter::import_ldr);
    server.register_importer<HdrTextureData>(TextureImporter::import_hdr);
    server.register_importer<MeshData>(MeshImporter::import);
    server.register_importer<MeshAsset>(MeshImporter::import_mesh_asset);
    server.register_importer<AudioData>(AudioImporter::import);
    server.register_importer<ShaderAsset>(ShaderImporter::import);
    server.register_importer<CubeMapAsset>([](const std::filesystem::path& path, AssetServer& s) -> std::any {
        // Reuse the HDR importer to load the raw data
        auto hdr_any = TextureImporter::import_hdr(path, s);
        if (!hdr_any.has_value()) return std::any{};
        CubeMapAsset asset;
        asset.hdr_source = std::any_cast<HdrTextureData>(std::move(hdr_any));
        asset.source_path = path.string();
        return asset;
    });

    // 3. Register default file-extension -> type mappings.
    //    Helios binary extensions (.hvemesh, .hvetex, .hvecube, .hveaudio) are
    //    included so the existing importers handle them transparently.
    server.register_extensions<MeshAsset>({"gltf", "glb", "hvemesh"});
    server.register_extensions<TextureData>({"png", "jpg", "jpeg", "bmp", "tga", "hvetex"});
    server.register_extensions<HdrTextureData>({"hdr", "exr", "hvecube"});
    server.register_extensions<AudioData>({"wav", "ogg", "mp3", "flac", "hveaudio"});
    server.register_extensions<ShaderAsset>({"spv"});

    // --- Binary asset import/export pipeline ---
    // These work alongside the old ImporterFn system.
    // Old system: load_sync<T>(path) -> uses ImporterFn -> returns runtime object
    // New system: import_asset(source, type, dest) -> creates Helios binary with GUID

    // Mesh importer: .glb/.gltf/.fbx -> optimized PBRVertex + index binary
    server.register_asset_importer(AssetBinaryType::Mesh,
        [](const std::vector<uint8_t>& /*source_bytes*/, const AssetMetadata& meta,
           AssetServer& srv) -> std::vector<uint8_t> {
            // Use the original source path (not temp file) so cgltf can find
            // external .bin and texture files for .gltf imports.
            auto it = meta.find("_source_path");
            if (it == meta.end() || it->second.empty()) {
                HELIOS_LOG(Assets, Error, "Mesh import: no _source_path in metadata");
                return {};
            }
            std::filesystem::path source_path = it->second;

            // If relative, resolve against asset server root
            if (source_path.is_relative()) {
                source_path = srv.root() / source_path;
            }

            auto result = MeshImporter::import_mesh_asset(source_path, srv);

            if (!result.has_value()) {
                HELIOS_LOG(Assets, Error, "Mesh import: cgltf parse failed");
                return {};
            }
            auto& mesh = std::any_cast<MeshAsset&>(result);

            // Serialize to optimized binary:
            //   [uint32] vertex_count
            //   [uint32] index_count
            //   [vertex_count * sizeof(PBRVertex)] raw vertex data
            //   [index_count  * sizeof(uint32)]    raw index data
            //   --- Material data ---
            //   [float] base_color_r, base_color_g, base_color_b
            //   [float] metallic, roughness
            //   --- Embedded textures (albedo, normal, metallic_roughness, emissive) ---
            //   each: [uint32] w, [uint32] h, [uint32] size, [uint8[]] pixels
            BinaryWriter w;
            w.write<uint32_t>(static_cast<uint32_t>(mesh.vertices.size()));
            w.write<uint32_t>(static_cast<uint32_t>(mesh.indices.size()));
            w.write_bytes(reinterpret_cast<const uint8_t*>(mesh.vertices.data()),
                          mesh.vertices.size() * sizeof(PBRVertex));
            w.write_bytes(reinterpret_cast<const uint8_t*>(mesh.indices.data()),
                          mesh.indices.size() * sizeof(uint32_t));

            // Material properties
            MaterialAsset mat;
            if (mesh.default_material) {
                const MaterialAsset* mp = srv.get<MaterialAsset>(
                    mesh.default_material.untyped());
                if (mp) mat = *mp;
            }
            w.write<float>(mat.base_color.r);
            w.write<float>(mat.base_color.g);
            w.write<float>(mat.base_color.b);
            w.write<float>(mat.metallic);
            w.write<float>(mat.roughness);

            // Embedded textures
            auto write_texture = [&](Handle<TextureAsset> tex_handle) {
                if (tex_handle) {
                    const TextureAsset* td = srv.get<TextureAsset>(
                        tex_handle.untyped());
                    if (td && !td->pixels.empty()) {
                        w.write<uint32_t>(td->width);
                        w.write<uint32_t>(td->height);
                        w.write<uint32_t>(
                            static_cast<uint32_t>(td->pixels.size()));
                        w.write_bytes(td->pixels.data(), td->pixels.size());
                        return;
                    }
                }
                // No texture: write zeros
                w.write<uint32_t>(0);
                w.write<uint32_t>(0);
                w.write<uint32_t>(0);
            };

            write_texture(mat.albedo);
            write_texture(mat.normal);
            write_texture(mat.metallic_roughness);
            write_texture(mat.emissive);

            return w.take();
        },
        AssetImportSettings{
            .type = AssetBinaryType::Mesh,
            .default_metadata = {},
            .source_extensions = {"glb", "gltf", "fbx", "obj"},
            .export_formats = {},
        });

    // Texture importer: .png/.jpg/.bmp/.tga -> optimized RGBA8 pixel binary
    server.register_asset_importer(AssetBinaryType::Texture,
        [](const std::vector<uint8_t>& source_bytes, const AssetMetadata& /*meta*/,
           AssetServer& /*srv*/) -> std::vector<uint8_t> {
            int w = 0, h = 0, channels = 0;
            constexpr int desired = 4; // RGBA
            stbi_set_flip_vertically_on_load(false);
            unsigned char* raw = stbi_load_from_memory(
                source_bytes.data(),
                static_cast<int>(source_bytes.size()),
                &w, &h, &channels, desired);
            if (!raw) {
                HELIOS_LOG(Assets, Error, "Texture import: stbi_load_from_memory failed: {}",
                           stbi_failure_reason());
                return {};
            }

            // Serialize:
            //   [uint32] width
            //   [uint32] height
            //   [uint32] channels (always 4)
            //   [width * height * 4 bytes] raw RGBA8 pixels
            BinaryWriter wr;
            wr.write<uint32_t>(static_cast<uint32_t>(w));
            wr.write<uint32_t>(static_cast<uint32_t>(h));
            wr.write<uint32_t>(static_cast<uint32_t>(desired));
            size_t pixel_bytes = static_cast<size_t>(w) * h * desired;
            wr.write_bytes(raw, pixel_bytes);
            stbi_image_free(raw);
            return wr.take();
        },
        AssetImportSettings{
            .type = AssetBinaryType::Texture,
            .default_metadata = {},
            .source_extensions = {"png", "jpg", "jpeg", "bmp", "tga"},
            .export_formats = {},
        });

    // CubeMap importer: .hdr/.exr -> optimized HDR float pixel binary
    server.register_asset_importer(AssetBinaryType::CubeMap,
        [](const std::vector<uint8_t>& source_bytes, const AssetMetadata& /*meta*/,
           AssetServer& /*srv*/) -> std::vector<uint8_t> {
            int w = 0, h = 0, channels = 0;
            stbi_set_flip_vertically_on_load(false);
            float* raw = stbi_loadf_from_memory(
                source_bytes.data(),
                static_cast<int>(source_bytes.size()),
                &w, &h, &channels, 0);
            if (!raw) {
                HELIOS_LOG(Assets, Error, "CubeMap import: stbi_loadf_from_memory failed: {}",
                           stbi_failure_reason());
                return {};
            }

            // Serialize:
            //   [uint32] width
            //   [uint32] height
            //   [uint32] channels
            //   [width * height * channels * sizeof(float)] raw HDR float pixels
            BinaryWriter wr;
            wr.write<uint32_t>(static_cast<uint32_t>(w));
            wr.write<uint32_t>(static_cast<uint32_t>(h));
            wr.write<uint32_t>(static_cast<uint32_t>(channels));
            size_t float_count = static_cast<size_t>(w) * h * channels;
            wr.write_bytes(reinterpret_cast<const uint8_t*>(raw),
                           float_count * sizeof(float));
            stbi_image_free(raw);
            return wr.take();
        },
        AssetImportSettings{
            .type = AssetBinaryType::CubeMap,
            .default_metadata = {},
            .source_extensions = {"hdr", "exr"},
            .export_formats = {},
        });

    // Audio importer: .wav/.ogg/.mp3/.flac -> binary audio data
    server.register_asset_importer(AssetBinaryType::Audio,
        [](const std::vector<uint8_t>& source_bytes, const AssetMetadata& /*meta*/,
           AssetServer& /*srv*/) -> std::vector<uint8_t> {
            return source_bytes;
        },
        AssetImportSettings{
            .type = AssetBinaryType::Audio,
            .default_metadata = {},
            .source_extensions = {"wav", "ogg", "mp3", "flac"},
            .export_formats = {"wav"},
        });

    // Shader importer: .spv -> binary shader data
    server.register_asset_importer(AssetBinaryType::Shader,
        [](const std::vector<uint8_t>& source_bytes, const AssetMetadata& /*meta*/,
           AssetServer& /*srv*/) -> std::vector<uint8_t> {
            return source_bytes;
        },
        AssetImportSettings{
            .type = AssetBinaryType::Shader,
            .default_metadata = {},
            .source_extensions = {"spv"},
            .export_formats = {},
        });

    // NOTE: Exporters removed. The binary now stores optimized runtime data
    // (PBRVertex[], RGBA8 pixels, HDR floats), not original source bytes.
    // Re-export to source format would require reverse conversion.

    // 4. Register AssetLoaded event type.
    app.add_event<AssetLoaded>();

    // 5. Add the drain system to PreUpdate: each frame, move completed async
    //    load results into the event channel so downstream systems can react.
    app.add_system(Schedule::PreUpdate,
        [](ResMut<std::shared_ptr<AssetServer>> server_res,
           EventWriter<AssetLoaded> writer) {
            auto completed = (*server_res)->drain_completed();
            for (auto& event : completed) {
                writer.send(std::move(event));
            }
        },
        "asset_drain_completed");

    // 6. Garbage-collect zero-refcount assets each frame.
    //    When entities are despawned, their Handle<T> destructors decrement
    //    refcounts. Assets with refcount 0 are freed here, allowing the GPU
    //    cache eviction (in extract_render_data) to reclaim GPU memory.
    app.add_system(Schedule::PreUpdate,
        [](ResMut<std::shared_ptr<AssetServer>> server_res) {
            (*server_res)->collect_garbage();
        },
        "asset_gc");

    // 7. Hot reload.
    if (config.hot_reload) {
        server.watch_for_changes(true);
    }
}

} // namespace helios

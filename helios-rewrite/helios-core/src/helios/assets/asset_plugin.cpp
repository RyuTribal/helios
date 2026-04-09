#include "helios/assets/asset_plugin.h"

#include "helios/assets/asset_server.h"
#include "helios/core/assert.h"
#include "helios/assets/mesh_asset.h"
#include "helios/assets/shader_asset.h"
#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/importers/mesh_importer.h"
#include "helios/assets/importers/audio_importer.h"
#include "helios/assets/importers/shader_importer.h"
#include "helios/ecs/app.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/system_params.h"
#include "helios/ecs/event_storage.h"

namespace helios {

void AssetPlugin::build(App& app) {
    HELIOS_ASSERT(!config.asset_root.empty(),
        "AssetPlugin requires an asset_root path. Pass AssetPluginConfig{.asset_root = \"path/to/assets\"}");

    // 1. Insert AssetServer as a World resource.
    //    AssetServer is non-movable, so we heap-allocate via a shared_ptr wrapper
    //    stored inside a helper struct that IS movable.
    app.world().insert_resource<std::shared_ptr<AssetServer>>(
        std::make_shared<AssetServer>(config.asset_root, config.loader_threads));

    // 2. Register default importers.
    auto& server = *app.world().resource<std::shared_ptr<AssetServer>>();
    server.register_importer<TextureData>(TextureImporter::import_ldr);
    server.register_importer<HdrTextureData>(TextureImporter::import_hdr);
    server.register_importer<MeshData>(MeshImporter::import);
    server.register_importer<MeshAsset>(MeshImporter::import_mesh_asset);
    server.register_importer<AudioData>(AudioImporter::import);
    server.register_importer<ShaderAsset>(ShaderImporter::import);

    // 3. Register default file-extension -> type mappings.
    server.register_extensions<MeshAsset>({"gltf", "glb"});
    server.register_extensions<TextureData>({"png", "jpg", "jpeg", "bmp", "tga"});
    server.register_extensions<HdrTextureData>({"hdr", "exr"});
    server.register_extensions<AudioData>({"wav", "ogg", "mp3", "flac"});
    server.register_extensions<ShaderAsset>({"spv"});

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

    // 6. Hot reload.
    if (config.hot_reload) {
        server.watch_for_changes(true);
    }
}

} // namespace helios

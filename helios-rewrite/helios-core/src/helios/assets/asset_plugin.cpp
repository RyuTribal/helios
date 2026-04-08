#include "helios/assets/asset_plugin.h"

#include "helios/assets/asset_server.h"
#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/importers/mesh_importer.h"
#include "helios/assets/importers/audio_importer.h"

namespace helios {

void AssetPlugin::build(App& /*app*/) {
    // NOTE: The integration with App and World is commented out because it
    // depends on the specific ECS resource/event API from Plan 1 (ECS) and
    // Plan 2 (App). When those are implemented, uncomment and wire up:
    //
    // 1. Insert AssetServer as a World resource:
    //    app.insert_resource<AssetServer>(
    //        AssetServer(config.asset_root, config.loader_threads));
    //
    // 2. Register default importers:
    //    auto& server = app.world().resource<AssetServer>();
    //    server.register_importer<TextureData>(TextureImporter::import_ldr);
    //    server.register_importer<HdrTextureData>(TextureImporter::import_hdr);
    //    server.register_importer<MeshData>(MeshImporter::import);
    //    server.register_importer<AudioData>(AudioImporter::import);
    //
    // 3. Register AssetLoaded event type:
    //    app.add_event<AssetLoaded>();
    //
    // 4. Add the drain system to PreUpdate:
    //    app.add_system(Schedule::PreUpdate, [](ResMut<AssetServer> server,
    //                                           EventWriter<AssetLoaded> writer) {
    //        auto completed = server->drain_completed();
    //        for (auto& event : completed) {
    //            writer.send(std::move(event));
    //        }
    //    });
    //
    // 5. Hot reload:
    //    if (config.hot_reload) {
    //        server.watch_for_changes(true);
    //    }
}

} // namespace helios

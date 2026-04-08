#include "helios/assets/asset_plugin.h"

#include "helios/assets/asset_server.h"
#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/importers/mesh_importer.h"
#include "helios/assets/importers/audio_importer.h"
#include "helios/ecs/app.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/system_params.h"
#include "helios/ecs/event_storage.h"

namespace helios {

void AssetPlugin::build(App& app) {
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
    server.register_importer<AudioData>(AudioImporter::import);

    // 3. Register AssetLoaded event type.
    app.add_event<AssetLoaded>();

    // 4. Add the drain system to PreUpdate: each frame, move completed async
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

    // 5. Hot reload.
    if (config.hot_reload) {
        server.watch_for_changes(true);
    }
}

} // namespace helios

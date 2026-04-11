#pragma once

#include "helios/assets/asset_server.h"
#include "helios/assets/mesh_asset.h"
#include "helios/components/components.h"
#include "helios/ecs/world.h"

namespace helios {

/// Kick off async loads for all MeshRenderer entities that have a
/// path set but no loaded handle. Non-blocking — meshes appear when ready.
inline void resolve_mesh_paths(World& world) {
    if (!world.has_resource<std::shared_ptr<AssetServer>>()) return;
    auto& server = world.resource<std::shared_ptr<AssetServer>>();
    auto q = world.query<MeshRenderer>();
    for (auto [e, mr] : q.with_entity()) {
        if (!mr.mesh_path.empty() && !mr.mesh)
            mr.mesh = server->load<MeshAsset>(mr.mesh_path);
    }
}

} // namespace helios

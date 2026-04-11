#include "helios/assets/handle.h"
#include "helios/assets/asset_server.h"

namespace helios {

void asset_handle_acquire(AssetServer* server, AssetHandle id) {
    if (server && id) server->acquire(id);
}

void asset_handle_release(AssetServer* server, AssetHandle id) {
    if (server && id) server->release(id);
}

} // namespace helios

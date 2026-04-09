#pragma once

#include "helios/ecs/asset_handle.h"
#include "helios/assets/asset_server.h"

#include <string>
#include <vector>
#include <functional>
#include <typeindex>

namespace helios {

class AssetServer;
class LoadBatch;

class LoadBatchBuilder {
public:
    explicit LoadBatchBuilder(AssetServer& server);

    // Add an asset to the batch. T determines which importer is used.
    template<typename T>
    LoadBatchBuilder& add(const std::string& path);

    // Submit all queued loads and return a batch tracker.
    LoadBatch submit();

private:
    struct PendingEntry {
        std::string path;
        std::type_index type;
        std::function<AssetHandle(AssetServer&, const std::string&)> load_fn;
    };

    AssetServer& m_server;
    std::vector<PendingEntry> m_pending;
};

class LoadBatch {
public:
    LoadBatch() = default;
    LoadBatch(AssetServer& server, std::vector<AssetHandle> handles,
              std::vector<std::string> paths);

    // Progress: 0.0 (nothing loaded) to 1.0 (all loaded or failed).
    float progress() const;

    // Total number of assets in this batch.
    int total() const;

    // Number of assets still loading.
    int remaining() const;

    // True when all assets have finished (loaded or failed).
    bool is_complete() const;

    // Paths of assets that failed to load.
    std::vector<std::string> failed() const;

    // All handles in the batch.
    const std::vector<AssetHandle>& handles() const { return m_handles; }

private:
    AssetServer* m_server = nullptr;
    std::vector<AssetHandle> m_handles;
    std::vector<std::string> m_paths;
};

// --- Template implementation ---

template<typename T>
LoadBatchBuilder& LoadBatchBuilder::add(const std::string& path) {
    m_pending.push_back(PendingEntry{
        path,
        std::type_index(typeid(T)),
        [](AssetServer& server, const std::string& p) -> AssetHandle {
            auto h = server.load<T>(p);
            return h.untyped();
        }
    });
    return *this;
}

} // namespace helios

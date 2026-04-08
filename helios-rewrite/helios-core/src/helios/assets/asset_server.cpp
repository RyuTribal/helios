#include "helios/assets/asset_server.h"
#include "helios/assets/load_batch.h"

#include <cassert>
#include <optional>

namespace helios {

AssetServer::AssetServer(const std::filesystem::path& asset_root,
                         uint32_t loader_thread_count)
    : m_root(asset_root)
{
    for (uint32_t i = 0; i < loader_thread_count; ++i) {
        m_loader_threads.emplace_back(
            [this](std::stop_token stop) { loader_thread_main(stop); }
        );
    }
}

AssetServer::~AssetServer() {
    for (auto& thread : m_loader_threads) {
        thread.request_stop();
    }
    m_queue_cv.notify_all();
    // jthread destructor joins automatically
}

AssetHandle AssetServer::next_handle() {
    return AssetHandle{m_next_id.fetch_add(1, std::memory_order_relaxed)};
}

AssetHandle AssetServer::find_cached(std::type_index type,
                                     const std::string& path) const {
    // m_mutex must be held by caller
    std::string cache_key = std::string(type.name()) + ":" + path;
    auto it = m_path_cache.find(cache_key);
    if (it != m_path_cache.end()) {
        return it->second;
    }
    return AssetHandle{0};
}

void AssetServer::execute_load(const LoadRequest& request) {
    // Find the importer for this type
    ImporterFn importer;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_importers.find(request.type);
        if (it == m_importers.end()) {
            auto asset_it = m_assets.find(request.handle.id);
            if (asset_it != m_assets.end()) {
                asset_it->second.status = AssetStatus::Failed;
            }
            std::lock_guard cmp_lock(m_completed_mutex);
            m_completed.push_back(AssetLoaded{
                request.handle,
                request.full_path.string(),
                false
            });
            return;
        }
        importer = it->second;
    }

    // Run the importer (this is the expensive part - no locks held)
    std::any result;
    bool success = true;
    try {
        result = importer(request.full_path);
    } catch (const std::exception& /*e*/) {
        success = false;
    } catch (...) {
        success = false;
    }

    // Store result
    {
        std::lock_guard lock(m_mutex);
        auto it = m_assets.find(request.handle.id);
        if (it != m_assets.end()) {
            if (success) {
                it->second.data = std::move(result);
                it->second.status = AssetStatus::Loaded;
            } else {
                it->second.status = AssetStatus::Failed;
            }
        }
    }

    // Queue completion event
    {
        std::lock_guard lock(m_completed_mutex);
        m_completed.push_back(AssetLoaded{
            request.handle,
            request.full_path.string(),
            success
        });
    }
}

void AssetServer::loader_thread_main(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::optional<LoadRequest> request;
        {
            std::unique_lock lock(m_queue_mutex);
            m_queue_cv.wait(lock, [&] {
                return stop.stop_requested() || !m_load_queue.empty();
            });

            if (stop.stop_requested()) return;
            if (m_load_queue.empty()) continue;

            request = std::move(m_load_queue.front());
            m_load_queue.pop();
        }

        if (request) {
            execute_load(*request);
        }
    }
}

AssetStatus AssetServer::status(AssetHandle handle) const {
    std::lock_guard lock(m_mutex);
    auto it = m_assets.find(handle.id);
    if (it == m_assets.end()) return AssetStatus::Failed;
    return it->second.status;
}

bool AssetServer::is_loaded(AssetHandle handle) const {
    return status(handle) == AssetStatus::Loaded;
}

void AssetServer::watch_for_changes(bool enable) {
    m_watching = enable;
    // Hot reload implementation deferred
}

std::vector<AssetLoaded> AssetServer::drain_completed() {
    std::lock_guard lock(m_completed_mutex);
    auto result = std::move(m_completed);
    m_completed.clear();
    return result;
}

LoadBatchBuilder AssetServer::load_batch() {
    return LoadBatchBuilder(*this);
}

} // namespace helios

#include "helios/assets/asset_server.h"
#include "helios/assets/load_batch.h"
#include "helios/core/engine_log_channels.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <optional>

namespace helios {

// ---- Extension helpers ----

std::string AssetServer::normalize_extension(const std::string& ext) {
    std::string result;
    result.reserve(ext.size());
    for (char c : ext) {
        if (c == '.') continue;
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

std::string AssetServer::extract_extension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos || dot + 1 >= path.size()) return {};
    return normalize_extension(path.substr(dot + 1));
}

// ---- Shared load_internal ----

AssetHandle AssetServer::load_internal(std::type_index type,
                                       const std::string& path,
                                       bool sync) {
    // Check cache first
    AssetHandle cached_handle{};
    {
        std::lock_guard lock(m_mutex);
        auto cached = find_cached(type, path);
        if (cached) {
            if (sync) {
                uint64_t key = cached.packed();
                auto asset_it = m_assets.find(key);
                if (asset_it != m_assets.end() &&
                    asset_it->second.status == AssetStatus::Loaded) {
                    cached_handle = cached;
                }
            } else {
                cached_handle = cached;
            }
        }
    }
    // Return cached handle without acquiring -- callers (Handle<T> ctor or
    // load_by_extension) are responsible for acquiring.
    if (cached_handle) {
        return cached_handle;
    }

    // Create entry
    auto handle = next_handle();
    auto full_path = m_root / path;
    uint64_t key = handle.packed();

    {
        std::lock_guard lock(m_mutex);
        m_assets.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(key),
            std::forward_as_tuple(full_path, type)
        );
        std::string cache_key = std::string(type.name()) + ":" + path;
        m_path_cache[cache_key] = handle;
    }

    if (sync) {
        execute_load(LoadRequest{handle, full_path, type});

        // Check if it succeeded
        bool failed = false;
        {
            std::lock_guard lock(m_mutex);
            auto it = m_assets.find(key);
            if (it != m_assets.end() && it->second.status == AssetStatus::Failed) {
                failed = true;
            }
        }
        if (failed) return AssetHandle{};

        return handle;
    }

    // Async path
    {
        std::lock_guard lock(m_queue_mutex);
        m_load_queue.push(LoadRequest{handle, full_path, type});
    }
    m_queue_cv.notify_one();

    return handle;
}

// ---- Extension-based loading ----

AssetHandle AssetServer::load_by_extension(const std::string& path) {
    auto ext = extract_extension(path);
    std::type_index type(typeid(void));
    {
        std::lock_guard lock(m_mutex);
        auto it = m_extension_map.find(ext);
        if (it == m_extension_map.end()) {
            return AssetHandle{};
        }
        type = it->second;
    }
    auto handle = load_internal(type, path, /*sync=*/false);
    if (handle) acquire(handle);
    return handle;
}

AssetHandle AssetServer::load_sync_by_extension(const std::string& path) {
    auto ext = extract_extension(path);
    std::type_index type(typeid(void));
    {
        std::lock_guard lock(m_mutex);
        auto it = m_extension_map.find(ext);
        if (it == m_extension_map.end()) {
            return AssetHandle{};
        }
        type = it->second;
    }
    auto handle = load_internal(type, path, /*sync=*/true);
    if (handle) acquire(handle);
    return handle;
}

std::optional<std::type_index> AssetServer::type_for_extension(
    const std::string& ext) const {
    auto normalized = normalize_extension(ext);
    std::lock_guard lock(m_mutex);
    auto it = m_extension_map.find(normalized);
    if (it == m_extension_map.end()) return std::nullopt;
    return it->second;
}

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
    uint32_t idx = m_next_index.fetch_add(1, std::memory_order_relaxed);
    return AssetHandle{idx, 1};  // generation starts at 1
}

AssetHandle AssetServer::find_cached(std::type_index type,
                                     const std::string& path) const {
    // m_mutex must be held by caller
    std::string cache_key = std::string(type.name()) + ":" + path;
    auto it = m_path_cache.find(cache_key);
    if (it != m_path_cache.end()) {
        return it->second;
    }
    return AssetHandle{};
}

void AssetServer::remove_from_path_cache(uint64_t packed_key) {
    // m_mutex must be held by caller
    for (auto it = m_path_cache.begin(); it != m_path_cache.end(); ++it) {
        if (it->second.packed() == packed_key) {
            m_path_cache.erase(it);
            return;
        }
    }
}

void AssetServer::execute_load(const LoadRequest& request) {
    uint64_t key = request.handle.packed();

    // Find the importer for this type
    ImporterFn importer;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_importers.find(request.type);
        if (it == m_importers.end()) {
            auto asset_it = m_assets.find(key);
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
        result = importer(request.full_path, *this);
    } catch (const std::exception& /*e*/) {
        success = false;
    } catch (...) {
        success = false;
    }

    // Store result
    {
        std::lock_guard lock(m_mutex);
        auto it = m_assets.find(key);
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
    uint64_t key = handle.packed();
    auto it = m_assets.find(key);
    if (it == m_assets.end()) return AssetStatus::Failed;
    return it->second.status;
}

bool AssetServer::is_loaded(AssetHandle handle) const {
    return status(handle) == AssetStatus::Loaded;
}

void AssetServer::acquire(AssetHandle handle) {
    std::lock_guard lock(m_mutex);
    uint64_t key = handle.packed();
    m_refcounts[key]++;
}

void AssetServer::release(AssetHandle handle) {
    std::lock_guard lock(m_mutex);
    uint64_t key = handle.packed();
    auto it = m_refcounts.find(key);
    if (it != m_refcounts.end() && it->second > 0) {
        it->second--;
    }
}

void AssetServer::add_dependency(AssetHandle parent, AssetHandle child) {
    std::lock_guard lock(m_mutex);
    uint64_t key = parent.packed();
    m_dependencies[key].push_back(child);
}

AssetHandle AssetServer::allocate_handle() {
    return next_handle();
}

uint32_t AssetServer::refcount(AssetHandle handle) const {
    std::lock_guard lock(m_mutex);
    uint64_t key = handle.packed();
    auto it = m_refcounts.find(key);
    if (it == m_refcounts.end()) return 0;
    return it->second;
}

std::vector<AssetHandle> AssetServer::collect_garbage() {
    std::lock_guard lock(m_mutex);
    std::vector<AssetHandle> unloaded;

    // Iterate until no more zero-refcount assets are found.
    // Cascading: releasing children may create new zero-refcount entries.
    bool found_any = true;
    while (found_any) {
        found_any = false;
        for (auto it = m_refcounts.begin(); it != m_refcounts.end(); ) {
            if (it->second == 0) {
                found_any = true;
                uint64_t key = it->first;
                AssetHandle handle = AssetHandle::from_packed(key);

                // Get path for logging before removing
                auto asset_it = m_assets.find(key);
                if (asset_it != m_assets.end()) {
                    std::string path_str = asset_it->second.path.string();
                    HELIOS_LOG(Assets, Info, "Unloaded asset '{}' (handle {}/{})",
                               path_str, handle.index, handle.generation);
                    m_assets.erase(asset_it);
                }

                // Remove from path cache
                remove_from_path_cache(key);

                // Cascade-release dependencies: decrement children refcounts
                auto dep_it = m_dependencies.find(key);
                if (dep_it != m_dependencies.end()) {
                    for (auto& child : dep_it->second) {
                        uint64_t child_key = child.packed();
                        auto child_rc = m_refcounts.find(child_key);
                        if (child_rc != m_refcounts.end() && child_rc->second > 0) {
                            child_rc->second--;
                        }
                    }
                    m_dependencies.erase(dep_it);
                }

                unloaded.push_back(handle);
                it = m_refcounts.erase(it);
            } else {
                ++it;
            }
        }
    }

    return unloaded;
}

void AssetServer::unload(AssetHandle handle) {
    std::lock_guard lock(m_mutex);
    uint64_t key = handle.packed();

    auto asset_it = m_assets.find(key);
    if (asset_it != m_assets.end()) {
        std::string path_str = asset_it->second.path.string();
        HELIOS_LOG(Assets, Info, "Unloaded asset '{}' (handle {}/{})",
                   path_str, handle.index, handle.generation);
        m_assets.erase(asset_it);
    }

    // Cascade-release dependencies
    auto dep_it = m_dependencies.find(key);
    if (dep_it != m_dependencies.end()) {
        for (auto& child : dep_it->second) {
            uint64_t child_key = child.packed();
            auto child_rc = m_refcounts.find(child_key);
            if (child_rc != m_refcounts.end() && child_rc->second > 0) {
                child_rc->second--;
            }
        }
        m_dependencies.erase(dep_it);
    }

    // Remove from path cache and refcounts
    remove_from_path_cache(key);
    m_refcounts.erase(key);
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

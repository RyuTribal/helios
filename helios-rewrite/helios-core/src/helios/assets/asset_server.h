#pragma once

#include "helios/ecs/asset_handle.h"

#include <any>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace helios {

// Forward declarations
class LoadBatchBuilder;
class LoadBatch;

// Type-erased importer function: takes (asset_root / path) -> std::any
// Returns the loaded asset data or throws on failure.
using ImporterFn = std::function<std::any(const std::filesystem::path&)>;

// Event emitted when an async load completes
struct AssetLoaded {
    AssetHandle handle;
    std::string path;
    bool success = true;
};

class AssetServer {
public:
    explicit AssetServer(const std::filesystem::path& asset_root,
                         uint32_t loader_thread_count = 2);
    ~AssetServer();

    // Non-copyable, non-movable (owns threads)
    AssetServer(const AssetServer&) = delete;
    AssetServer& operator=(const AssetServer&) = delete;
    AssetServer(AssetServer&&) = delete;
    AssetServer& operator=(AssetServer&&) = delete;

    // --- Importer registration ---
    template<typename T>
    void register_importer(ImporterFn importer);

    // --- Loading ---

    // Async load: returns handle immediately, loading happens on background thread.
    template<typename T>
    AssetHandle load(const std::string& path);

    // Sync load: blocks until the asset is loaded or fails. Returns valid handle
    // on success, null handle on failure.
    template<typename T>
    AssetHandle load_sync(const std::string& path);

    // Batch loading with progress tracking.
    LoadBatchBuilder load_batch();

    // --- Resolution ---

    // Resolve handle to loaded asset. Returns nullptr if not loaded yet or wrong type.
    template<typename T>
    const T* get(AssetHandle handle) const;

    // Mutable access (for importers that need post-processing).
    template<typename T>
    T* get_mut(AssetHandle handle);

    // --- Status ---
    AssetStatus status(AssetHandle handle) const;
    bool is_loaded(AssetHandle handle) const;

    // --- Refcount management ---

    /// Increment refcount for an asset handle.
    void acquire(AssetHandle handle);

    /// Decrement refcount for an asset handle.
    void release(AssetHandle handle);

    /// Returns the current refcount for an asset handle (0 if not tracked).
    uint32_t refcount(AssetHandle handle) const;

    // --- Garbage collection ---

    /// Unload assets with zero refcount.
    /// Returns handles of assets that were unloaded.
    std::vector<AssetHandle> collect_garbage();

    /// Explicitly unload a single asset by handle.
    void unload(AssetHandle handle);

    // --- Hot reload ---
    void watch_for_changes(bool enable);

    // --- Drain completed events ---
    std::vector<AssetLoaded> drain_completed();

    // --- Accessors ---
    const std::filesystem::path& root() const { return m_root; }

private:
    // Internal asset entry
    struct AssetEntry {
        std::any data;
        AssetStatus status = AssetStatus::Loading;
        std::filesystem::path path;
        std::type_index type;

        AssetEntry(std::filesystem::path p, std::type_index t)
            : path(std::move(p)), type(t) {}
    };

    // Load request for the background thread queue
    struct LoadRequest {
        AssetHandle handle;
        std::filesystem::path full_path;
        std::type_index type;
    };

    AssetHandle next_handle();
    AssetHandle find_cached(std::type_index type, const std::string& path) const;
    void execute_load(const LoadRequest& request);
    void loader_thread_main(std::stop_token stop);
    void remove_from_path_cache(uint64_t packed_key);

    // --- Data ---
    std::filesystem::path m_root;

    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, AssetEntry> m_assets;  // keyed by handle.packed()

    // Path -> handle cache for deduplication: key = "TypeIndex:path"
    std::unordered_map<std::string, AssetHandle> m_path_cache;

    // Importers: type_index -> importer function
    std::unordered_map<std::type_index, ImporterFn> m_importers;

    // Refcount tracking: keyed by handle.packed()
    std::unordered_map<uint64_t, uint32_t> m_refcounts;

    // Background loading
    std::queue<LoadRequest> m_load_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::vector<std::jthread> m_loader_threads;

    // Completed loads (drained by main thread each frame)
    std::mutex m_completed_mutex;
    std::vector<AssetLoaded> m_completed;

    // Handle index generator (0 is reserved for null)
    std::atomic<uint32_t> m_next_index{1};

    bool m_watching = false;
};

// --- Template implementations ---

template<typename T>
void AssetServer::register_importer(ImporterFn importer) {
    std::lock_guard lock(m_mutex);
    m_importers[std::type_index(typeid(T))] = std::move(importer);
}

template<typename T>
AssetHandle AssetServer::load(const std::string& path) {
    auto type = std::type_index(typeid(T));

    // Check cache first
    {
        std::lock_guard lock(m_mutex);
        auto cached = find_cached(type, path);
        if (cached) return cached;
    }

    // Create entry and enqueue
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

    {
        std::lock_guard lock(m_queue_mutex);
        m_load_queue.push(LoadRequest{handle, full_path, type});
    }
    m_queue_cv.notify_one();

    return handle;
}

template<typename T>
AssetHandle AssetServer::load_sync(const std::string& path) {
    auto type = std::type_index(typeid(T));

    // Check cache first
    {
        std::lock_guard lock(m_mutex);
        auto cached = find_cached(type, path);
        if (cached) {
            uint64_t key = cached.packed();
            auto asset_it = m_assets.find(key);
            if (asset_it != m_assets.end() && asset_it->second.status == AssetStatus::Loaded) {
                return cached;
            }
        }
    }

    // Create entry and load immediately on the calling thread
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

    // Execute synchronously (not on a worker thread)
    execute_load(LoadRequest{handle, full_path, type});

    // Check if it succeeded
    {
        std::lock_guard lock(m_mutex);
        auto it = m_assets.find(key);
        if (it != m_assets.end() && it->second.status == AssetStatus::Failed) {
            return AssetHandle{};
        }
    }

    return handle;
}

template<typename T>
const T* AssetServer::get(AssetHandle handle) const {
    std::lock_guard lock(m_mutex);
    uint64_t key = handle.packed();
    auto it = m_assets.find(key);
    if (it == m_assets.end()) return nullptr;
    if (it->second.status != AssetStatus::Loaded) return nullptr;
    return std::any_cast<T>(&it->second.data);
}

template<typename T>
T* AssetServer::get_mut(AssetHandle handle) {
    std::lock_guard lock(m_mutex);
    uint64_t key = handle.packed();
    auto it = m_assets.find(key);
    if (it == m_assets.end()) return nullptr;
    if (it->second.status != AssetStatus::Loaded) return nullptr;
    return std::any_cast<T>(&it->second.data);
}

} // namespace helios

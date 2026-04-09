#pragma once

#include "helios/ecs/asset_handle.h"
#include "helios/assets/handle.h"

#include <algorithm>
#include <any>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <optional>
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

// Forward declaration for importer context
class AssetServer;

// Type-erased importer function: takes (asset_root / path, server) -> std::any
// Returns the loaded asset data or throws on failure.
// The server reference allows importers to create sub-assets (textures, materials).
using ImporterFn = std::function<std::any(const std::filesystem::path&, AssetServer&)>;

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
    // The returned Handle<T> holds one refcount. Copying increments it; destruction
    // decrements it. When refcount reaches zero, the asset becomes eligible for GC.
    template<typename T>
    Handle<T> load(const std::string& path);

    // Sync load: blocks until the asset is loaded or fails. Returns a valid
    // Handle<T> on success, or a null handle on failure.
    template<typename T>
    Handle<T> load_sync(const std::string& path);

    // Batch loading with progress tracking.
    LoadBatchBuilder load_batch();

    // --- Extension mapping ---

    // Register file extensions that map to asset type T.
    // Extensions are normalized: lowercased, leading dots stripped.
    // e.g. register_extensions<MeshAsset>({"gltf", "glb", ".GLTF"})
    template<typename T>
    void register_extensions(std::initializer_list<std::string> extensions);

    // Load an asset by inferring the type from the file extension.
    // Returns a raw AssetHandle (not typed). Use get<T>() to resolve.
    AssetHandle load_by_extension(const std::string& path);

    // Synchronous variant of load_by_extension.
    AssetHandle load_sync_by_extension(const std::string& path);

    // Query which type_index is registered for a given extension.
    // Returns std::nullopt if the extension is not registered.
    std::optional<std::type_index> type_for_extension(const std::string& ext) const;

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

    // --- Dependency tracking ---

    /// Register that `parent` depends on `child`. When parent's refcount
    /// reaches zero, child is automatically released. Multiple parents may
    /// depend on the same child (shared sub-assets).
    void add_dependency(AssetHandle parent, AssetHandle child);

    // --- Sub-asset creation ---

    /// Store an already-constructed asset and return a handle for it.
    /// Used by importers to create sub-assets (textures, materials).
    /// Note: returns raw AssetHandle (not Handle<T>) because importers
    /// typically store handles in asset structs without needing refcounting.
    template<typename T>
    AssetHandle store(const std::string& path, T asset);

    /// Allocate a fresh handle without loading -- for manual construction.
    AssetHandle allocate_handle();

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

    // Shared internal load: dispatches to async or sync path depending on flag.
    AssetHandle load_internal(std::type_index type, const std::string& path, bool sync);

    // Normalize a file extension: lowercase, strip leading dots.
    static std::string normalize_extension(const std::string& ext);

    // Extract extension from a file path (normalized).
    static std::string extract_extension(const std::string& path);

    // --- Data ---
    std::filesystem::path m_root;

    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, AssetEntry> m_assets;  // keyed by handle.packed()

    // Path -> handle cache for deduplication: key = "TypeIndex:path"
    std::unordered_map<std::string, AssetHandle> m_path_cache;

    // Importers: type_index -> importer function
    std::unordered_map<std::type_index, ImporterFn> m_importers;

    // Extension -> type_index mapping for load_by_extension
    std::unordered_map<std::string, std::type_index> m_extension_map;

    // Refcount tracking: keyed by handle.packed()
    std::unordered_map<uint64_t, uint32_t> m_refcounts;

    // Dependency tracking: parent -> list of children.
    // When parent refcount hits 0, all children are released.
    std::unordered_map<uint64_t, std::vector<AssetHandle>> m_dependencies;

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
void AssetServer::register_extensions(std::initializer_list<std::string> extensions) {
    auto type = std::type_index(typeid(T));
    std::lock_guard lock(m_mutex);
    for (const auto& ext : extensions) {
        m_extension_map.emplace(normalize_extension(ext), type);
    }
}

template<typename T>
Handle<T> AssetServer::load(const std::string& path) {
    auto raw = load_internal(std::type_index(typeid(T)), path, /*sync=*/false);
    if (!raw) return Handle<T>{};
    return Handle<T>(raw, this);
}

template<typename T>
Handle<T> AssetServer::load_sync(const std::string& path) {
    auto raw = load_internal(std::type_index(typeid(T)), path, /*sync=*/true);
    if (!raw) return Handle<T>{};
    return Handle<T>(raw, this);
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

template<typename T>
AssetHandle AssetServer::store(const std::string& path, T asset) {
    auto type = std::type_index(typeid(T));

    // Check cache first
    {
        std::lock_guard lock(m_mutex);
        auto cached = find_cached(type, path);
        if (cached) return cached;
    }

    auto handle = next_handle();
    auto full_path = m_root / path;
    uint64_t key = handle.packed();

    {
        std::lock_guard lock(m_mutex);
        auto [it, inserted] = m_assets.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(key),
            std::forward_as_tuple(full_path, type)
        );
        it->second.data = std::any(std::move(asset));
        it->second.status = AssetStatus::Loaded;

        std::string cache_key = std::string(type.name()) + ":" + path;
        m_path_cache[cache_key] = handle;
    }

    return handle;
}

} // namespace helios

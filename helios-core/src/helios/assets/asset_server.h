#pragma once

#include "helios/ecs/asset_handle.h"
#include "helios/assets/handle.h"
#include "helios/assets/asset_binary.h"

#include <algorithm>
#include <any>
#include <atomic>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
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

/// Import function: takes source file bytes + metadata, produces asset data bytes.
/// The AssetServer wraps the result in a Helios binary with GUID header.
using AssetImportFn = std::function<std::vector<uint8_t>(
    const std::vector<uint8_t>& source_bytes,
    const AssetMetadata& metadata,
    AssetServer& server)>;

/// Export function: takes asset binary data bytes + metadata, produces source file bytes.
using AssetExportFn = std::function<std::vector<uint8_t>(
    const std::vector<uint8_t>& asset_data,
    const AssetMetadata& metadata)>;

/// Import settings: describes what an importer supports (for UI).
struct AssetImportSettings {
    AssetBinaryType type = AssetBinaryType::Unknown;
    AssetMetadata default_metadata;
    std::vector<std::string> source_extensions;  // e.g. {"glb", "gltf", "fbx"}
    std::vector<std::string> export_formats;     // e.g. {"glb", "gltf"}
};

// Event emitted when an async load completes
struct AssetLoaded {
    AssetHandle handle;
    std::string path;
    bool success = true;
};

class ThreadPool;

class AssetServer {
public:
    explicit AssetServer(const std::filesystem::path& asset_root,
                         std::shared_ptr<ThreadPool> pool = nullptr);
    ~AssetServer();

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

    /// Get the path of an asset by its handle. Returns empty if handle is invalid.
    std::string get_path(AssetHandle handle) const {
        std::lock_guard lock(m_mutex);
        auto it = m_assets.find(handle.packed());
        if (it == m_assets.end()) return "";
        return it->second.path.string();
    }

    /// Change the asset root directory at runtime. Clears the path cache
    /// since cached paths are relative to the old root.
    void set_root(const std::filesystem::path& new_root);

    // --- Binary asset import/export pipeline ---

    /// Register an importer for a binary asset type.
    void register_asset_importer(AssetBinaryType type,
                                  AssetImportFn import_fn,
                                  AssetImportSettings settings);

    /// Register an exporter for a binary asset type + format.
    void register_asset_exporter(AssetBinaryType type,
                                  const std::string& format,
                                  AssetExportFn export_fn);

    /// Import a source file into a Helios binary asset.
    /// Generates a new GUID, calls the registered importer, writes the result.
    /// Returns the path to the created Helios asset (relative to root).
    std::string import_asset(const std::filesystem::path& source_path,
                              AssetBinaryType type,
                              const std::string& dest_relative_path,
                              const AssetMetadata& extra_metadata = {});

    /// Re-import: replace asset data from a new source. GUID preserved.
    void reimport_asset(const std::filesystem::path& asset_path,
                        const std::filesystem::path& new_source_path,
                        const AssetMetadata& extra_metadata = {});

    /// Export a Helios binary asset to an external format.
    std::vector<uint8_t> export_asset(const std::filesystem::path& asset_path,
                                       const std::string& format);

    /// Check if a Helios asset's source file has changed since import.
    /// Reads the asset binary header, extracts _source_path and _source_hash,
    /// then compares with the current source file hash.
    /// Returns false if the asset has no _source_path or the source is unchanged.
    bool is_source_outdated(const std::filesystem::path& asset_path) const;

    /// Get import settings for a type (for import modal UI).
    const AssetImportSettings* get_import_settings(AssetBinaryType type) const;

    /// Detect the asset type from a source file extension.
    std::optional<AssetBinaryType> detect_import_type(const std::string& extension) const;

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

    // Background loading via shared thread pool
    std::shared_ptr<ThreadPool> m_pool;

    // Completed loads (drained by main thread each frame)
    std::mutex m_completed_mutex;
    std::vector<AssetLoaded> m_completed;

    // Handle index generator (0 is reserved for null)
    std::atomic<uint32_t> m_next_index{1};

    bool m_watching = false;

    // Binary asset import/export pipeline
    std::unordered_map<uint32_t, AssetImportFn> m_asset_importers;       // AssetBinaryType -> import fn
    std::unordered_map<uint32_t, AssetImportSettings> m_import_settings; // AssetBinaryType -> settings
    std::unordered_map<std::string, AssetExportFn> m_asset_exporters;    // "type:format" -> export fn
    std::unordered_map<std::string, AssetBinaryType> m_ext_to_import_type; // "glb" -> Mesh
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

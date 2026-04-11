#include "helios/assets/asset_server.h"
#include "helios/assets/asset_binary.h"
#include "helios/assets/load_batch.h"
#include "helios/ecs/thread_pool.h"
#include "helios/core/engine_log_channels.h"
#include "helios/core/uuid.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <fstream>
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
    AssetHandle pending_sync_handle{};
    std::filesystem::path pending_sync_path;
    {
        std::lock_guard lock(m_mutex);
        auto cached = find_cached(type, path);
        if (cached) {
            uint64_t key = cached.packed();
            auto asset_it = m_assets.find(key);
            if (asset_it != m_assets.end()) {
                if (asset_it->second.status == AssetStatus::Loaded) {
                    return cached;
                }
                if (sync && asset_it->second.status == AssetStatus::Loading) {
                    // Sync requested but entry still Loading (e.g., queued by
                    // async preload with no worker threads). Will execute on
                    // calling thread after releasing the lock.
                    pending_sync_handle = cached;
                    pending_sync_path = m_root / path;
                }
            }
            if (!sync) {
                return cached;
            }
        }
    }
    // Execute outside lock scope to avoid deadlock
    if (pending_sync_handle) {
        execute_load(LoadRequest{pending_sync_handle, pending_sync_path, type});
        return pending_sync_handle;
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

    // Async path — submit to shared thread pool
    if (m_pool) {
        LoadRequest req{handle, full_path, type};
        m_pool->submit([this, req = std::move(req)]() { execute_load(req); });
    } else {
        // No pool — fall back to sync on calling thread
        execute_load(LoadRequest{handle, full_path, type});
    }

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
                         std::shared_ptr<ThreadPool> pool)
    : m_root(asset_root), m_pool(std::move(pool))
{
}

AssetServer::~AssetServer() = default;

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
    auto result = importer(request.full_path, *this);
    bool success = result.has_value();

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
    // NOTE: The main mutex is held for the entire cascading loop. This blocks
    // all concurrent loads / status queries while GC runs.  Acceptable for now
    // because the loop only touches in-memory maps and the number of
    // zero-refcount assets per frame is typically small.  If profiling shows
    // contention, split into: (1) collect handles under lock, (2) release
    // lock, (3) do heavy cleanup outside.
    std::lock_guard lock(m_mutex);
    std::vector<AssetHandle> unloaded;

    // Iterate until no more zero-refcount assets are found.
    // Cascading: releasing children may create new zero-refcount entries.
    bool found_any = true;
    while (found_any) {
        found_any = false;
        for (auto it = m_refcounts.begin(); it != m_refcounts.end(); ) {
            if (it->second == 0) {
                uint64_t key = it->first;

                // Skip assets that are still being loaded on a background
                // thread -- erasing them would leave the worker writing to
                // a dangling map entry.
                auto asset_it = m_assets.find(key);
                if (asset_it != m_assets.end() &&
                    asset_it->second.status == AssetStatus::Loading) {
                    ++it;
                    continue;
                }

                found_any = true;
                AssetHandle handle = AssetHandle::from_packed(key);
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
        // Do not erase an asset that is still being loaded on a background
        // thread -- the worker holds a reference to this map entry.
        if (asset_it->second.status == AssetStatus::Loading) {
            HELIOS_LOG(Assets, Warn,
                       "Cannot unload asset (handle {}/{}) -- still loading",
                       handle.index, handle.generation);
            return;
        }
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
    // m_completed is guaranteed empty after std::move; no need to clear().
    return result;
}

LoadBatchBuilder AssetServer::load_batch() {
    return LoadBatchBuilder(*this);
}

void AssetServer::set_root(const std::filesystem::path& new_root) {
    std::lock_guard lock(m_mutex);
    m_root = new_root;
    m_path_cache.clear();
}

// ---- Binary asset import/export pipeline ----

static std::string compute_source_hash(const std::vector<uint8_t>& bytes) {
    // Simple hash: size + FNV-1a of first 4KB
    size_t hash = 14695981039346656037ULL; // FNV offset basis
    size_t len = std::min(bytes.size(), size_t(4096));
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL; // FNV prime
    }
    return std::to_string(bytes.size()) + ":" + std::to_string(hash);
}

static std::vector<uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        HELIOS_LOG(Assets, Error, "Cannot open file: {}", path.string());
        return {};
    }
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

static bool write_file_bytes(const std::filesystem::path& path,
                             const std::vector<uint8_t>& bytes) {
    // Ensure parent directory exists
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) {
        HELIOS_LOG(Assets, Error, "Cannot write file: {}", path.string());
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    return true;
}

void AssetServer::register_asset_importer(AssetBinaryType type,
                                           AssetImportFn import_fn,
                                           AssetImportSettings settings) {
    auto key = static_cast<uint32_t>(type);
    m_asset_importers[key] = std::move(import_fn);

    // Map each source extension to this type
    for (const auto& ext : settings.source_extensions) {
        m_ext_to_import_type[normalize_extension(ext)] = type;
    }

    settings.type = type;
    m_import_settings[key] = std::move(settings);
}

void AssetServer::register_asset_exporter(AssetBinaryType type,
                                           const std::string& format,
                                           AssetExportFn export_fn) {
    std::string key = std::to_string(static_cast<uint32_t>(type)) + ":" + format;
    m_asset_exporters[key] = std::move(export_fn);
}

std::string AssetServer::import_asset(const std::filesystem::path& source_path,
                                       AssetBinaryType type,
                                       const std::string& dest_relative_path,
                                       const AssetMetadata& extra_metadata) {
    // 1. Read source file bytes
    std::filesystem::path full_source = source_path.is_absolute()
        ? source_path
        : m_root / source_path;
    auto source_bytes = read_file_bytes(full_source);
    if (source_bytes.empty()) {
        HELIOS_LOG(Assets, Error, "import_asset: failed to read source '{}'", full_source.string());
        return {};
    }

    // 2. Look up importer
    auto type_key = static_cast<uint32_t>(type);
    auto it = m_asset_importers.find(type_key);
    if (it == m_asset_importers.end()) {
        HELIOS_LOG(Assets, Error, "No importer registered for asset type {}", type_key);
        return {};
    }

    // 3. Build metadata: merge defaults + extra + system fields
    AssetMetadata metadata;
    auto settings_it = m_import_settings.find(type_key);
    if (settings_it != m_import_settings.end()) {
        metadata = settings_it->second.default_metadata;
    }
    for (const auto& [k, v] : extra_metadata) {
        metadata[k] = v;
    }

    // Compute relative source path for metadata
    std::string source_relative = source_path.is_absolute()
        ? std::filesystem::relative(source_path, m_root).string()
        : source_path.string();
    metadata["_source_path"] = source_relative;

    // Source hash
    metadata["_source_hash"] = compute_source_hash(source_bytes);

    // Import time
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    metadata["_import_time"] = std::to_string(epoch);

    // 4. Call importer
    auto asset_data = it->second(source_bytes, metadata, *this);

    // 5. Generate UUID
    UUID guid = UUID::generate();

    // 6. Build header and write
    AssetBinaryHeader header;
    header.guid = guid;
    header.type = type;
    header.version = 1;
    header.metadata = std::move(metadata);

    auto file_bytes = write_asset_binary(header, asset_data);

    // 7. Write to dest path
    auto dest_full = m_root / dest_relative_path;
    write_file_bytes(dest_full, file_bytes);

    HELIOS_LOG(Assets, Info, "Imported '{}' -> '{}' (GUID: {})",
               source_relative, dest_relative_path, guid.to_string());

    return dest_relative_path;
}

void AssetServer::reimport_asset(const std::filesystem::path& asset_path,
                                  const std::filesystem::path& new_source_path,
                                  const AssetMetadata& extra_metadata) {
    // 1. Read existing Helios binary to extract header (preserve GUID + user metadata)
    auto full_asset = asset_path.is_absolute()
        ? asset_path
        : m_root / asset_path;
    auto existing_bytes = read_file_bytes(full_asset);
    if (existing_bytes.empty()) {
        HELIOS_LOG(Assets, Error, "reimport_asset: failed to read '{}'", full_asset.string());
        return;
    }

    auto existing = read_asset_binary(existing_bytes.data(), existing_bytes.size());
    if (!existing) {
        HELIOS_LOG(Assets, Error, "Cannot read existing asset binary: {}", full_asset.string());
        return;
    }

    auto& old_header = existing->header;

    // 2. Read new source bytes
    std::filesystem::path full_source = new_source_path.is_absolute()
        ? new_source_path
        : m_root / new_source_path;
    auto source_bytes = read_file_bytes(full_source);
    if (source_bytes.empty()) {
        HELIOS_LOG(Assets, Error, "reimport_asset: failed to read source '{}'", full_source.string());
        return;
    }

    // 3. Look up importer by header.type
    auto type_key = static_cast<uint32_t>(old_header.type);
    auto it = m_asset_importers.find(type_key);
    if (it == m_asset_importers.end()) {
        HELIOS_LOG(Assets, Error, "No importer registered for asset type {}", type_key);
        return;
    }

    // 4. Build merged metadata: keep existing, update system fields + extras
    AssetMetadata metadata = old_header.metadata;
    for (const auto& [k, v] : extra_metadata) {
        metadata[k] = v;
    }

    std::string source_relative = new_source_path.is_absolute()
        ? std::filesystem::relative(new_source_path, m_root).string()
        : new_source_path.string();
    metadata["_source_path"] = source_relative;
    metadata["_source_hash"] = compute_source_hash(source_bytes);

    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    metadata["_import_time"] = std::to_string(epoch);

    // 5. Call importer with new source bytes
    auto asset_data = it->second(source_bytes, metadata, *this);

    // 6. Write back with same GUID
    AssetBinaryHeader header;
    header.guid = old_header.guid;
    header.type = old_header.type;
    header.version = old_header.version;
    header.metadata = std::move(metadata);

    auto file_bytes = write_asset_binary(header, asset_data);
    write_file_bytes(full_asset, file_bytes);

    HELIOS_LOG(Assets, Info, "Re-imported '{}' from '{}' (GUID: {})",
               full_asset.string(), source_relative,
               old_header.guid.to_string());
}

std::vector<uint8_t> AssetServer::export_asset(
    const std::filesystem::path& asset_path,
    const std::string& format) {
    // 1. Read Helios binary
    auto full_path = asset_path.is_absolute()
        ? asset_path
        : m_root / asset_path;
    auto file_bytes = read_file_bytes(full_path);
    if (file_bytes.empty()) {
        HELIOS_LOG(Assets, Error, "export_asset: failed to read '{}'", full_path.string());
        return {};
    }

    auto result = read_asset_binary(file_bytes.data(), file_bytes.size());
    if (!result) {
        HELIOS_LOG(Assets, Error, "Cannot read asset binary: {}", full_path.string());
        return {};
    }

    // 2. Look up exporter
    auto type_key = static_cast<uint32_t>(result->header.type);
    std::string exporter_key = std::to_string(type_key) + ":" + format;
    auto it = m_asset_exporters.find(exporter_key);
    if (it == m_asset_exporters.end()) {
        HELIOS_LOG(Assets, Error, "No exporter registered for type {} format '{}'", type_key, format);
        return {};
    }

    // 3. Extract payload data bytes from the reader
    std::vector<uint8_t> data_bytes(result->data.remaining());
    if (!data_bytes.empty()) {
        result->data.read_bytes(data_bytes.data(), data_bytes.size());
    }

    // 4. Call exporter
    return it->second(data_bytes, result->header.metadata);
}

bool AssetServer::is_source_outdated(const std::filesystem::path& asset_path) const {
    // 1. Read the asset binary header from m_root / asset_path
    auto full_path = asset_path.is_absolute()
        ? asset_path
        : m_root / asset_path;

    auto file_bytes = read_file_bytes(full_path);
    if (file_bytes.empty()) return false; // Cannot read asset file

    auto header = read_asset_header(file_bytes.data(), file_bytes.size());
    if (!header) return false; // Not a valid Helios binary

    // 2. Get _source_path and _source_hash from metadata
    auto src_it = header->metadata.find("_source_path");
    if (src_it == header->metadata.end() || src_it->second.empty()) {
        return false; // No source path recorded
    }

    auto hash_it = header->metadata.find("_source_hash");
    if (hash_it == header->metadata.end()) {
        return false; // No hash to compare against
    }

    // 3. Read the source file and compute its current hash
    auto source_full = m_root / src_it->second;
    if (!std::filesystem::exists(source_full)) {
        return false; // Source file no longer exists — cannot determine
    }

    auto source_bytes = read_file_bytes(source_full);
    if (source_bytes.empty()) return false; // Cannot read source

    // 4. Compare hashes
    std::string current_hash = compute_source_hash(source_bytes);
    return current_hash != hash_it->second;
}

const AssetImportSettings* AssetServer::get_import_settings(
    AssetBinaryType type) const {
    auto it = m_import_settings.find(static_cast<uint32_t>(type));
    if (it == m_import_settings.end()) return nullptr;
    return &it->second;
}

std::optional<AssetBinaryType> AssetServer::detect_import_type(
    const std::string& extension) const {
    auto it = m_ext_to_import_type.find(normalize_extension(extension));
    if (it == m_ext_to_import_type.end()) return std::nullopt;
    return it->second;
}

} // namespace helios

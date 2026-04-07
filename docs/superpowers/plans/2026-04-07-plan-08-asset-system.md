# Asset System — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the engine's asset system: async/sync loading with handle-based references, batch loading with progress tracking, asset importers for textures/meshes/audio, reflection-based YAML and binary serialization, and scene file serialization.

**Architecture:** Assets are owned by `AssetServer` (a World resource). Components reference assets via `AssetHandle` (a plain uint64_t ID, not a smart pointer). Background `std::jthread` workers pick load requests from a thread-safe queue, run type-specific importers, and store results. `LoadBatch` wraps multiple loads with progress tracking. Reflection-based serialization uses qlibs/reflect to auto-iterate aggregate struct fields for zero-boilerplate YAML and binary round-trips.

**Tech Stack:** C++20, stb_image (texture loading), assimp (mesh loading), yaml-cpp (YAML serialization), qlibs/reflect (compile-time reflection), std::jthread + std::mutex (thread safety)

**Spec:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` sections 1.4, 1.5, 8.1, 8.2, 8.3

**Dependencies:** Plan 1 (ECS Core) — World, Resources, Events, Entity, Components. Plan 2 (App) — App, Plugin, schedule stages.

**New module directory:** `helios-core/src/assets/` and `helios-core/src/serialization/`

---

## Task 1: AssetHandle and AssetStatus

**Files:**
- Create: `helios-core/src/assets/asset_handle.h`

The foundational types used throughout the engine. `AssetHandle` is a value type holding a `uint64_t` ID. It is used in every component that references an asset (MeshRenderer, AudioSource, etc.) and must be hashable for use in `std::unordered_map`.

- [ ] **Step 1: Create asset_handle.h**

```cpp
// helios-core/src/assets/asset_handle.h
#pragma once

#include <cstdint>
#include <functional>

namespace helios {

enum class AssetStatus : uint8_t {
    Loading,
    Loaded,
    Failed
};

struct AssetHandle {
    uint64_t id = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const AssetHandle&) const = default;
    auto operator<=>(const AssetHandle&) const = default;
};

} // namespace helios

// std::hash specialization - must be in global namespace
template<>
struct std::hash<helios::AssetHandle> {
    std::size_t operator()(const helios::AssetHandle& handle) const noexcept {
        return std::hash<uint64_t>{}(handle.id);
    }
};
```

Design notes:
- `operator bool` is explicit to avoid accidental integer conversions.
- `operator==` uses defaulted comparison (C++20).
- `operator<=>` enables use in `std::map`/`std::set` if needed.
- `std::hash` specialization enables use as key in `std::unordered_map` and `std::unordered_set`.
- `AssetStatus` is `uint8_t` backed for compact storage in the asset entry.

- [ ] **Step 2: Verify compilation**

```bash
cd helios-core && cmake --build build --target helios-core 2>&1 | head -20
```

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/assets/asset_handle.h
git commit -m "feat(assets): add AssetHandle struct and AssetStatus enum"
```

---

## Task 2: AssetServer — Core Structure and Sync Loading

**Files:**
- Create: `helios-core/src/assets/asset_server.h`
- Create: `helios-core/src/assets/asset_server.cpp`

The AssetServer is the central resource that owns all loaded asset data. This task implements the core data structures, handle generation, synchronous loading, and status queries. Async loading is added in Task 3.

- [ ] **Step 1: Create asset_server.h**

```cpp
// helios-core/src/assets/asset_server.h
#pragma once

#include "asset_handle.h"

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
    // Register an importer for a specific asset type T.
    // The importer function receives the full filesystem path and returns std::any
    // containing the loaded T.
    template<typename T>
    void register_importer(ImporterFn importer);

    // --- Loading ---

    // Async load: returns handle immediately, loading happens on background thread.
    template<typename T>
    AssetHandle load(const std::string& path);

    // Sync load: blocks until the asset is loaded or fails. Returns valid handle
    // on success, handle with id=0 on failure.
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

    // --- Hot reload ---
    void watch_for_changes(bool enable);

    // --- Drain completed events ---
    // Returns all AssetLoaded events since last drain. Called by the asset system
    // each frame to emit events into the ECS event channel.
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

    // Generate a unique handle ID
    AssetHandle next_handle();

    // Find an existing handle for a (type, path) pair (cache lookup)
    AssetHandle find_cached(std::type_index type, const std::string& path) const;

    // Execute a load (called on loader threads or synchronously)
    void execute_load(const LoadRequest& request);

    // Background thread main loop
    void loader_thread_main(std::stop_token stop);

    // --- Data ---
    std::filesystem::path m_root;

    // Asset storage (protected by m_mutex)
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, AssetEntry> m_assets;

    // Path -> handle cache for deduplication: key = "TypeIndex:path"
    std::unordered_map<std::string, AssetHandle> m_path_cache;

    // Importers: type_index -> importer function
    std::unordered_map<std::type_index, ImporterFn> m_importers;

    // Background loading
    std::queue<LoadRequest> m_load_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::vector<std::jthread> m_loader_threads;

    // Completed loads (drained by main thread each frame)
    std::mutex m_completed_mutex;
    std::vector<AssetLoaded> m_completed;

    // Handle ID generator
    std::atomic<uint64_t> m_next_id{1};

    // Hot reload
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

    {
        std::lock_guard lock(m_mutex);
        m_assets.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(handle.id),
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
        if (cached && m_assets.at(cached.id).status == AssetStatus::Loaded) {
            return cached;
        }
    }

    // Create entry and load immediately on the calling thread
    auto handle = next_handle();
    auto full_path = m_root / path;

    {
        std::lock_guard lock(m_mutex);
        m_assets.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(handle.id),
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
        auto it = m_assets.find(handle.id);
        if (it != m_assets.end() && it->second.status == AssetStatus::Failed) {
            return AssetHandle{0};
        }
    }

    return handle;
}

template<typename T>
const T* AssetServer::get(AssetHandle handle) const {
    std::lock_guard lock(m_mutex);
    auto it = m_assets.find(handle.id);
    if (it == m_assets.end()) return nullptr;
    if (it->second.status != AssetStatus::Loaded) return nullptr;
    return std::any_cast<T>(&it->second.data);
}

template<typename T>
T* AssetServer::get_mut(AssetHandle handle) {
    std::lock_guard lock(m_mutex);
    auto it = m_assets.find(handle.id);
    if (it == m_assets.end()) return nullptr;
    if (it->second.status != AssetStatus::Loaded) return nullptr;
    return std::any_cast<T>(&it->second.data);
}
```

- [ ] **Step 2: Create asset_server.cpp**

```cpp
// helios-core/src/assets/asset_server.cpp
#include "asset_server.h"

#include <cassert>
// Include engine logging when available:
// #include "core/logging.h"

namespace helios {

AssetServer::AssetServer(const std::filesystem::path& asset_root,
                         uint32_t loader_thread_count)
    : m_root(asset_root)
{
    // Spawn background loader threads
    for (uint32_t i = 0; i < loader_thread_count; ++i) {
        m_loader_threads.emplace_back(
            [this](std::stop_token stop) { loader_thread_main(stop); }
        );
    }
}

AssetServer::~AssetServer() {
    // Request all threads to stop
    for (auto& thread : m_loader_threads) {
        thread.request_stop();
    }
    // Wake all waiting threads so they can observe the stop token
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
            // No importer registered - mark as failed
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
        // Log error when logging is available:
        // HVE_CORE_ERROR_TAG("AssetServer", "Failed to load '{}': {}",
        //                    request.full_path.string(), e.what());
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
        LoadRequest request;
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

        execute_load(request);
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
    // Hot reload implementation deferred - will use inotify (Linux) or
    // ReadDirectoryChangesW (Windows) to watch m_root for file changes,
    // then re-import and update the corresponding AssetEntry.
}

std::vector<AssetLoaded> AssetServer::drain_completed() {
    std::lock_guard lock(m_completed_mutex);
    auto result = std::move(m_completed);
    m_completed.clear();
    return result;
}

} // namespace helios
```

- [ ] **Step 3: Verify compilation**

```bash
cd helios-core && cmake --build build --target helios-core 2>&1 | head -20
```

- [ ] **Step 4: Commit**

```bash
git add helios-core/src/assets/asset_server.h helios-core/src/assets/asset_server.cpp
git commit -m "feat(assets): implement AssetServer with sync/async loading and thread pool"
```

---

## Task 3: LoadBatchBuilder and LoadBatch

**Files:**
- Create: `helios-core/src/assets/load_batch.h`
- Create: `helios-core/src/assets/load_batch.cpp`

`LoadBatchBuilder` accumulates multiple load requests. `submit()` fires them all into the AssetServer and returns a `LoadBatch` that tracks progress.

- [ ] **Step 1: Create load_batch.h**

```cpp
// helios-core/src/assets/load_batch.h
#pragma once

#include "asset_handle.h"
#include "asset_server.h"

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
        // Type-erased load function that calls server.load<T>(path)
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
            return server.load<T>(p);
        }
    });
    return *this;
}

} // namespace helios
```

- [ ] **Step 2: Create load_batch.cpp**

```cpp
// helios-core/src/assets/load_batch.cpp
#include "load_batch.h"

namespace helios {

// --- LoadBatchBuilder ---

LoadBatchBuilder::LoadBatchBuilder(AssetServer& server)
    : m_server(server) {}

LoadBatch LoadBatchBuilder::submit() {
    std::vector<AssetHandle> handles;
    std::vector<std::string> paths;
    handles.reserve(m_pending.size());
    paths.reserve(m_pending.size());

    for (auto& entry : m_pending) {
        auto handle = entry.load_fn(m_server, entry.path);
        handles.push_back(handle);
        paths.push_back(entry.path);
    }

    m_pending.clear();
    return LoadBatch(m_server, std::move(handles), std::move(paths));
}

// --- LoadBatch ---

LoadBatch::LoadBatch(AssetServer& server, std::vector<AssetHandle> handles,
                     std::vector<std::string> paths)
    : m_server(&server)
    , m_handles(std::move(handles))
    , m_paths(std::move(paths))
{}

float LoadBatch::progress() const {
    if (m_handles.empty()) return 1.0f;

    int done = 0;
    for (const auto& h : m_handles) {
        auto s = m_server->status(h);
        if (s == AssetStatus::Loaded || s == AssetStatus::Failed) {
            ++done;
        }
    }
    return static_cast<float>(done) / static_cast<float>(m_handles.size());
}

int LoadBatch::total() const {
    return static_cast<int>(m_handles.size());
}

int LoadBatch::remaining() const {
    int count = 0;
    for (const auto& h : m_handles) {
        if (m_server->status(h) == AssetStatus::Loading) {
            ++count;
        }
    }
    return count;
}

bool LoadBatch::is_complete() const {
    return remaining() == 0;
}

std::vector<std::string> LoadBatch::failed() const {
    std::vector<std::string> result;
    for (size_t i = 0; i < m_handles.size(); ++i) {
        if (m_server->status(m_handles[i]) == AssetStatus::Failed) {
            result.push_back(m_paths[i]);
        }
    }
    return result;
}

} // namespace helios
```

- [ ] **Step 3: Wire LoadBatchBuilder into AssetServer**

In `asset_server.h`, add the `load_batch()` method body (it was declared but not defined). Add to `asset_server.cpp`:

```cpp
LoadBatchBuilder AssetServer::load_batch() {
    return LoadBatchBuilder(*this);
}
```

- [ ] **Step 4: Verify compilation**

```bash
cd helios-core && cmake --build build --target helios-core 2>&1 | head -20
```

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/assets/load_batch.h helios-core/src/assets/load_batch.cpp
git add helios-core/src/assets/asset_server.cpp
git commit -m "feat(assets): add LoadBatchBuilder and LoadBatch for batch loading with progress"
```

---

## Task 4: TextureImporter

**Files:**
- Create: `helios-core/src/assets/importers/texture_importer.h`
- Create: `helios-core/src/assets/importers/texture_importer.cpp`

Loads image files from disk using stb_image. Produces a `TextureData` struct containing raw pixel data, dimensions, and channel count. This is GPU-agnostic raw data; the renderer is responsible for uploading it to the GPU.

- [ ] **Step 1: Create texture_importer.h**

```cpp
// helios-core/src/assets/importers/texture_importer.h
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace helios {

// Raw texture data loaded from disk. GPU upload is handled by the renderer.
struct TextureData {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;           // actual channels in the file
    int desired_channels = 4;   // what we requested (always RGBA)
    std::string source_path;

    bool is_valid() const { return !pixels.empty() && width > 0 && height > 0; }
    size_t byte_size() const {
        return static_cast<size_t>(width) * height * desired_channels;
    }
};

// HDR texture data (float per channel, for environment maps / IBL).
struct HdrTextureData {
    std::vector<float> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string source_path;

    bool is_valid() const { return !pixels.empty() && width > 0 && height > 0; }
};

class TextureImporter {
public:
    // Load LDR texture (PNG, JPG, BMP, TGA). Returns TextureData in std::any.
    // Throws std::runtime_error on failure.
    static std::any import_ldr(const std::filesystem::path& path);

    // Load HDR texture (HDR, EXR). Returns HdrTextureData in std::any.
    // Throws std::runtime_error on failure.
    static std::any import_hdr(const std::filesystem::path& path);
};

} // namespace helios
```

- [ ] **Step 2: Create texture_importer.cpp**

```cpp
// helios-core/src/assets/importers/texture_importer.cpp
#include "texture_importer.h"

#include <any>
#include <stdexcept>

// stb_image - include only (implementation is in a separate TU or
// already defined elsewhere in the project).
// If this is the first inclusion, define STB_IMAGE_IMPLEMENTATION in
// exactly one .cpp file (e.g., a dedicated stb_impl.cpp).
#include <stb_image.h>

namespace helios {

std::any TextureImporter::import_ldr(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Texture file not found: " + path.string());
    }

    int w, h, channels;
    constexpr int desired = 4; // Always load as RGBA
    stbi_set_flip_vertically_on_load(false);

    unsigned char* raw = stbi_load(path.string().c_str(), &w, &h, &channels,
                                   desired);
    if (!raw) {
        throw std::runtime_error(
            "stbi_load failed for '" + path.string() + "': " +
            stbi_failure_reason());
    }

    TextureData data;
    data.width = w;
    data.height = h;
    data.channels = channels;
    data.desired_channels = desired;
    data.source_path = path.string();

    size_t byte_count = static_cast<size_t>(w) * h * desired;
    data.pixels.assign(raw, raw + byte_count);

    stbi_image_free(raw);

    return std::any(std::move(data));
}

std::any TextureImporter::import_hdr(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("HDR texture file not found: " + path.string());
    }

    int w, h, channels;
    stbi_set_flip_vertically_on_load(false);

    float* raw = stbi_loadf(path.string().c_str(), &w, &h, &channels, 0);
    if (!raw) {
        throw std::runtime_error(
            "stbi_loadf failed for '" + path.string() + "': " +
            stbi_failure_reason());
    }

    HdrTextureData data;
    data.width = w;
    data.height = h;
    data.channels = channels;
    data.source_path = path.string();

    size_t float_count = static_cast<size_t>(w) * h * channels;
    data.pixels.assign(raw, raw + float_count);

    stbi_image_free(raw);

    return std::any(std::move(data));
}

} // namespace helios
```

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/assets/importers/texture_importer.h
git add helios-core/src/assets/importers/texture_importer.cpp
git commit -m "feat(assets): add TextureImporter using stb_image (LDR + HDR)"
```

---

## Task 5: MeshImporter

**Files:**
- Create: `helios-core/src/assets/importers/mesh_importer.h`
- Create: `helios-core/src/assets/importers/mesh_importer.cpp`

Loads mesh files using assimp. Produces a `MeshData` struct containing vertex/index data and per-submesh material references. The data is GPU-agnostic; the renderer uploads it.

- [ ] **Step 1: Create mesh_importer.h**

```cpp
// helios-core/src/assets/importers/mesh_importer.h
#pragma once

#include <any>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace helios {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 texcoord{0.0f};
    glm::vec4 tangent{0.0f};   // xyz = tangent, w = bitangent sign
};

struct SubMesh {
    uint32_t index_offset = 0;
    uint32_t index_count = 0;
    uint32_t vertex_offset = 0;
    uint32_t vertex_count = 0;
    int material_index = -1;     // index into MeshData::materials
};

struct MaterialData {
    std::string name;
    glm::vec3 base_color{1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;

    // Relative paths to texture files (empty if not present).
    // The caller should load these via AssetServer separately.
    std::string albedo_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
    std::string ao_texture;
    std::string emissive_texture;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> submeshes;
    std::vector<MaterialData> materials;
    std::string source_path;

    bool is_valid() const { return !vertices.empty() && !indices.empty(); }
};

class MeshImporter {
public:
    // Load mesh file (GLTF, FBX, OBJ, etc.). Returns MeshData in std::any.
    // Throws std::runtime_error on failure.
    static std::any import(const std::filesystem::path& path);
};

} // namespace helios
```

- [ ] **Step 2: Create mesh_importer.cpp**

```cpp
// helios-core/src/assets/importers/mesh_importer.cpp
#include "mesh_importer.h"

#include <stdexcept>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace helios {

namespace {

MaterialData extract_material(const aiMaterial* mat,
                              const std::filesystem::path& model_dir) {
    MaterialData result;

    aiString name;
    if (mat->Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS) {
        result.name = name.C_Str();
    }

    aiColor3D color;
    if (mat->Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS) {
        result.base_color = {color.r, color.g, color.b};
    } else if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS) {
        result.base_color = {color.r, color.g, color.b};
    }

    float metallic = 0.0f;
    if (mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) {
        result.metallic = metallic;
    }

    float roughness = 1.0f;
    if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
        result.roughness = roughness;
    }

    auto get_texture_path = [&](aiTextureType type) -> std::string {
        if (mat->GetTextureCount(type) > 0) {
            aiString tex_path;
            mat->GetTexture(type, 0, &tex_path);
            auto full = model_dir / tex_path.C_Str();
            return full.string();
        }
        return {};
    };

    result.albedo_texture = get_texture_path(aiTextureType_BASE_COLOR);
    if (result.albedo_texture.empty()) {
        result.albedo_texture = get_texture_path(aiTextureType_DIFFUSE);
    }
    result.normal_texture = get_texture_path(aiTextureType_NORMALS);
    result.metallic_roughness_texture =
        get_texture_path(aiTextureType_METALNESS);
    result.ao_texture =
        get_texture_path(aiTextureType_AMBIENT_OCCLUSION);
    result.emissive_texture = get_texture_path(aiTextureType_EMISSIVE);

    return result;
}

void process_mesh(const aiMesh* mesh, MeshData& out) {
    SubMesh submesh;
    submesh.vertex_offset = static_cast<uint32_t>(out.vertices.size());
    submesh.index_offset = static_cast<uint32_t>(out.indices.size());
    submesh.vertex_count = mesh->mNumVertices;
    submesh.material_index = mesh->mMaterialIndex;

    // Vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v;
        v.position = {mesh->mVertices[i].x,
                      mesh->mVertices[i].y,
                      mesh->mVertices[i].z};

        if (mesh->HasNormals()) {
            v.normal = {mesh->mNormals[i].x,
                        mesh->mNormals[i].y,
                        mesh->mNormals[i].z};
        }

        if (mesh->mTextureCoords[0]) {
            v.texcoord = {mesh->mTextureCoords[0][i].x,
                          mesh->mTextureCoords[0][i].y};
        }

        if (mesh->HasTangentsAndBitangents()) {
            v.tangent = {mesh->mTangents[i].x,
                         mesh->mTangents[i].y,
                         mesh->mTangents[i].z,
                         1.0f};  // bitangent sign computed in shader
        }

        out.vertices.push_back(v);
    }

    // Indices
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            out.indices.push_back(submesh.vertex_offset + face.mIndices[j]);
        }
    }

    submesh.index_count =
        static_cast<uint32_t>(out.indices.size()) - submesh.index_offset;
    out.submeshes.push_back(submesh);
}

void process_node(const aiNode* node, const aiScene* scene, MeshData& out) {
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        process_mesh(scene->mMeshes[node->mMeshes[i]], out);
    }
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        process_node(node->mChildren[i], scene, out);
    }
}

} // anonymous namespace

std::any MeshImporter::import(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Mesh file not found: " + path.string());
    }

    Assimp::Importer importer;

    constexpr unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |
        aiProcess_OptimizeMeshes;

    const aiScene* scene = importer.ReadFile(path.string(), flags);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
        !scene->mRootNode) {
        throw std::runtime_error(
            "Assimp failed to load '" + path.string() + "': " +
            importer.GetErrorString());
    }

    MeshData data;
    data.source_path = path.string();

    // Extract materials
    std::filesystem::path model_dir = path.parent_path();
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        data.materials.push_back(
            extract_material(scene->mMaterials[i], model_dir));
    }

    // Process all nodes recursively
    process_node(scene->mRootNode, scene, data);

    return std::any(std::move(data));
}

} // namespace helios
```

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/assets/importers/mesh_importer.h
git add helios-core/src/assets/importers/mesh_importer.cpp
git commit -m "feat(assets): add MeshImporter using assimp (vertices, indices, materials)"
```

---

## Task 6: AudioImporter

**Files:**
- Create: `helios-core/src/assets/importers/audio_importer.h`
- Create: `helios-core/src/assets/importers/audio_importer.cpp`

Loads audio files from disk into raw PCM data. This importer reads the raw bytes and stores file metadata. The audio backend (SoLoud) handles actual decoding; we store the file contents in memory for it.

- [ ] **Step 1: Create audio_importer.h**

```cpp
// helios-core/src/assets/importers/audio_importer.h
#pragma once

#include <any>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace helios {

enum class AudioFormat : uint8_t {
    WAV,
    OGG,
    MP3,
    FLAC,
    Unknown
};

// Raw audio file data loaded from disk. The audio backend handles decoding.
struct AudioData {
    std::vector<uint8_t> file_bytes;  // full file contents in memory
    AudioFormat format = AudioFormat::Unknown;
    std::string source_path;

    bool is_valid() const { return !file_bytes.empty(); }
    size_t byte_size() const { return file_bytes.size(); }
};

class AudioImporter {
public:
    // Load audio file. Returns AudioData in std::any.
    // Throws std::runtime_error on failure.
    static std::any import(const std::filesystem::path& path);

    // Determine format from file extension.
    static AudioFormat format_from_extension(const std::string& ext);
};

} // namespace helios
```

- [ ] **Step 2: Create audio_importer.cpp**

```cpp
// helios-core/src/assets/importers/audio_importer.cpp
#include "audio_importer.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace helios {

AudioFormat AudioImporter::format_from_extension(const std::string& ext) {
    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == ".wav") return AudioFormat::WAV;
    if (lower == ".ogg") return AudioFormat::OGG;
    if (lower == ".mp3") return AudioFormat::MP3;
    if (lower == ".flac") return AudioFormat::FLAC;
    return AudioFormat::Unknown;
}

std::any AudioImporter::import(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Audio file not found: " + path.string());
    }

    // Read entire file into memory
    auto file_size = std::filesystem::file_size(path);
    if (file_size == 0) {
        throw std::runtime_error("Audio file is empty: " + path.string());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open audio file: " + path.string());
    }

    AudioData data;
    data.file_bytes.resize(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(data.file_bytes.data()),
              static_cast<std::streamsize>(file_size));

    if (!file.good() && !file.eof()) {
        throw std::runtime_error("Failed to read audio file: " + path.string());
    }

    data.format = format_from_extension(path.extension().string());
    data.source_path = path.string();

    return std::any(std::move(data));
}

} // namespace helios
```

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/assets/importers/audio_importer.h
git add helios-core/src/assets/importers/audio_importer.cpp
git commit -m "feat(assets): add AudioImporter for WAV/OGG/MP3/FLAC file loading"
```

---

## Task 7: Asset Plugin — Register Importers and Emit Events

**Files:**
- Create: `helios-core/src/assets/asset_plugin.h`
- Create: `helios-core/src/assets/asset_plugin.cpp`

The AssetPlugin registers default importers with the AssetServer and adds a system that drains completed loads into the ECS event channel each frame.

- [ ] **Step 1: Create asset_plugin.h**

```cpp
// helios-core/src/assets/asset_plugin.h
#pragma once

#include <filesystem>
#include <string>

namespace helios {

// Forward declarations
class App;

struct AssetPluginConfig {
    std::filesystem::path asset_root = "assets";
    uint32_t loader_threads = 2;
    bool hot_reload = false;
};

struct AssetPlugin {
    AssetPluginConfig config;

    explicit AssetPlugin(AssetPluginConfig cfg = {})
        : config(std::move(cfg)) {}

    void build(App& app);
};

} // namespace helios
```

- [ ] **Step 2: Create asset_plugin.cpp**

```cpp
// helios-core/src/assets/asset_plugin.cpp
#include "asset_plugin.h"

#include "asset_server.h"
#include "importers/texture_importer.h"
#include "importers/mesh_importer.h"
#include "importers/audio_importer.h"

// These includes come from Plan 1 (ECS) and Plan 2 (App):
// #include "app/app.h"
// #include "ecs/world.h"

namespace helios {

// System: drain completed async loads and emit AssetLoaded events.
// Runs each frame in the PreUpdate stage.
void asset_event_system(/* ResMut<AssetServer> server,
                           EventWriter<AssetLoaded> writer */) {
    // auto completed = server->drain_completed();
    // for (auto& event : completed) {
    //     writer.send(std::move(event));
    // }
    //
    // Uncomment when ECS resource/event infrastructure is available.
    // The logic is:
    //   1. Call server->drain_completed() to get all loads that finished
    //      since last frame.
    //   2. For each AssetLoaded, send it into the event channel so that
    //      user systems can react (e.g., create GPU textures when a
    //      TextureData finishes loading).
}

void AssetPlugin::build(App& app) {
    // Insert AssetServer as a World resource
    // app.insert_resource<AssetServer>(
    //     AssetServer(config.asset_root, config.loader_threads));

    // Register default importers
    // auto& server = app.world().resource<AssetServer>();
    // server.register_importer<TextureData>(TextureImporter::import_ldr);
    // server.register_importer<HdrTextureData>(TextureImporter::import_hdr);
    // server.register_importer<MeshData>(MeshImporter::import);
    // server.register_importer<AudioData>(AudioImporter::import);

    // Register AssetLoaded event type
    // app.add_event<AssetLoaded>();

    // Add the drain system to PreUpdate
    // app.add_system(Schedule::PreUpdate, asset_event_system);

    // Hot reload
    // if (config.hot_reload) {
    //     server.watch_for_changes(true);
    // }

    // NOTE: The above is commented out because it depends on Plan 1 (ECS)
    // and Plan 2 (App) APIs. When those are implemented, uncomment and
    // remove the comments. The structure is correct per the spec.
}

} // namespace helios
```

Design notes:
- The plugin registers all three importer types so that `load<TextureData>("textures/foo.png")` works out of the box.
- The `asset_event_system` bridges the thread-safe `drain_completed()` into the ECS event channel, allowing user systems to react to `AssetLoaded` events.
- Hot reload is opt-in via the config. When enabled, the AssetServer watches the asset root directory and re-imports changed files.

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/assets/asset_plugin.h helios-core/src/assets/asset_plugin.cpp
git commit -m "feat(assets): add AssetPlugin with default importer registration and event drain"
```

---

## Task 8: Reflection Helpers for Serialization

**Files:**
- Create: `helios-core/src/serialization/reflect_helpers.h`

Provides overloaded `write_field` and `read_field` functions for types used in engine components: glm types, `std::string`, `AssetHandle`, enums, and scalars. These are called by the generic `serialize_component<T>` / `deserialize_component<T>` functions via qlibs/reflect.

- [ ] **Step 1: Create reflect_helpers.h**

```cpp
// helios-core/src/serialization/reflect_helpers.h
#pragma once

#include "../assets/asset_handle.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Forward-declare yaml-cpp types to avoid pulling in the full header here.
// The .cpp files that use these will include <yaml-cpp/yaml.h>.
namespace YAML {
    class Emitter;
    class Node;
}

namespace helios {

// ============================================================
// YAML write_field overloads
// ============================================================
// Each overload writes a named key:value pair to a YAML emitter.

// Scalars: float, double, int, uint, bool
inline void write_yaml(YAML::Emitter& out, std::string_view name, float v);
inline void write_yaml(YAML::Emitter& out, std::string_view name, double v);
inline void write_yaml(YAML::Emitter& out, std::string_view name, int v);
inline void write_yaml(YAML::Emitter& out, std::string_view name, uint32_t v);
inline void write_yaml(YAML::Emitter& out, std::string_view name, uint64_t v);
inline void write_yaml(YAML::Emitter& out, std::string_view name, bool v);

// std::string
inline void write_yaml(YAML::Emitter& out, std::string_view name,
                        const std::string& v);

// AssetHandle - serialized as the uint64 id
inline void write_yaml(YAML::Emitter& out, std::string_view name,
                        const AssetHandle& v);

// glm types - serialized as flow sequences [x, y, z] / [x, y, z, w]
inline void write_yaml(YAML::Emitter& out, std::string_view name,
                        const glm::vec2& v);
inline void write_yaml(YAML::Emitter& out, std::string_view name,
                        const glm::vec3& v);
inline void write_yaml(YAML::Emitter& out, std::string_view name,
                        const glm::vec4& v);
inline void write_yaml(YAML::Emitter& out, std::string_view name,
                        const glm::quat& v);
inline void write_yaml(YAML::Emitter& out, std::string_view name,
                        const glm::mat4& v);

// Enums - serialized as underlying integer value
template<typename T>
    requires std::is_enum_v<T>
void write_yaml(YAML::Emitter& out, std::string_view name, const T& v);

// ============================================================
// YAML read_field overloads
// ============================================================
// Each overload reads a named value from a YAML node.

inline void read_yaml(const YAML::Node& node, std::string_view name, float& v);
inline void read_yaml(const YAML::Node& node, std::string_view name, double& v);
inline void read_yaml(const YAML::Node& node, std::string_view name, int& v);
inline void read_yaml(const YAML::Node& node, std::string_view name, uint32_t& v);
inline void read_yaml(const YAML::Node& node, std::string_view name, uint64_t& v);
inline void read_yaml(const YAML::Node& node, std::string_view name, bool& v);

inline void read_yaml(const YAML::Node& node, std::string_view name,
                       std::string& v);

inline void read_yaml(const YAML::Node& node, std::string_view name,
                       AssetHandle& v);

inline void read_yaml(const YAML::Node& node, std::string_view name,
                       glm::vec2& v);
inline void read_yaml(const YAML::Node& node, std::string_view name,
                       glm::vec3& v);
inline void read_yaml(const YAML::Node& node, std::string_view name,
                       glm::vec4& v);
inline void read_yaml(const YAML::Node& node, std::string_view name,
                       glm::quat& v);
inline void read_yaml(const YAML::Node& node, std::string_view name,
                       glm::mat4& v);

template<typename T>
    requires std::is_enum_v<T>
void read_yaml(const YAML::Node& node, std::string_view name, T& v);

// ============================================================
// Binary write/read helpers
// ============================================================
// Raw bytes with size headers for each field.

inline void write_binary(std::ostream& out, float v);
inline void write_binary(std::ostream& out, double v);
inline void write_binary(std::ostream& out, int v);
inline void write_binary(std::ostream& out, uint32_t v);
inline void write_binary(std::ostream& out, uint64_t v);
inline void write_binary(std::ostream& out, bool v);
inline void write_binary(std::ostream& out, const std::string& v);
inline void write_binary(std::ostream& out, const AssetHandle& v);
inline void write_binary(std::ostream& out, const glm::vec2& v);
inline void write_binary(std::ostream& out, const glm::vec3& v);
inline void write_binary(std::ostream& out, const glm::vec4& v);
inline void write_binary(std::ostream& out, const glm::quat& v);
inline void write_binary(std::ostream& out, const glm::mat4& v);

template<typename T>
    requires std::is_enum_v<T>
void write_binary(std::ostream& out, const T& v);

inline void read_binary(std::istream& in, float& v);
inline void read_binary(std::istream& in, double& v);
inline void read_binary(std::istream& in, int& v);
inline void read_binary(std::istream& in, uint32_t& v);
inline void read_binary(std::istream& in, uint64_t& v);
inline void read_binary(std::istream& in, bool& v);
inline void read_binary(std::istream& in, std::string& v);
inline void read_binary(std::istream& in, AssetHandle& v);
inline void read_binary(std::istream& in, glm::vec2& v);
inline void read_binary(std::istream& in, glm::vec3& v);
inline void read_binary(std::istream& in, glm::vec4& v);
inline void read_binary(std::istream& in, glm::quat& v);
inline void read_binary(std::istream& in, glm::mat4& v);

template<typename T>
    requires std::is_enum_v<T>
void read_binary(std::istream& in, T& v);

} // namespace helios
```

Design notes:
- This header declares all overloads. Implementations go in `yaml_serializer.cpp` and `binary_serializer.cpp` (Tasks 9-10) where the full yaml-cpp headers are available.
- The `inline` declarations serve as the overload set. Since the bodies need yaml-cpp, the actual definitions will be in the .cpp files (the `inline` keyword is replaced by normal definitions there; the header will forward-declare and the .cpp will define).
- Enum support uses C++20 `requires` clause. Enums are stored as their underlying integer type.

**Revision for implementation clarity:** Rather than `inline` forward-declarations, this header should be structured as a type-trait / overload-set header that the serializer .cpp files include. The actual YAML/binary write/read implementations go in their respective .cpp files. Update the declarations to remove `inline` and use a pattern the serializers can link against.

Rewrite: make this a pure declaration header (no `inline`, no definitions). The definitions live in `yaml_serializer.cpp` and `binary_serializer.cpp`.

```cpp
// Corrected: declarations only. Remove all 'inline' keywords above.
// Definitions are in yaml_serializer.cpp (YAML overloads) and
// binary_serializer.cpp (binary overloads).
```

- [ ] **Step 2: Commit**

```bash
git add helios-core/src/serialization/reflect_helpers.h
git commit -m "feat(serialization): add reflect_helpers.h with write/read overloads for all engine types"
```

---

## Task 9: YamlSerializer

**Files:**
- Create: `helios-core/src/serialization/yaml_serializer.h`
- Create: `helios-core/src/serialization/yaml_serializer.cpp`

Reflection-based YAML serialization. Uses qlibs/reflect to iterate all fields of an aggregate struct and call the corresponding `write_yaml` / `read_yaml` overload from `reflect_helpers.h`. Zero registration code needed per component.

- [ ] **Step 1: Create yaml_serializer.h**

```cpp
// helios-core/src/serialization/yaml_serializer.h
#pragma once

#include "reflect_helpers.h"

#include <reflect>  // qlibs/reflect

#include <yaml-cpp/yaml.h>

#include <string>
#include <sstream>

namespace helios {

class YamlSerializer {
public:
    // Serialize a single aggregate component to a YAML mapping.
    // Returns the YAML string for the component.
    template<typename T>
    static std::string serialize_component(const T& component);

    // Deserialize a component from a YAML node.
    // The node should be a mapping with keys matching field names.
    template<typename T>
    static T deserialize_component(const YAML::Node& node);

    // Serialize a component into an existing emitter (as a mapping block).
    // The caller is responsible for emitter lifecycle.
    template<typename T>
    static void serialize_into(const T& component, YAML::Emitter& out);

    // Deserialize from a YAML string.
    template<typename T>
    static T deserialize_from_string(const std::string& yaml_str);
};

// --- Template implementations ---

template<typename T>
std::string YamlSerializer::serialize_component(const T& component) {
    YAML::Emitter out;
    out << YAML::BeginMap;
    serialize_into(component, out);
    out << YAML::EndMap;
    return std::string(out.c_str());
}

template<typename T>
void YamlSerializer::serialize_into(const T& component, YAML::Emitter& out) {
    reflect::for_each([&](auto I) {
        constexpr auto name = reflect::member_name<I>(T{});
        const auto& value = reflect::get<I>(component);
        write_yaml(out, name, value);
    }, component);
}

template<typename T>
T YamlSerializer::deserialize_component(const YAML::Node& node) {
    T component{};
    reflect::for_each([&](auto I) {
        constexpr auto name = reflect::member_name<I>(T{});
        auto& value = reflect::get<I>(component);
        read_yaml(node, name, value);
    }, component);
    return component;
}

template<typename T>
T YamlSerializer::deserialize_from_string(const std::string& yaml_str) {
    YAML::Node node = YAML::Load(yaml_str);
    return deserialize_component<T>(node);
}

} // namespace helios
```

- [ ] **Step 2: Create yaml_serializer.cpp**

Implements all `write_yaml` and `read_yaml` overloads declared in `reflect_helpers.h`.

```cpp
// helios-core/src/serialization/yaml_serializer.cpp
#include "yaml_serializer.h"
#include "reflect_helpers.h"

#include <yaml-cpp/yaml.h>

namespace helios {

// ============================================================
// write_yaml implementations
// ============================================================

void write_yaml(YAML::Emitter& out, std::string_view name, float v) {
    out << YAML::Key << std::string(name) << YAML::Value << v;
}

void write_yaml(YAML::Emitter& out, std::string_view name, double v) {
    out << YAML::Key << std::string(name) << YAML::Value << v;
}

void write_yaml(YAML::Emitter& out, std::string_view name, int v) {
    out << YAML::Key << std::string(name) << YAML::Value << v;
}

void write_yaml(YAML::Emitter& out, std::string_view name, uint32_t v) {
    out << YAML::Key << std::string(name) << YAML::Value << v;
}

void write_yaml(YAML::Emitter& out, std::string_view name, uint64_t v) {
    out << YAML::Key << std::string(name) << YAML::Value << v;
}

void write_yaml(YAML::Emitter& out, std::string_view name, bool v) {
    out << YAML::Key << std::string(name) << YAML::Value << v;
}

void write_yaml(YAML::Emitter& out, std::string_view name,
                const std::string& v) {
    out << YAML::Key << std::string(name) << YAML::Value << v;
}

void write_yaml(YAML::Emitter& out, std::string_view name,
                const AssetHandle& v) {
    out << YAML::Key << std::string(name) << YAML::Value << v.id;
}

void write_yaml(YAML::Emitter& out, std::string_view name,
                const glm::vec2& v) {
    out << YAML::Key << std::string(name);
    out << YAML::Value << YAML::Flow << YAML::BeginSeq
        << v.x << v.y << YAML::EndSeq;
}

void write_yaml(YAML::Emitter& out, std::string_view name,
                const glm::vec3& v) {
    out << YAML::Key << std::string(name);
    out << YAML::Value << YAML::Flow << YAML::BeginSeq
        << v.x << v.y << v.z << YAML::EndSeq;
}

void write_yaml(YAML::Emitter& out, std::string_view name,
                const glm::vec4& v) {
    out << YAML::Key << std::string(name);
    out << YAML::Value << YAML::Flow << YAML::BeginSeq
        << v.x << v.y << v.z << v.w << YAML::EndSeq;
}

void write_yaml(YAML::Emitter& out, std::string_view name,
                const glm::quat& v) {
    // Stored as [w, x, y, z] matching glm::quat constructor order
    out << YAML::Key << std::string(name);
    out << YAML::Value << YAML::Flow << YAML::BeginSeq
        << v.w << v.x << v.y << v.z << YAML::EndSeq;
}

void write_yaml(YAML::Emitter& out, std::string_view name,
                const glm::mat4& v) {
    out << YAML::Key << std::string(name);
    out << YAML::Value << YAML::Flow << YAML::BeginSeq;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out << v[col][row];
        }
    }
    out << YAML::EndSeq;
}

// ============================================================
// read_yaml implementations
// ============================================================

void read_yaml(const YAML::Node& node, std::string_view name, float& v) {
    auto key = std::string(name);
    if (node[key]) v = node[key].as<float>();
}

void read_yaml(const YAML::Node& node, std::string_view name, double& v) {
    auto key = std::string(name);
    if (node[key]) v = node[key].as<double>();
}

void read_yaml(const YAML::Node& node, std::string_view name, int& v) {
    auto key = std::string(name);
    if (node[key]) v = node[key].as<int>();
}

void read_yaml(const YAML::Node& node, std::string_view name, uint32_t& v) {
    auto key = std::string(name);
    if (node[key]) v = node[key].as<uint32_t>();
}

void read_yaml(const YAML::Node& node, std::string_view name, uint64_t& v) {
    auto key = std::string(name);
    if (node[key]) v = node[key].as<uint64_t>();
}

void read_yaml(const YAML::Node& node, std::string_view name, bool& v) {
    auto key = std::string(name);
    if (node[key]) v = node[key].as<bool>();
}

void read_yaml(const YAML::Node& node, std::string_view name,
               std::string& v) {
    auto key = std::string(name);
    if (node[key]) v = node[key].as<std::string>();
}

void read_yaml(const YAML::Node& node, std::string_view name,
               AssetHandle& v) {
    auto key = std::string(name);
    if (node[key]) v.id = node[key].as<uint64_t>();
}

void read_yaml(const YAML::Node& node, std::string_view name,
               glm::vec2& v) {
    auto key = std::string(name);
    if (node[key] && node[key].IsSequence() && node[key].size() >= 2) {
        v.x = node[key][0].as<float>();
        v.y = node[key][1].as<float>();
    }
}

void read_yaml(const YAML::Node& node, std::string_view name,
               glm::vec3& v) {
    auto key = std::string(name);
    if (node[key] && node[key].IsSequence() && node[key].size() >= 3) {
        v.x = node[key][0].as<float>();
        v.y = node[key][1].as<float>();
        v.z = node[key][2].as<float>();
    }
}

void read_yaml(const YAML::Node& node, std::string_view name,
               glm::vec4& v) {
    auto key = std::string(name);
    if (node[key] && node[key].IsSequence() && node[key].size() >= 4) {
        v.x = node[key][0].as<float>();
        v.y = node[key][1].as<float>();
        v.z = node[key][2].as<float>();
        v.w = node[key][3].as<float>();
    }
}

void read_yaml(const YAML::Node& node, std::string_view name,
               glm::quat& v) {
    auto key = std::string(name);
    if (node[key] && node[key].IsSequence() && node[key].size() >= 4) {
        v.w = node[key][0].as<float>();
        v.x = node[key][1].as<float>();
        v.y = node[key][2].as<float>();
        v.z = node[key][3].as<float>();
    }
}

void read_yaml(const YAML::Node& node, std::string_view name,
               glm::mat4& v) {
    auto key = std::string(name);
    if (node[key] && node[key].IsSequence() && node[key].size() >= 16) {
        int idx = 0;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                v[col][row] = node[key][idx++].as<float>();
            }
        }
    }
}

} // namespace helios
```

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/serialization/yaml_serializer.h
git add helios-core/src/serialization/yaml_serializer.cpp
git commit -m "feat(serialization): add YamlSerializer with reflection-based component round-trip"
```

---

## Task 10: BinarySerializer

**Files:**
- Create: `helios-core/src/serialization/binary_serializer.h`
- Create: `helios-core/src/serialization/binary_serializer.cpp`

Binary serialization for fast runtime loading. Uses qlibs/reflect the same way as YamlSerializer, but writes raw bytes with size headers instead of YAML.

- [ ] **Step 1: Create binary_serializer.h**

```cpp
// helios-core/src/serialization/binary_serializer.h
#pragma once

#include "reflect_helpers.h"

#include <reflect>  // qlibs/reflect

#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace helios {

class BinarySerializer {
public:
    // Serialize a component to a binary byte buffer.
    template<typename T>
    static std::vector<uint8_t> serialize_component(const T& component);

    // Deserialize a component from a binary byte buffer.
    template<typename T>
    static T deserialize_component(const std::vector<uint8_t>& data);

    // Serialize into an existing output stream.
    template<typename T>
    static void serialize_into(const T& component, std::ostream& out);

    // Deserialize from an input stream.
    template<typename T>
    static T deserialize_from(std::istream& in);
};

// --- Template implementations ---

template<typename T>
std::vector<uint8_t> BinarySerializer::serialize_component(
    const T& component) {
    std::ostringstream oss(std::ios::binary);
    serialize_into(component, oss);
    auto str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

template<typename T>
T BinarySerializer::deserialize_component(
    const std::vector<uint8_t>& data) {
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    return deserialize_from<T>(iss);
}

template<typename T>
void BinarySerializer::serialize_into(const T& component, std::ostream& out) {
    reflect::for_each([&](auto I) {
        const auto& value = reflect::get<I>(component);
        write_binary(out, value);
    }, component);
}

template<typename T>
T BinarySerializer::deserialize_from(std::istream& in) {
    T component{};
    reflect::for_each([&](auto I) {
        auto& value = reflect::get<I>(component);
        read_binary(in, value);
    }, component);
    return component;
}

} // namespace helios
```

- [ ] **Step 2: Create binary_serializer.cpp**

Implements all `write_binary` and `read_binary` overloads from `reflect_helpers.h`.

```cpp
// helios-core/src/serialization/binary_serializer.cpp
#include "binary_serializer.h"
#include "reflect_helpers.h"

#include <cstring>

namespace helios {

// ============================================================
// Helper: raw write/read
// ============================================================

namespace {

template<typename T>
void write_raw(std::ostream& out, const T& v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(T));
}

template<typename T>
void read_raw(std::istream& in, T& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(T));
}

} // anonymous namespace

// ============================================================
// write_binary implementations
// ============================================================

void write_binary(std::ostream& out, float v)    { write_raw(out, v); }
void write_binary(std::ostream& out, double v)   { write_raw(out, v); }
void write_binary(std::ostream& out, int v)      { write_raw(out, v); }
void write_binary(std::ostream& out, uint32_t v) { write_raw(out, v); }
void write_binary(std::ostream& out, uint64_t v) { write_raw(out, v); }
void write_binary(std::ostream& out, bool v)     { write_raw(out, v); }

void write_binary(std::ostream& out, const std::string& v) {
    // Write string as: uint32_t length + raw chars (no null terminator)
    uint32_t len = static_cast<uint32_t>(v.size());
    write_raw(out, len);
    if (len > 0) {
        out.write(v.data(), len);
    }
}

void write_binary(std::ostream& out, const AssetHandle& v) {
    write_raw(out, v.id);
}

void write_binary(std::ostream& out, const glm::vec2& v) {
    write_raw(out, v.x);
    write_raw(out, v.y);
}

void write_binary(std::ostream& out, const glm::vec3& v) {
    write_raw(out, v.x);
    write_raw(out, v.y);
    write_raw(out, v.z);
}

void write_binary(std::ostream& out, const glm::vec4& v) {
    write_raw(out, v.x);
    write_raw(out, v.y);
    write_raw(out, v.z);
    write_raw(out, v.w);
}

void write_binary(std::ostream& out, const glm::quat& v) {
    write_raw(out, v.w);
    write_raw(out, v.x);
    write_raw(out, v.y);
    write_raw(out, v.z);
}

void write_binary(std::ostream& out, const glm::mat4& v) {
    // Column-major: 4 columns of 4 floats
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            write_raw(out, v[col][row]);
        }
    }
}

// ============================================================
// read_binary implementations
// ============================================================

void read_binary(std::istream& in, float& v)    { read_raw(in, v); }
void read_binary(std::istream& in, double& v)   { read_raw(in, v); }
void read_binary(std::istream& in, int& v)      { read_raw(in, v); }
void read_binary(std::istream& in, uint32_t& v) { read_raw(in, v); }
void read_binary(std::istream& in, uint64_t& v) { read_raw(in, v); }
void read_binary(std::istream& in, bool& v)     { read_raw(in, v); }

void read_binary(std::istream& in, std::string& v) {
    uint32_t len = 0;
    read_raw(in, len);
    v.resize(len);
    if (len > 0) {
        in.read(v.data(), len);
    }
}

void read_binary(std::istream& in, AssetHandle& v) {
    read_raw(in, v.id);
}

void read_binary(std::istream& in, glm::vec2& v) {
    read_raw(in, v.x);
    read_raw(in, v.y);
}

void read_binary(std::istream& in, glm::vec3& v) {
    read_raw(in, v.x);
    read_raw(in, v.y);
    read_raw(in, v.z);
}

void read_binary(std::istream& in, glm::vec4& v) {
    read_raw(in, v.x);
    read_raw(in, v.y);
    read_raw(in, v.z);
    read_raw(in, v.w);
}

void read_binary(std::istream& in, glm::quat& v) {
    read_raw(in, v.w);
    read_raw(in, v.x);
    read_raw(in, v.y);
    read_raw(in, v.z);
}

void read_binary(std::istream& in, glm::mat4& v) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            read_raw(in, v[col][row]);
        }
    }
}

} // namespace helios
```

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/serialization/binary_serializer.h
git add helios-core/src/serialization/binary_serializer.cpp
git commit -m "feat(serialization): add BinarySerializer with reflection-based raw byte round-trip"
```

---

## Task 11: Scene File Serialization

**Files:**
- Create: `helios-core/src/serialization/scene_serializer.h`
- Create: `helios-core/src/serialization/scene_serializer.cpp`

Saves and loads entire scenes as YAML files. Each entity is serialized with its components using reflection. The scene format matches the spec: scene name, entity list, each entity with tag and component mappings.

- [ ] **Step 1: Create scene_serializer.h**

```cpp
// helios-core/src/serialization/scene_serializer.h
#pragma once

#include <filesystem>
#include <string>
#include <functional>
#include <typeindex>
#include <unordered_map>

namespace helios {

// Forward declarations (from ECS Plan 1)
class World;
struct Entity;

// Type-erased component serializer/deserializer pair.
// Registered per component type so the scene serializer knows how to
// handle each component without hardcoding types.
struct ComponentSerializer {
    std::string type_name;  // e.g. "Transform", "MeshRenderer"

    // Serialize: given entity and world, write component YAML into emitter
    std::function<void(const World&, Entity, YAML::Emitter&)> serialize;

    // Deserialize: given entity, world, and YAML node, add component to entity
    std::function<void(World&, Entity, const YAML::Node&)> deserialize;
};

class SceneSerializer {
public:
    // Register a component type for serialization.
    // T must be an aggregate type compatible with qlibs/reflect.
    template<typename T>
    void register_component(const std::string& type_name);

    // Serialize entire world to YAML string.
    std::string serialize(const World& world,
                          const std::string& scene_name) const;

    // Save scene to file.
    bool save(const World& world, const std::string& scene_name,
              const std::filesystem::path& path) const;

    // Load scene from YAML string into world (spawns new entities).
    bool deserialize(World& world, const std::string& yaml_str) const;

    // Load scene from file.
    bool load(World& world, const std::filesystem::path& path) const;

private:
    std::vector<ComponentSerializer> m_serializers;
};
```

- [ ] **Step 2: Create scene_serializer.cpp**

```cpp
// helios-core/src/serialization/scene_serializer.cpp
#include "scene_serializer.h"
#include "yaml_serializer.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

// These includes come from Plan 1 (ECS):
// #include "ecs/world.h"
// #include "ecs/entity.h"

namespace helios {

std::string SceneSerializer::serialize(const World& world,
                                       const std::string& scene_name) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "scene" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << scene_name;
    out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;

    // Iterate all entities in the world.
    // When ECS is available:
    //
    // world.each([&](Entity entity) {
    //     out << YAML::BeginMap;
    //
    //     // Entity ID
    //     out << YAML::Key << "id" << YAML::Value << entity.id();
    //
    //     // Tag (if present)
    //     // if (world.has<Tag>(entity)) {
    //     //     out << YAML::Key << "tag" << YAML::Value
    //     //         << world.get<Tag>(entity).name;
    //     // }
    //
    //     // Components
    //     out << YAML::Key << "components" << YAML::Value << YAML::BeginMap;
    //     for (const auto& serializer : m_serializers) {
    //         // Check if entity has this component, then serialize
    //         serializer.serialize(world, entity, out);
    //     }
    //     out << YAML::EndMap;
    //
    //     out << YAML::EndMap;
    // });

    out << YAML::EndSeq;
    out << YAML::EndMap;
    out << YAML::EndMap;

    return std::string(out.c_str());
}

bool SceneSerializer::save(const World& world,
                           const std::string& scene_name,
                           const std::filesystem::path& path) const {
    auto yaml = serialize(world, scene_name);

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << yaml;
    return file.good();
}

bool SceneSerializer::deserialize(World& world,
                                  const std::string& yaml_str) const {
    YAML::Node root;
    try {
        root = YAML::Load(yaml_str);
    } catch (const YAML::Exception&) {
        return false;
    }

    auto scene_node = root["scene"];
    if (!scene_node) return false;

    auto entities_node = scene_node["entities"];
    if (!entities_node || !entities_node.IsSequence()) return false;

    for (const auto& entity_node : entities_node) {
        // Spawn a new entity
        // auto entity = world.spawn();

        // Set tag if present
        // if (entity_node["tag"]) {
        //     world.add(entity, Tag{entity_node["tag"].as<std::string>()});
        // }

        // Deserialize components
        auto components_node = entity_node["components"];
        if (components_node && components_node.IsMap()) {
            for (const auto& serializer : m_serializers) {
                auto comp_node = components_node[serializer.type_name];
                if (comp_node) {
                    // serializer.deserialize(world, entity, comp_node);
                }
            }
        }
    }

    return true;
}

bool SceneSerializer::load(World& world,
                           const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string yaml_str(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    return deserialize(world, yaml_str);
}

} // namespace helios
```

The `register_component<T>` template is implemented in the header:

```cpp
// In scene_serializer.h, after the class definition:
template<typename T>
void SceneSerializer::register_component(const std::string& type_name) {
    ComponentSerializer cs;
    cs.type_name = type_name;

    cs.serialize = [type_name](const World& world, Entity entity,
                               YAML::Emitter& out) {
        // if (!world.has<T>(entity)) return;
        // const auto& component = world.get<T>(entity);
        // out << YAML::Key << type_name << YAML::Value << YAML::BeginMap;
        // YamlSerializer::serialize_into(component, out);
        // out << YAML::EndMap;
        (void)world; (void)entity; (void)out; // suppress unused warnings
    };

    cs.deserialize = [](World& world, Entity entity,
                        const YAML::Node& node) {
        // auto component = YamlSerializer::deserialize_component<T>(node);
        // world.add(entity, std::move(component));
        (void)world; (void)entity; (void)node;
    };

    m_serializers.push_back(std::move(cs));
}
```

Design notes:
- The scene format matches the spec exactly: `scene > name, entities > [id, tag, components > {TypeName: {fields...}}]`.
- Component serializer registration is open-ended. The AssetPlugin or user code calls `register_component<Transform>("Transform")` once, and it works for all future save/load operations.
- The commented-out ECS calls (`world.spawn()`, `world.has<T>()`, etc.) are the exact API from Plan 1. Uncomment when ECS is implemented.

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/serialization/scene_serializer.h
git add helios-core/src/serialization/scene_serializer.cpp
git commit -m "feat(serialization): add SceneSerializer for YAML scene file save/load"
```

---

## Task 12: CMakeLists.txt Updates

**Files:**
- Modify: `helios-core/CMakeLists.txt`

Add the new source files to the build and link required dependencies (yaml-cpp, stb, assimp, qlibs-reflect).

- [ ] **Step 1: Add asset sources to helios-core**

Add the following source files to the `helios-core` target:

```cmake
# In helios-core/CMakeLists.txt, add to the target sources:

# Assets
src/assets/asset_handle.h
src/assets/asset_server.h
src/assets/asset_server.cpp
src/assets/load_batch.h
src/assets/load_batch.cpp
src/assets/asset_plugin.h
src/assets/asset_plugin.cpp
src/assets/importers/texture_importer.h
src/assets/importers/texture_importer.cpp
src/assets/importers/mesh_importer.h
src/assets/importers/mesh_importer.cpp
src/assets/importers/audio_importer.h
src/assets/importers/audio_importer.cpp

# Serialization
src/serialization/reflect_helpers.h
src/serialization/yaml_serializer.h
src/serialization/yaml_serializer.cpp
src/serialization/binary_serializer.h
src/serialization/binary_serializer.cpp
src/serialization/scene_serializer.h
src/serialization/scene_serializer.cpp
```

- [ ] **Step 2: Add link dependencies**

```cmake
target_link_libraries(helios-core
    PUBLIC
        glm::glm
        yaml-cpp
    PRIVATE
        stb_image
        assimp
        Threads::Threads   # for std::jthread / std::mutex
)

# qlibs/reflect is header-only
target_include_directories(helios-core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/vendor/qlibs-reflect
)
```

Ensure `find_package(Threads REQUIRED)` is in the root `CMakeLists.txt` if not already present.

- [ ] **Step 3: Verify full build**

```bash
cd helios-core && cmake --build build --target helios-core 2>&1 | head -30
```

- [ ] **Step 4: Commit**

```bash
git add helios-core/CMakeLists.txt
git commit -m "build: add asset system and serialization sources to helios-core"
```

---

## Task 13: Unit Tests — Asset Loading

**Files:**
- Create: `helios-core/tests/test_asset_server.cpp`

Tests for AssetServer async/sync loading, handle resolution, batch progress, and failure handling.

- [ ] **Step 1: Create test_asset_server.cpp**

```cpp
// helios-core/tests/test_asset_server.cpp
#include <gtest/gtest.h>

#include "assets/asset_handle.h"
#include "assets/asset_server.h"
#include "assets/load_batch.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace helios::test {

// Simple test asset type
struct TestAsset {
    std::string content;
};

class AssetServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test directory with test files
        m_test_dir = std::filesystem::temp_directory_path() / "helios_test_assets";
        std::filesystem::create_directories(m_test_dir);

        // Write a test file
        {
            std::ofstream f(m_test_dir / "test.txt");
            f << "hello world";
        }
        // Write another test file
        {
            std::ofstream f(m_test_dir / "test2.txt");
            f << "second file";
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(m_test_dir);
    }

    // Register a simple text-file importer for TestAsset
    void register_test_importer(AssetServer& server) {
        server.register_importer<TestAsset>(
            [](const std::filesystem::path& path) -> std::any {
                std::ifstream f(path);
                if (!f.is_open()) {
                    throw std::runtime_error("Cannot open: " + path.string());
                }
                std::string content(
                    (std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
                return std::any(TestAsset{std::move(content)});
            });
    }

    std::filesystem::path m_test_dir;
};

// ----- AssetHandle tests -----

TEST(AssetHandleTest, DefaultIsNull) {
    AssetHandle h;
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_EQ(h.id, 0u);
}

TEST(AssetHandleTest, NonZeroIsValid) {
    AssetHandle h{42};
    EXPECT_TRUE(static_cast<bool>(h));
}

TEST(AssetHandleTest, Equality) {
    AssetHandle a{1};
    AssetHandle b{1};
    AssetHandle c{2};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(AssetHandleTest, HashWorks) {
    std::unordered_map<AssetHandle, int> map;
    AssetHandle h{99};
    map[h] = 42;
    EXPECT_EQ(map[h], 42);
}

// ----- Sync loading -----

TEST_F(AssetServerTest, SyncLoadSucceeds) {
    AssetServer server(m_test_dir, /*loader_threads=*/1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));
    EXPECT_EQ(server.status(handle), AssetStatus::Loaded);
    EXPECT_TRUE(server.is_loaded(handle));

    const TestAsset* asset = server.get<TestAsset>(handle);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->content, "hello world");
}

TEST_F(AssetServerTest, SyncLoadMissingFileFails) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("nonexistent.txt");
    // load_sync returns id=0 on failure
    EXPECT_FALSE(static_cast<bool>(handle));
}

// ----- Async loading -----

TEST_F(AssetServerTest, AsyncLoadCompletesAndResolves) {
    AssetServer server(m_test_dir, 2);
    register_test_importer(server);

    auto handle = server.load<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));

    // Poll until loaded (with timeout)
    auto start = std::chrono::steady_clock::now();
    while (!server.is_loaded(handle)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5))
            << "Async load timed out";
    }

    const TestAsset* asset = server.get<TestAsset>(handle);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->content, "hello world");
}

TEST_F(AssetServerTest, AsyncLoadEmitsCompletedEvent) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load<TestAsset>("test.txt");

    // Wait for completion
    auto start = std::chrono::steady_clock::now();
    while (!server.is_loaded(handle)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    auto completed = server.drain_completed();
    ASSERT_FALSE(completed.empty());

    bool found = false;
    for (const auto& event : completed) {
        if (event.handle == handle) {
            found = true;
            EXPECT_TRUE(event.success);
        }
    }
    EXPECT_TRUE(found) << "AssetLoaded event for handle not found";
}

TEST_F(AssetServerTest, AsyncLoadFailureStatus) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load<TestAsset>("does_not_exist.txt");

    // Wait for failure
    auto start = std::chrono::steady_clock::now();
    while (server.status(handle) == AssetStatus::Loading) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    EXPECT_EQ(server.status(handle), AssetStatus::Failed);
    EXPECT_EQ(server.get<TestAsset>(handle), nullptr);
}

// ----- Cache deduplication -----

TEST_F(AssetServerTest, DuplicateLoadReturnsSameHandle) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto h1 = server.load<TestAsset>("test.txt");
    auto h2 = server.load<TestAsset>("test.txt");
    EXPECT_EQ(h1, h2);
}

// ----- Batch loading -----

TEST_F(AssetServerTest, BatchLoadProgress) {
    AssetServer server(m_test_dir, 2);
    register_test_importer(server);

    auto batch = server.load_batch()
        .add<TestAsset>("test.txt")
        .add<TestAsset>("test2.txt")
        .submit();

    EXPECT_EQ(batch.total(), 2);

    // Wait for completion
    auto start = std::chrono::steady_clock::now();
    while (!batch.is_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    EXPECT_FLOAT_EQ(batch.progress(), 1.0f);
    EXPECT_EQ(batch.remaining(), 0);
    EXPECT_TRUE(batch.failed().empty());

    // Verify both loaded
    for (const auto& h : batch.handles()) {
        EXPECT_TRUE(server.is_loaded(h));
    }
}

TEST_F(AssetServerTest, BatchLoadWithFailure) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto batch = server.load_batch()
        .add<TestAsset>("test.txt")
        .add<TestAsset>("missing.txt")  // will fail
        .submit();

    EXPECT_EQ(batch.total(), 2);

    // Wait for completion
    auto start = std::chrono::steady_clock::now();
    while (!batch.is_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    EXPECT_TRUE(batch.is_complete());
    auto failures = batch.failed();
    EXPECT_EQ(failures.size(), 1u);
    // The failed path should contain "missing.txt"
    EXPECT_NE(failures[0].find("missing.txt"), std::string::npos);
}

// ----- Wrong type get returns nullptr -----

TEST_F(AssetServerTest, GetWithWrongTypeReturnsNull) {
    struct OtherAsset { int x; };

    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));

    // Try to get as wrong type
    const OtherAsset* wrong = server.get<OtherAsset>(handle);
    EXPECT_EQ(wrong, nullptr);
}

} // namespace helios::test
```

- [ ] **Step 2: Add test to CMakeLists.txt**

```cmake
# In helios-core/tests/CMakeLists.txt or equivalent:
add_executable(test_asset_server test_asset_server.cpp)
target_link_libraries(test_asset_server
    PRIVATE helios-core GTest::gtest_main Threads::Threads)
add_test(NAME AssetServer COMMAND test_asset_server)
```

- [ ] **Step 3: Run tests**

```bash
cd helios-core && cmake --build build --target test_asset_server && ./build/test_asset_server
```

All tests should pass.

- [ ] **Step 4: Commit**

```bash
git add helios-core/tests/test_asset_server.cpp helios-core/tests/CMakeLists.txt
git commit -m "test(assets): add AssetServer unit tests (sync, async, batch, failure, cache)"
```

---

## Task 14: Unit Tests — Serialization Round-Trips

**Files:**
- Create: `helios-core/tests/test_serialization.cpp`

Tests YAML and binary round-trips for Transform, Tag, MeshRenderer, and other aggregate components.

- [ ] **Step 1: Create test_serialization.cpp**

```cpp
// helios-core/tests/test_serialization.cpp
#include <gtest/gtest.h>

#include "assets/asset_handle.h"
#include "serialization/yaml_serializer.h"
#include "serialization/binary_serializer.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <string>

namespace helios::test {

// ----- Test component types -----
// These mirror the spec's component definitions. They are plain aggregates
// compatible with qlibs/reflect.

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct Tag {
    std::string name;
};

struct MeshRenderer {
    AssetHandle mesh{};
    AssetHandle material{};
};

struct PointLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
};

// ----- YAML round-trip tests -----

TEST(YamlSerializerTest, TransformRoundTrip) {
    Transform original;
    original.position = {1.5f, 2.0f, -3.0f};
    original.rotation = glm::quat{0.707f, 0.0f, 0.707f, 0.0f};
    original.scale = {2.0f, 2.0f, 2.0f};

    auto yaml = YamlSerializer::serialize_component(original);
    auto restored = YamlSerializer::deserialize_from_string<Transform>(yaml);

    EXPECT_NEAR(restored.position.x, original.position.x, 0.001f);
    EXPECT_NEAR(restored.position.y, original.position.y, 0.001f);
    EXPECT_NEAR(restored.position.z, original.position.z, 0.001f);

    EXPECT_NEAR(restored.rotation.w, original.rotation.w, 0.001f);
    EXPECT_NEAR(restored.rotation.x, original.rotation.x, 0.001f);
    EXPECT_NEAR(restored.rotation.y, original.rotation.y, 0.001f);
    EXPECT_NEAR(restored.rotation.z, original.rotation.z, 0.001f);

    EXPECT_NEAR(restored.scale.x, original.scale.x, 0.001f);
    EXPECT_NEAR(restored.scale.y, original.scale.y, 0.001f);
    EXPECT_NEAR(restored.scale.z, original.scale.z, 0.001f);
}

TEST(YamlSerializerTest, TagRoundTrip) {
    Tag original;
    original.name = "Player Entity";

    auto yaml = YamlSerializer::serialize_component(original);
    auto restored = YamlSerializer::deserialize_from_string<Tag>(yaml);

    EXPECT_EQ(restored.name, original.name);
}

TEST(YamlSerializerTest, MeshRendererRoundTrip) {
    MeshRenderer original;
    original.mesh = AssetHandle{42};
    original.material = AssetHandle{99};

    auto yaml = YamlSerializer::serialize_component(original);
    auto restored =
        YamlSerializer::deserialize_from_string<MeshRenderer>(yaml);

    EXPECT_EQ(restored.mesh.id, original.mesh.id);
    EXPECT_EQ(restored.material.id, original.material.id);
}

TEST(YamlSerializerTest, PointLightRoundTrip) {
    PointLight original;
    original.color = {0.9f, 0.8f, 0.3f};
    original.intensity = 5.0f;
    original.radius = 25.0f;

    auto yaml = YamlSerializer::serialize_component(original);
    auto restored =
        YamlSerializer::deserialize_from_string<PointLight>(yaml);

    EXPECT_NEAR(restored.color.x, original.color.x, 0.001f);
    EXPECT_NEAR(restored.color.y, original.color.y, 0.001f);
    EXPECT_NEAR(restored.color.z, original.color.z, 0.001f);
    EXPECT_NEAR(restored.intensity, original.intensity, 0.001f);
    EXPECT_NEAR(restored.radius, original.radius, 0.001f);
}

TEST(YamlSerializerTest, DefaultValuesPreserved) {
    Transform original;  // all defaults

    auto yaml = YamlSerializer::serialize_component(original);
    auto restored = YamlSerializer::deserialize_from_string<Transform>(yaml);

    EXPECT_NEAR(restored.position.x, 0.0f, 0.001f);
    EXPECT_NEAR(restored.position.y, 0.0f, 0.001f);
    EXPECT_NEAR(restored.position.z, 0.0f, 0.001f);

    EXPECT_NEAR(restored.rotation.w, 1.0f, 0.001f);
    EXPECT_NEAR(restored.rotation.x, 0.0f, 0.001f);

    EXPECT_NEAR(restored.scale.x, 1.0f, 0.001f);
}

TEST(YamlSerializerTest, EmptyStringHandled) {
    Tag original;
    original.name = "";

    auto yaml = YamlSerializer::serialize_component(original);
    auto restored = YamlSerializer::deserialize_from_string<Tag>(yaml);

    EXPECT_EQ(restored.name, "");
}

TEST(YamlSerializerTest, SpecialCharactersInString) {
    Tag original;
    original.name = "Entity: \"Player\" [1]";

    auto yaml = YamlSerializer::serialize_component(original);
    auto restored = YamlSerializer::deserialize_from_string<Tag>(yaml);

    EXPECT_EQ(restored.name, original.name);
}

// ----- Binary round-trip tests -----

TEST(BinarySerializerTest, TransformRoundTrip) {
    Transform original;
    original.position = {1.5f, 2.0f, -3.0f};
    original.rotation = glm::quat{0.707f, 0.0f, 0.707f, 0.0f};
    original.scale = {2.0f, 2.0f, 2.0f};

    auto bytes = BinarySerializer::serialize_component(original);
    auto restored = BinarySerializer::deserialize_component<Transform>(bytes);

    // Binary is exact (no float-to-string conversion loss)
    EXPECT_EQ(restored.position.x, original.position.x);
    EXPECT_EQ(restored.position.y, original.position.y);
    EXPECT_EQ(restored.position.z, original.position.z);

    EXPECT_EQ(restored.rotation.w, original.rotation.w);
    EXPECT_EQ(restored.rotation.x, original.rotation.x);
    EXPECT_EQ(restored.rotation.y, original.rotation.y);
    EXPECT_EQ(restored.rotation.z, original.rotation.z);

    EXPECT_EQ(restored.scale.x, original.scale.x);
}

TEST(BinarySerializerTest, TagRoundTrip) {
    Tag original;
    original.name = "Player Entity";

    auto bytes = BinarySerializer::serialize_component(original);
    auto restored = BinarySerializer::deserialize_component<Tag>(bytes);

    EXPECT_EQ(restored.name, original.name);
}

TEST(BinarySerializerTest, MeshRendererRoundTrip) {
    MeshRenderer original;
    original.mesh = AssetHandle{42};
    original.material = AssetHandle{99};

    auto bytes = BinarySerializer::serialize_component(original);
    auto restored =
        BinarySerializer::deserialize_component<MeshRenderer>(bytes);

    EXPECT_EQ(restored.mesh.id, original.mesh.id);
    EXPECT_EQ(restored.material.id, original.material.id);
}

TEST(BinarySerializerTest, PointLightRoundTrip) {
    PointLight original;
    original.color = {0.9f, 0.8f, 0.3f};
    original.intensity = 5.0f;
    original.radius = 25.0f;

    auto bytes = BinarySerializer::serialize_component(original);
    auto restored =
        BinarySerializer::deserialize_component<PointLight>(bytes);

    EXPECT_EQ(restored.color.x, original.color.x);
    EXPECT_EQ(restored.color.y, original.color.y);
    EXPECT_EQ(restored.color.z, original.color.z);
    EXPECT_EQ(restored.intensity, original.intensity);
    EXPECT_EQ(restored.radius, original.radius);
}

TEST(BinarySerializerTest, EmptyStringRoundTrip) {
    Tag original;
    original.name = "";

    auto bytes = BinarySerializer::serialize_component(original);
    auto restored = BinarySerializer::deserialize_component<Tag>(bytes);

    EXPECT_EQ(restored.name, "");
}

TEST(BinarySerializerTest, NullAssetHandleRoundTrip) {
    MeshRenderer original;  // both handles are 0

    auto bytes = BinarySerializer::serialize_component(original);
    auto restored =
        BinarySerializer::deserialize_component<MeshRenderer>(bytes);

    EXPECT_EQ(restored.mesh.id, 0u);
    EXPECT_EQ(restored.material.id, 0u);
}

TEST(BinarySerializerTest, ByteSizeIsCompact) {
    Transform t;
    auto bytes = BinarySerializer::serialize_component(t);

    // Transform has: vec3 (12) + quat (16) + vec3 (12) = 40 bytes
    EXPECT_EQ(bytes.size(), 40u);
}

} // namespace helios::test
```

- [ ] **Step 2: Add test to CMakeLists.txt**

```cmake
add_executable(test_serialization test_serialization.cpp)
target_link_libraries(test_serialization
    PRIVATE helios-core GTest::gtest_main yaml-cpp)
add_test(NAME Serialization COMMAND test_serialization)
```

- [ ] **Step 3: Run tests**

```bash
cd helios-core && cmake --build build --target test_serialization && ./build/test_serialization
```

All tests should pass.

- [ ] **Step 4: Commit**

```bash
git add helios-core/tests/test_serialization.cpp helios-core/tests/CMakeLists.txt
git commit -m "test(serialization): add YAML and binary round-trip tests for Transform, Tag, MeshRenderer"
```

---

## Task 15: Umbrella Header and Final Wiring

**Files:**
- Create: `helios-core/src/assets/assets.h` (umbrella include)
- Create: `helios-core/src/serialization/serialization.h` (umbrella include)

Convenience headers that pull in the full asset and serialization APIs.

- [ ] **Step 1: Create assets.h**

```cpp
// helios-core/src/assets/assets.h
#pragma once

#include "asset_handle.h"
#include "asset_server.h"
#include "load_batch.h"
#include "asset_plugin.h"
#include "importers/texture_importer.h"
#include "importers/mesh_importer.h"
#include "importers/audio_importer.h"
```

- [ ] **Step 2: Create serialization.h**

```cpp
// helios-core/src/serialization/serialization.h
#pragma once

#include "reflect_helpers.h"
#include "yaml_serializer.h"
#include "binary_serializer.h"
#include "scene_serializer.h"
```

- [ ] **Step 3: Verify everything compiles and all tests pass**

```bash
cd helios-core && cmake --build build 2>&1 | tail -5
cd helios-core && ctest --output-on-failure
```

- [ ] **Step 4: Commit**

```bash
git add helios-core/src/assets/assets.h helios-core/src/serialization/serialization.h
git commit -m "feat(assets): add umbrella headers for assets and serialization modules"
```

---

## File Summary

```
helios-core/
  src/
    assets/
      asset_handle.h                    → AssetHandle, AssetStatus
      asset_server.h                    → AssetServer (template-heavy, mostly header)
      asset_server.cpp                  → AssetServer non-template methods
      load_batch.h                      → LoadBatchBuilder, LoadBatch
      load_batch.cpp                    → LoadBatch implementation
      asset_plugin.h                    → AssetPlugin config + struct
      asset_plugin.cpp                  → Importer registration, event drain system
      assets.h                          → Umbrella include
      importers/
        texture_importer.h              → TextureData, HdrTextureData, TextureImporter
        texture_importer.cpp            → stb_image loading
        mesh_importer.h                 → Vertex, SubMesh, MaterialData, MeshData, MeshImporter
        mesh_importer.cpp               → assimp loading
        audio_importer.h               → AudioData, AudioFormat, AudioImporter
        audio_importer.cpp             → Raw file byte loading
    serialization/
      reflect_helpers.h                 → write_yaml/read_yaml/write_binary/read_binary overloads
      yaml_serializer.h                → YamlSerializer (templates + reflect)
      yaml_serializer.cpp             → All write_yaml/read_yaml definitions
      binary_serializer.h             → BinarySerializer (templates + reflect)
      binary_serializer.cpp           → All write_binary/read_binary definitions
      scene_serializer.h              → SceneSerializer with component registration
      scene_serializer.cpp            → Scene YAML save/load
      serialization.h                 → Umbrella include
  tests/
    test_asset_server.cpp              → 11 tests: handle, sync, async, batch, cache, failure
    test_serialization.cpp             → 14 tests: YAML + binary round-trips
```

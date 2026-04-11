#include "helios/assets/load_batch.h"

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
{
    // Acquire a refcount for each handle so the assets cannot be GC'd
    // while the batch is alive.
    for (const auto& h : m_handles) {
        m_server->acquire(h);
    }
}

LoadBatch::~LoadBatch() {
    release_all();
}

LoadBatch::LoadBatch(LoadBatch&& other) noexcept
    : m_server(other.m_server)
    , m_handles(std::move(other.m_handles))
    , m_paths(std::move(other.m_paths))
{
    other.m_server = nullptr;
}

LoadBatch& LoadBatch::operator=(LoadBatch&& other) noexcept {
    if (this != &other) {
        release_all();
        m_server = other.m_server;
        m_handles = std::move(other.m_handles);
        m_paths = std::move(other.m_paths);
        other.m_server = nullptr;
    }
    return *this;
}

void LoadBatch::release_all() {
    if (m_server) {
        for (const auto& h : m_handles) {
            m_server->release(h);
        }
    }
}

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

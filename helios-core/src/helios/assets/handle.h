#pragma once

#include "helios/ecs/asset_handle.h"

#include <cstdint>
#include <functional>

namespace helios {

// Forward declaration -- Handle<T> stores a non-owning pointer to AssetServer.
class AssetServer;

// Non-template bridge functions defined in handle.cpp.
// Break the Handle<T> <-> AssetServer circular header dependency.
void asset_handle_acquire(AssetServer* server, AssetHandle id);
void asset_handle_release(AssetServer* server, AssetHandle id);

/// RAII typed asset handle. Automatically acquires/releases a refcount
/// on the AssetServer when copied/moved/destroyed.
///
/// A null handle (default-constructed or with null server) is valid and
/// acts as a no-op for acquire/release.
///
/// The AssetServer MUST outlive all Handles that reference it. This is
/// guaranteed because AssetServer is a World resource destroyed in
/// reverse insertion order (after components that hold handles).
template<typename T>
class Handle {
public:
    /// Null handle (no asset, no server).
    Handle() = default;

    /// Construct from a raw AssetHandle and server pointer.
    /// Acquires a refcount immediately.
    Handle(AssetHandle raw, AssetServer* server)
        : m_id(raw), m_server(server) {
        asset_handle_acquire(m_server, m_id);
    }

    ~Handle() {
        asset_handle_release(m_server, m_id);
    }

    Handle(const Handle& other)
        : m_id(other.m_id), m_server(other.m_server) {
        asset_handle_acquire(m_server, m_id);
    }

    Handle& operator=(const Handle& other) {
        if (this != &other) {
            asset_handle_release(m_server, m_id);
            m_id = other.m_id;
            m_server = other.m_server;
            asset_handle_acquire(m_server, m_id);
        }
        return *this;
    }

    Handle(Handle&& other) noexcept
        : m_id(other.m_id), m_server(other.m_server) {
        other.m_id = {};
        other.m_server = nullptr;
    }

    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            asset_handle_release(m_server, m_id);
            m_id = other.m_id;
            m_server = other.m_server;
            other.m_id = {};
            other.m_server = nullptr;
        }
        return *this;
    }

    /// Release the held reference and become a null handle.
    void reset() {
        asset_handle_release(m_server, m_id);
        m_id = {};
        m_server = nullptr;
    }

    explicit operator bool() const { return m_id.index != 0 || m_id.generation != 0; }
    bool operator==(const Handle& other) const { return m_id == other.m_id; }

    /// Return the underlying type-erased AssetHandle.
    AssetHandle untyped() const { return m_id; }

    uint32_t index() const { return m_id.index; }
    uint32_t generation() const { return m_id.generation; }

    /// Construct a Handle from a raw AssetHandle without a server.
    /// The resulting handle has no refcount management (acquire/release
    /// are no-ops). Useful for tests and deserialization.
    static Handle from(AssetHandle h) { return Handle(h, nullptr); }

private:
    /// Private constructor that skips acquire -- used by from().
    /// The two-arg public constructor always acquires.
    /// from() creates handles without a server, so acquire is a no-op anyway.

    AssetHandle m_id{};
    AssetServer* m_server = nullptr;
};

} // namespace helios

/// std::hash specialization for Handle<T>.
template<typename T>
struct std::hash<helios::Handle<T>> {
    size_t operator()(const helios::Handle<T>& h) const noexcept {
        return std::hash<uint64_t>{}(h.untyped().packed());
    }
};

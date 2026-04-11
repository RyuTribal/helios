#pragma once

#include <cstdint>
#include <functional>

namespace helios {

/// Lightweight handle for a scene managed by SceneManager.
/// 8 bytes: index + generation for slot-map style reuse.
struct SceneHandle {
    uint32_t index = 0;
    uint32_t generation = 0;

    explicit operator bool() const { return index != 0 || generation != 0; }
    bool operator==(const SceneHandle&) const = default;
    bool operator!=(const SceneHandle&) const = default;

    /// Pack into a single uint64_t key for use in hash maps.
    uint64_t packed() const {
        return (static_cast<uint64_t>(index) << 32) | static_cast<uint64_t>(generation);
    }

    /// Unpack a uint64_t key back into a SceneHandle.
    static SceneHandle from_packed(uint64_t key) {
        return SceneHandle{
            static_cast<uint32_t>(key >> 32),
            static_cast<uint32_t>(key & 0xFFFFFFFF)
        };
    }
};

/// Tag component attached to every entity spawned by a scene.
/// Allows filtering all entities belonging to a specific scene.
struct SceneTag {
    SceneHandle scene;
};

} // namespace helios

template <>
struct std::hash<helios::SceneHandle> {
    size_t operator()(helios::SceneHandle h) const noexcept {
        return std::hash<uint64_t>{}(h.packed());
    }
};

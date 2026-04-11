#pragma once

#include <cstdint>
#include <cstddef>
#include <compare>
#include <functional>

namespace helios {

enum class AssetStatus : uint8_t {
    Loading,
    Loaded,
    Failed
};

struct AssetHandle {
    uint32_t index = 0;
    uint32_t generation = 0;

    explicit operator bool() const { return index != 0 || generation != 0; }
    bool operator==(const AssetHandle&) const = default;
    bool operator!=(const AssetHandle&) const = default;
    auto operator<=>(const AssetHandle&) const = default;

    /// Pack index and generation into a single uint64_t key for maps.
    uint64_t packed() const {
        return (static_cast<uint64_t>(index) << 32) | static_cast<uint64_t>(generation);
    }

    /// Unpack a uint64_t key back into an AssetHandle.
    static AssetHandle from_packed(uint64_t key) {
        return AssetHandle{
            static_cast<uint32_t>(key >> 32),
            static_cast<uint32_t>(key & 0xFFFFFFFF)
        };
    }
};

struct BodyHandle {
    uint64_t id = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const BodyHandle&) const = default;
};

struct SoundHandle {
    uint64_t id = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const SoundHandle&) const = default;
};

} // namespace helios

template <>
struct std::hash<helios::AssetHandle> {
    size_t operator()(helios::AssetHandle h) const noexcept {
        return std::hash<uint64_t>{}(h.packed());
    }
};

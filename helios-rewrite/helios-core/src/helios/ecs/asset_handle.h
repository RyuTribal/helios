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
    uint64_t id = 0;

    explicit operator bool() const { return id != 0; }
    bool operator==(const AssetHandle&) const = default;
    bool operator!=(const AssetHandle&) const = default;
    auto operator<=>(const AssetHandle&) const = default;
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
        return std::hash<uint64_t>{}(h.id);
    }
};

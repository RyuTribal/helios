#pragma once

#include "helios/ecs/asset_handle.h"

#include <cstdint>
#include <functional>

namespace helios {

/// Typed asset handle. 8 bytes, trivially copyable, plain aggregate.
/// Wraps AssetHandle with compile-time type safety.
template<typename T>
struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0;

    explicit operator bool() const { return index != 0 || generation != 0; }
    bool operator==(const Handle&) const = default;

    /// Convert to type-erased AssetHandle.
    AssetHandle untyped() const { return {index, generation}; }

    /// Construct from a type-erased AssetHandle.
    static Handle from(AssetHandle h) { return {h.index, h.generation}; }
};

} // namespace helios

/// std::hash specialization for Handle<T>.
template<typename T>
struct std::hash<helios::Handle<T>> {
    size_t operator()(helios::Handle<T> h) const noexcept {
        return std::hash<uint64_t>{}(h.untyped().packed());
    }
};

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace helios {

struct Entity {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool operator==(const Entity&) const = default;
    bool operator!=(const Entity&) const = default;

    explicit operator bool() const { return generation != 0; }

    static const Entity INVALID;
};

} // namespace helios

template <>
struct std::hash<helios::Entity> {
    size_t operator()(helios::Entity e) const noexcept {
        return std::hash<uint64_t>{}(
            (static_cast<uint64_t>(e.generation) << 32) | e.index);
    }
};

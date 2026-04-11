#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <iosfwd>

namespace helios {

/// 128-bit universally unique identifier.
/// Generated via thread-local PRNG — safe to call from any thread.
struct UUID {
    uint64_t high = 0;
    uint64_t low = 0;

    /// Generate a new random UUID (v4-like, thread-safe).
    static UUID generate();

    /// Parse from string: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" or "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    static UUID from_string(std::string_view str);

    /// Format as "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    std::string to_string() const;

    bool operator==(const UUID&) const = default;
    bool operator!=(const UUID&) const = default;

    /// True if not the nil UUID (all zeros).
    explicit operator bool() const { return high != 0 || low != 0; }

    /// Nil UUID constant.
    static const UUID NIL;
};

/// Stream output.
std::ostream& operator<<(std::ostream& os, const UUID& uuid);

} // namespace helios

/// Hash specialization.
template <>
struct std::hash<helios::UUID> {
    size_t operator()(const helios::UUID& uuid) const noexcept {
        // FNV-1a inspired combine
        size_t h = std::hash<uint64_t>{}(uuid.high);
        h ^= std::hash<uint64_t>{}(uuid.low) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

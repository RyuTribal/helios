// helios-core/src/helios/ecs/access_descriptor.h
#pragma once

#include <cstdint>
#include <typeindex>
#include <vector>

namespace helios {

enum class AccessMode : uint8_t {
    Read,
    Write,
};

struct AccessDescriptor {
    std::type_index type;
    AccessMode      mode;

    bool conflicts_with(const AccessDescriptor& other) const {
        if (type != other.type) return false;
        // Two reads never conflict. Anything involving a write conflicts.
        return (mode == AccessMode::Write || other.mode == AccessMode::Write);
    }

    bool operator==(const AccessDescriptor& other) const {
        return type == other.type && mode == other.mode;
    }
};

/// Check whether two access-descriptor lists have any conflict.
inline bool has_conflict(const std::vector<AccessDescriptor>& a,
                         const std::vector<AccessDescriptor>& b) {
    for (const auto& da : a) {
        for (const auto& db : b) {
            if (da.conflicts_with(db)) return true;
        }
    }
    return false;
}

} // namespace helios

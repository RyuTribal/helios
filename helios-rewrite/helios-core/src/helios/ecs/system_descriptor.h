// helios-core/src/helios/ecs/system_descriptor.h
#pragma once

#include "helios/ecs/access_descriptor.h"
#include "helios/core/assert.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <typeindex>
#include <vector>

namespace helios {

// Forward declaration -- World is from Plan 1
class World;

/// Stable identifier for a registered system. Wraps a monotonically increasing uint64.
struct SystemId {
    uint64_t value = 0;

    bool operator==(const SystemId&) const = default;
    bool operator!=(const SystemId&) const = default;
    bool operator<(const SystemId& other) const { return value < other.value; }
};

/// Type-erased descriptor for a single system.
struct SystemDescriptor {
    SystemId                         id;
    std::string                      name;       // human-readable, for debug
    std::function<void(World&)>      run;        // type-erased invocation
    std::vector<AccessDescriptor>    accesses;   // all reads and writes
    std::vector<SystemId>            after;       // must run after these systems
    std::vector<SystemId>            before;      // must run before these systems

    /// Optional tag for bulk removal (used by state management).
    /// Systems owned by a state are tagged with the state's type_index
    /// so they can be removed when the state is popped.
    std::optional<std::type_index>   owner_tag;
};

/// Helper: extract only reads from accesses
inline std::vector<AccessDescriptor> reads_of(const SystemDescriptor& desc) {
    std::vector<AccessDescriptor> result;
    for (const auto& a : desc.accesses) {
        if (a.mode == AccessMode::Read) result.push_back(a);
    }
    return result;
}

/// Helper: extract only writes from accesses
inline std::vector<AccessDescriptor> writes_of(const SystemDescriptor& desc) {
    std::vector<AccessDescriptor> result;
    for (const auto& a : desc.accesses) {
        if (a.mode == AccessMode::Write) result.push_back(a);
    }
    return result;
}

/// Builder returned by add_system, allows chaining .after() / .before()
class SystemDescriptorBuilder {
public:
    explicit SystemDescriptorBuilder(SystemDescriptor& desc)
        : m_desc(desc) {}

    SystemDescriptorBuilder& after(SystemId id) {
        HELIOS_ASSERT(id.value != 0,
            "after() called with invalid SystemId -- check id_of() returned a valid system");
        m_desc.after.push_back(id);
        return *this;
    }

    SystemDescriptorBuilder& before(SystemId id) {
        HELIOS_ASSERT(id.value != 0,
            "before() called with invalid SystemId -- check id_of() returned a valid system");
        m_desc.before.push_back(id);
        return *this;
    }

    SystemId id() const { return m_desc.id; }

private:
    SystemDescriptor& m_desc;
};

} // namespace helios

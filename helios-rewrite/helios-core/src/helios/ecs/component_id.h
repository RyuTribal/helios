#pragma once
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <concepts>

namespace helios {

// Components must be aggregates: no user-declared constructors, no virtual
// functions, no private/protected data members. This is required for:
//   1. Automatic reflection via qlibs/reflect
//   2. Cache-friendly archetype storage
//   3. Safe type-erased column operations
template <typename T>
concept Component = std::is_aggregate_v<T> && std::is_move_constructible_v<T> && std::is_destructible_v<T>;

using ComponentId = std::type_index;
using ArchetypeId = std::vector<ComponentId>;

template <typename T>
ComponentId component_id() {
    return ComponentId(typeid(T));
}

template <typename... Ts>
ArchetypeId make_archetype_id() {
    ArchetypeId id{component_id<Ts>()...};
    std::sort(id.begin(), id.end());
    return id;
}

inline bool archetype_has(const ArchetypeId& id, ComponentId comp) {
    return std::binary_search(id.begin(), id.end(), comp);
}

inline ArchetypeId archetype_with(const ArchetypeId& id, ComponentId comp) {
    ArchetypeId result = id;
    auto pos = std::lower_bound(result.begin(), result.end(), comp);
    if (pos == result.end() || *pos != comp) {
        result.insert(pos, comp);
    }
    return result;
}

inline ArchetypeId archetype_without(const ArchetypeId& id, ComponentId comp) {
    ArchetypeId result = id;
    auto pos = std::lower_bound(result.begin(), result.end(), comp);
    if (pos != result.end() && *pos == comp) {
        result.erase(pos);
    }
    return result;
}

} // namespace helios

template <>
struct std::hash<helios::ArchetypeId> {
    size_t operator()(const helios::ArchetypeId& id) const noexcept {
        size_t seed = id.size();
        for (const auto& comp : id) {
            seed ^= std::hash<std::type_index>{}(comp) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

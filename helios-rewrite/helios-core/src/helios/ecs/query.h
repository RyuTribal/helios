#pragma once
#include "helios/ecs/query_filters.h"
#include "helios/ecs/archetype_storage.h"
#include "helios/ecs/component_id.h"
#include "helios/ecs/entity.h"
#include <vector>
#include <tuple>
#include <optional>
#include <cstddef>

namespace helios {

// ---------------------------------------------------------------------------
// Query<Params...>
//
// Params can be:
//   T          - fetch mutable reference    (T&)
//   const T    - fetch const reference      (const T&)
//   With<T>    - require component, no fetch
//   Without<T> - exclude component, no fetch
//   Optional<T>- fetch pointer (nullptr when absent)
// ---------------------------------------------------------------------------

template <typename... Params>
class Query {
public:
    using ResultTuple = fetch_tuple_t<Params...>;

    explicit Query(ArchetypeStorage& storage)
        : m_storage(&storage)
    {
        cache_archetypes();
    }

    // -----------------------------------------------------------------------
    // Fetch helpers (static, shared by Iterator and get())
    // -----------------------------------------------------------------------
private:
    template <typename P>
    static auto fetch_one(Archetype& arch, size_t row) {
        if constexpr (is_filter_v<P>) {
            return std::tuple<>();
        } else if constexpr (is_optional_v<P>) {
            using Inner = filter_inner_t<P>;
            if (arch.has_component(component_id<Inner>())) {
                return std::tuple<Inner*>(
                    &arch.get_column<Inner>().template get<Inner>(row));
            } else {
                return std::tuple<Inner*>(nullptr);
            }
        } else if constexpr (std::is_const_v<P>) {
            using Raw = std::remove_const_t<P>;
            return std::tuple<const Raw&>(
                arch.get_column<Raw>().template get<Raw>(row));
        } else {
            return std::tuple<P&>(
                arch.get_column<P>().template get<P>(row));
        }
    }

    static ResultTuple fetch_from(Archetype& arch, size_t row) {
        return std::tuple_cat(fetch_one<Params>(arch, row)...);
    }

public:
    // -----------------------------------------------------------------------
    // Iterator
    // -----------------------------------------------------------------------
    class Iterator {
    public:
        using value_type = ResultTuple;
        using difference_type = std::ptrdiff_t;

        Iterator() = default;

        Iterator(const std::vector<Archetype*>& archetypes,
                 size_t arch_idx, size_t row)
            : m_archetypes(&archetypes)
            , m_arch_idx(arch_idx)
            , m_row(row)
        {
            skip_empty();
        }

        ResultTuple operator*() const {
            Archetype& arch = *(*m_archetypes)[m_arch_idx];
            return fetch_from(arch, m_row);
        }

        Iterator& operator++() {
            ++m_row;
            if (m_row >= (*m_archetypes)[m_arch_idx]->size()) {
                m_row = 0;
                ++m_arch_idx;
                skip_empty();
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator copy = *this;
            ++(*this);
            return copy;
        }

        bool operator==(const Iterator& other) const {
            return m_arch_idx == other.m_arch_idx && m_row == other.m_row;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        const std::vector<Archetype*>* m_archetypes = nullptr;
        size_t m_arch_idx = 0;
        size_t m_row = 0;

        void skip_empty() {
            while (m_arch_idx < m_archetypes->size() &&
                   (*m_archetypes)[m_arch_idx]->empty()) {
                ++m_arch_idx;
            }
        }
    };

    // -----------------------------------------------------------------------
    // Range interface
    // -----------------------------------------------------------------------

    Iterator begin() const {
        return Iterator(m_cached, 0, 0);
    }

    Iterator end() const {
        return Iterator(m_cached, m_cached.size(), 0);
    }

    // -----------------------------------------------------------------------
    // Utilities
    // -----------------------------------------------------------------------

    /// Total entity count across all matching archetypes.
    size_t count() const {
        size_t total = 0;
        for (auto* arch : m_cached) {
            total += arch->size();
        }
        return total;
    }

    bool is_empty() const {
        return count() == 0;
    }

    /// Retrieve components for a specific entity.
    /// Returns std::nullopt if the entity is not in a matching archetype.
    std::optional<ResultTuple> get(Entity entity) const {
        auto loc = m_storage->locate(entity);
        if (!loc.has_value()) return std::nullopt;

        Archetype* arch = loc->archetype;
        if (!matches(*arch)) return std::nullopt;

        return fetch_from(*arch, loc->row);
    }

private:
    ArchetypeStorage* m_storage = nullptr;
    std::vector<Archetype*> m_cached;

    void cache_archetypes() {
        m_storage->for_each_archetype([&](Archetype& arch) {
            if (matches(arch)) {
                m_cached.push_back(&arch);
            }
        });
    }

    /// Check whether an archetype satisfies all parameter constraints.
    static bool matches(const Archetype& arch) {
        return (check_param<Params>(arch) && ...);
    }

    /// Per-parameter archetype check.
    template <typename P>
    static bool check_param(const Archetype& arch) {
        if constexpr (is_with_v<P>) {
            return arch.has_component(component_id<filter_inner_t<P>>());
        } else if constexpr (is_without_v<P>) {
            return !arch.has_component(component_id<filter_inner_t<P>>());
        } else if constexpr (is_optional_v<P>) {
            return true;
        } else {
            using Raw = std::remove_const_t<P>;
            return arch.has_component(component_id<Raw>());
        }
    }
};

} // namespace helios

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
//
// Snapshot semantics:
//   The set of matching archetypes is captured at construction time
//   (cache_archetypes). Archetypes created AFTER construction (e.g. by
//   another system spawning entities with a new component combination)
//   are invisible to this query instance.
//
// Structural mutation during iteration:
//   Calling World::despawn(), World::add(), or World::remove() on an entity
//   that is currently being iterated over is undefined behavior -- the
//   archetype's internal arrays may be invalidated. Use Commands to defer
//   structural changes; the scheduler applies them after each stage.
// ---------------------------------------------------------------------------

template <typename... Params>
class Query {
public:
    using ResultTuple = fetch_tuple_t<Params...>;
    using EntityResultTuple = decltype(std::tuple_cat(
        std::declval<std::tuple<Entity>>(), std::declval<ResultTuple>()));

    explicit Query(ArchetypeStorage& storage,
                   uint32_t last_run_tick = 0,
                   uint32_t current_tick = 0)
        : m_storage(&storage)
        , m_last_run_tick(last_run_tick)
        , m_current_tick(current_tick)
    {
        cache_archetypes();
    }

    // -----------------------------------------------------------------------
    // Fetch helpers (static, shared by Iterator and get())
    // -----------------------------------------------------------------------
private:
    template <typename P>
    static auto fetch_one(Archetype& arch, size_t row, uint32_t current_tick) {
        if constexpr (is_filter_v<P>) {
            return std::tuple<>();
        } else if constexpr (is_optional_v<P>) {
            using Inner = filter_inner_t<P>;
            if (arch.has_component(component_id<Inner>())) {
                // Stamp on mutable Optional access
                if constexpr (!std::is_const_v<Inner>) {
                    arch.get_column<Inner>().stamp(row, current_tick);
                }
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
            // Mutable access: auto-stamp the change tick.
            arch.get_column<P>().stamp(row, current_tick);
            return std::tuple<P&>(
                arch.get_column<P>().template get<P>(row));
        }
    }

    static ResultTuple fetch_from(Archetype& arch, size_t row, uint32_t current_tick) {
        return std::tuple_cat(fetch_one<Params>(arch, row, current_tick)...);
    }

    /// True at compile time when any Params are Changed<T>.
    static constexpr bool has_changed_filter = (is_changed_v<Params> || ...);

    /// Per-entity check: returns true if all Changed<T> filters pass.
    /// A Changed<T> filter passes when the component's tick is > last_run_tick
    /// (i.e. the component was modified since the owning system last ran).
    template <typename P>
    static bool check_changed_one([[maybe_unused]] Archetype& arch,
                                  [[maybe_unused]] size_t row,
                                  [[maybe_unused]] uint32_t last_run_tick) {
        if constexpr (is_changed_v<P>) {
            using Inner = filter_inner_t<P>;
            return arch.get_column<Inner>().changed_tick(row) > last_run_tick;
        } else {
            return true;
        }
    }

    static bool passes_changed_filters(Archetype& arch, size_t row, uint32_t last_run_tick) {
        return (check_changed_one<Params>(arch, row, last_run_tick) && ...);
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
                 size_t arch_idx, size_t row,
                 uint32_t last_run_tick, uint32_t current_tick)
            : m_archetypes(&archetypes)
            , m_arch_idx(arch_idx)
            , m_row(row)
            , m_last_run_tick(last_run_tick)
            , m_current_tick(current_tick)
        {
            advance_to_valid();
        }

        ResultTuple operator*() const {
            Archetype& arch = *(*m_archetypes)[m_arch_idx];
            return fetch_from(arch, m_row, m_current_tick);
        }

        Iterator& operator++() {
            advance_one();
            advance_to_valid();
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
        uint32_t m_last_run_tick = 0;
        uint32_t m_current_tick = 0;

        /// Move to the next position (row+1, wrapping to next archetype).
        void advance_one() {
            ++m_row;
            if (m_row >= (*m_archetypes)[m_arch_idx]->size()) {
                m_row = 0;
                ++m_arch_idx;
            }
        }

        /// Skip empty archetypes and, when Changed<T> filters are present,
        /// skip rows that do not pass the per-entity tick check.
        void advance_to_valid() {
            while (m_arch_idx < m_archetypes->size()) {
                auto* arch = (*m_archetypes)[m_arch_idx];
                if (arch->empty()) {
                    ++m_arch_idx;
                    m_row = 0;
                    continue;
                }
                if constexpr (has_changed_filter) {
                    if (m_row < arch->size() &&
                        !passes_changed_filters(*arch, m_row, m_last_run_tick)) {
                        advance_one();
                        continue;
                    }
                }
                if (m_row < arch->size()) break;
                // Exhausted this archetype.
                ++m_arch_idx;
                m_row = 0;
            }
        }
    };

    // -----------------------------------------------------------------------
    // EntityIterator — yields (Entity, components...) tuples
    // -----------------------------------------------------------------------
    class EntityIterator {
    public:
        using value_type = EntityResultTuple;
        using difference_type = std::ptrdiff_t;

        EntityIterator() = default;

        EntityIterator(const std::vector<Archetype*>& archetypes,
                       size_t arch_idx, size_t row,
                       uint32_t last_run_tick, uint32_t current_tick)
            : m_archetypes(&archetypes)
            , m_arch_idx(arch_idx)
            , m_row(row)
            , m_last_run_tick(last_run_tick)
            , m_current_tick(current_tick)
        {
            advance_to_valid();
        }

        EntityResultTuple operator*() const {
            Archetype& arch = *(*m_archetypes)[m_arch_idx];
            Entity entity = arch.entities[m_row];
            return std::tuple_cat(std::make_tuple(entity), fetch_from(arch, m_row, m_current_tick));
        }

        EntityIterator& operator++() {
            advance_one();
            advance_to_valid();
            return *this;
        }

        EntityIterator operator++(int) {
            EntityIterator copy = *this;
            ++(*this);
            return copy;
        }

        bool operator==(const EntityIterator& other) const {
            return m_arch_idx == other.m_arch_idx && m_row == other.m_row;
        }

        bool operator!=(const EntityIterator& other) const {
            return !(*this == other);
        }

    private:
        const std::vector<Archetype*>* m_archetypes = nullptr;
        size_t m_arch_idx = 0;
        size_t m_row = 0;
        uint32_t m_last_run_tick = 0;
        uint32_t m_current_tick = 0;

        void advance_one() {
            ++m_row;
            if (m_row >= (*m_archetypes)[m_arch_idx]->size()) {
                m_row = 0;
                ++m_arch_idx;
            }
        }

        void advance_to_valid() {
            while (m_arch_idx < m_archetypes->size()) {
                auto* arch = (*m_archetypes)[m_arch_idx];
                if (arch->empty()) {
                    ++m_arch_idx;
                    m_row = 0;
                    continue;
                }
                if constexpr (has_changed_filter) {
                    if (m_row < arch->size() &&
                        !passes_changed_filters(*arch, m_row, m_last_run_tick)) {
                        advance_one();
                        continue;
                    }
                }
                if (m_row < arch->size()) break;
                ++m_arch_idx;
                m_row = 0;
            }
        }
    };

    // -----------------------------------------------------------------------
    // EntityView — lightweight range returned by with_entity()
    // -----------------------------------------------------------------------
    class EntityView {
    public:
        EntityView(const std::vector<Archetype*>& cached,
                   uint32_t last_run_tick, uint32_t current_tick)
            : m_cached(&cached)
            , m_last_run_tick(last_run_tick)
            , m_current_tick(current_tick) {}

        EntityIterator begin() const {
            return EntityIterator(*m_cached, 0, 0, m_last_run_tick, m_current_tick);
        }
        EntityIterator end() const {
            return EntityIterator(*m_cached, m_cached->size(), 0, m_last_run_tick, m_current_tick);
        }
    private:
        const std::vector<Archetype*>* m_cached;
        uint32_t m_last_run_tick;
        uint32_t m_current_tick;
    };

    // -----------------------------------------------------------------------
    // Range interface
    // -----------------------------------------------------------------------

    Iterator begin() const {
        return Iterator(m_cached, 0, 0, m_last_run_tick, m_current_tick);
    }

    Iterator end() const {
        return Iterator(m_cached, m_cached.size(), 0, m_last_run_tick, m_current_tick);
    }

    /// Return a view that yields (Entity, components...) tuples.
    EntityView with_entity() const {
        return EntityView(m_cached, m_last_run_tick, m_current_tick);
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

    /// The world tick at the time this query was created.
    uint32_t current_tick() const { return m_current_tick; }

    /// Retrieve components for a specific entity.
    /// Returns std::nullopt if the entity is not in a matching archetype.
    std::optional<ResultTuple> get(Entity entity) const {
        auto loc = m_storage->locate(entity);
        if (!loc.has_value()) return std::nullopt;

        Archetype* arch = loc->archetype;
        if (!matches(*arch)) return std::nullopt;

        return fetch_from(*arch, loc->row, m_current_tick);
    }

private:
    ArchetypeStorage* m_storage = nullptr;
    std::vector<Archetype*> m_cached;
    uint32_t m_last_run_tick = 0;   // tick when the owning system last ran
    uint32_t m_current_tick = 0;    // world tick when this query was created

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
        if constexpr (is_changed_v<P>) {
            // Changed<T> requires the component to be present (like With<T>).
            // Per-entity tick filtering happens in the iterator.
            return arch.has_component(component_id<filter_inner_t<P>>());
        } else if constexpr (is_with_v<P>) {
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

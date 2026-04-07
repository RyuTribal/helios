#pragma once
#include "helios/ecs/component_id.h"
#include "helios/ecs/column.h"
#include "helios/ecs/entity.h"
#include <vector>
#include <unordered_map>
#include "helios/core/assert.h"

namespace helios {

struct Archetype {
    ArchetypeId id;
    std::vector<Entity> entities;
    std::vector<Column> columns;
    std::unordered_map<ComponentId, size_t> column_index;

    bool has_component(ComponentId comp) const {
        return column_index.contains(comp);
    }

    template <typename... Ts>
    bool has_all() const {
        return (has_component(component_id<Ts>()) && ...);
    }

    template <typename... Ts>
    bool has_none() const {
        return (!has_component(component_id<Ts>()) && ...);
    }

    template <typename T>
    Column& get_column() {
        auto it = column_index.find(component_id<T>());
        HELIOS_ASSERT(it != column_index.end());
        return columns[it->second];
    }

    template <typename T>
    const Column& get_column() const {
        auto it = column_index.find(component_id<T>());
        HELIOS_ASSERT(it != column_index.end());
        return columns[it->second];
    }

    template <typename T>
    T& get(size_t row) {
        return get_column<T>().template get<T>(row);
    }

    template <typename T>
    const T& get(size_t row) const {
        return get_column<T>().template get<T>(row);
    }

    void swap_remove(size_t row) {
        HELIOS_ASSERT(row < entities.size());
        size_t last = entities.size() - 1;
        if (row != last) {
            entities[row] = entities[last];
        }
        entities.pop_back();

        for (auto& col : columns) {
            col.swap_remove(row);
        }
    }

    size_t size() const { return entities.size(); }
    bool empty() const { return entities.empty(); }
};

Archetype create_archetype(const ArchetypeId& id,
    const std::unordered_map<ComponentId, std::function<Column()>>& column_factories);

} // namespace helios

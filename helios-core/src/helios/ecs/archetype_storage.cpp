#include "helios/ecs/archetype_storage.h"
#include "helios/core/engine_log_channels.h"
#include "helios/core/assert.h"

namespace helios {

Archetype& ArchetypeStorage::get_or_create(const ArchetypeId& id) {
    auto it = m_archetypes.find(id);
    if (it != m_archetypes.end()) {
        return *it->second;
    }
    auto arch = std::make_unique<Archetype>(create_archetype(id, m_column_factories));
    auto* ptr = arch.get();
    m_archetypes.emplace(id, std::move(arch));
    HELIOS_LOG(ECS, Debug, "Created archetype with {} component(s)", id.size());
    return *ptr;
}

size_t ArchetypeStorage::add_entity(Archetype& archetype, Entity entity) {
    size_t row = archetype.entities.size();
    archetype.entities.push_back(entity);
    m_entity_locations[entity] = EntityLocation{&archetype, row};
    return row;
}

void ArchetypeStorage::remove_entity(Entity entity) {
    auto loc_it = m_entity_locations.find(entity);
    if (loc_it == m_entity_locations.end()) {
        return;
    }

    EntityLocation loc = loc_it->second;
    Archetype& arch = *loc.archetype;
    size_t row = loc.row;
    size_t last = arch.entities.size() - 1;

    // If removing an entity that is not the last, the swap-remove in
    // Archetype::swap_remove will move the last entity into this slot.
    // We need to update that entity's location.
    if (row != last) {
        Entity swapped = arch.entities[last];
        m_entity_locations[swapped].row = row;
    }

    arch.swap_remove(row);
    m_entity_locations.erase(loc_it);
}

void ArchetypeStorage::move_entity(Entity entity, Archetype& from, Archetype& to) {
    auto loc_it = m_entity_locations.find(entity);
    HELIOS_ASSERT(loc_it != m_entity_locations.end());
    HELIOS_LOG(ECS, Trace, "Moving entity {{index={}, gen={}}} between archetypes ({} -> {} components)",
        entity.index, entity.generation, from.id.size(), to.id.size());
    size_t src_row = loc_it->second.row;

    // Temporary buffer for moving component data between columns.
    // We use a stack buffer large enough for typical components;
    // heap-allocate for anything larger.
    static constexpr size_t STACK_BUF_SIZE = 256;
    alignas(64) std::byte stack_buf[STACK_BUF_SIZE];

    // Move shared component data from source columns to destination columns.
    for (auto& [comp_id, dst_col_idx] : to.column_index) {
        auto src_it = from.column_index.find(comp_id);
        if (src_it == from.column_index.end()) {
            // Component only in destination -- caller will push it afterward.
            continue;
        }

        Column& src_col = from.columns[src_it->second];
        Column& dst_col = to.columns[dst_col_idx];
        size_t elem_size = src_col.element_size();

        void* buf = (elem_size <= STACK_BUF_SIZE)
                         ? static_cast<void*>(stack_buf)
                         : static_cast<void*>(new std::byte[elem_size]);

        // Preserve the change-detection tick from the source column.
        uint32_t tick = src_col.changed_tick(src_row);
        src_col.move_out_and_swap_remove(src_row, buf);
        dst_col.push(buf, tick);
        // push() move-constructs from buf, leaving a moved-from object that
        // still needs its destructor called.
        dst_col.destroy_element(buf);

        if (elem_size > STACK_BUF_SIZE) {
            delete[] static_cast<std::byte*>(buf);
        }
    }

    // Swap-remove any component columns in source that are NOT in destination
    // (these are being "removed" from the entity).
    for (auto& [comp_id, src_col_idx] : from.column_index) {
        if (!to.column_index.contains(comp_id)) {
            from.columns[src_col_idx].swap_remove(src_row);
        }
    }

    // Swap-remove the entity from the source entity vector.
    size_t last = from.entities.size() - 1;
    if (src_row != last) {
        Entity swapped = from.entities[last];
        m_entity_locations[swapped].row = src_row;
        from.entities[src_row] = from.entities[last];
    }
    from.entities.pop_back();

    // Add entity to destination archetype.
    size_t dst_row = to.entities.size();
    to.entities.push_back(entity);
    m_entity_locations[entity] = EntityLocation{&to, dst_row};
}

std::optional<EntityLocation> ArchetypeStorage::locate(Entity entity) const {
    auto it = m_entity_locations.find(entity);
    if (it == m_entity_locations.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool ArchetypeStorage::contains(Entity entity) const {
    return m_entity_locations.contains(entity);
}

void ArchetypeStorage::set_location(Entity entity, Archetype* archetype, size_t row) {
    m_entity_locations[entity] = EntityLocation{archetype, row};
}

void ArchetypeStorage::for_each_archetype(const std::function<void(Archetype&)>& callback) {
    for (auto& [id, archetype] : m_archetypes) {
        callback(*archetype);
    }
}

void ArchetypeStorage::for_each_archetype(const std::function<void(const Archetype&)>& callback) const {
    for (const auto& [id, archetype] : m_archetypes) {
        callback(*archetype);
    }
}

} // namespace helios

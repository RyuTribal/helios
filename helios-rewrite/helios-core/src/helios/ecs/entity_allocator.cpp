#include "helios/ecs/entity_allocator.h"
#include "helios/core/assert.h"

namespace helios {

Entity EntityAllocator::allocate() {
    uint32_t index;
    if (!m_free_list.empty()) {
        index = m_free_list.back();
        m_free_list.pop_back();
        m_entries[index].alive = true;
    } else {
        index = static_cast<uint32_t>(m_entries.size());
        m_entries.push_back(Entry{.generation = 1, .alive = true});
    }
    ++m_alive_count;
    return Entity{index, m_entries[index].generation};
}

void EntityAllocator::deallocate(Entity entity) {
    HELIOS_ASSERT(entity.index < m_entries.size());
    auto& entry = m_entries[entity.index];
    HELIOS_ASSERT(entry.alive);
    HELIOS_ASSERT(entry.generation == entity.generation);
    entry.alive = false;
    entry.generation++;
    if (entry.generation == 0) entry.generation = 1;  // skip INVALID generation
    m_free_list.push_back(entity.index);
    --m_alive_count;
}

bool EntityAllocator::is_alive(Entity entity) const {
    if (entity.index >= m_entries.size()) return false;
    const auto& entry = m_entries[entity.index];
    return entry.alive && entry.generation == entity.generation;
}

} // namespace helios

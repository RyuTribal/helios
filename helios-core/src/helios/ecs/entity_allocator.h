#pragma once

#include "helios/ecs/entity.h"
#include <vector>
#include <cstdint>

namespace helios {

class EntityAllocator {
public:
    EntityAllocator() = default;
    Entity allocate();
    void deallocate(Entity entity);
    bool is_alive(Entity entity) const;
    size_t alive_count() const { return m_alive_count; }

private:
    struct Entry {
        uint32_t generation = 0;
        bool alive = false;
    };

    std::vector<Entry> m_entries;
    std::vector<uint32_t> m_free_list;
    size_t m_alive_count = 0;
};

} // namespace helios

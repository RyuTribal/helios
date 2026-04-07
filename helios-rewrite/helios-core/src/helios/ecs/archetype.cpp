#include "helios/ecs/archetype.h"
#include "helios/core/assert.h"

namespace helios {

Archetype create_archetype(const ArchetypeId& id,
    const std::unordered_map<ComponentId, std::function<Column()>>& column_factories) {
    Archetype arch;
    arch.id = id;
    arch.columns.reserve(id.size());
    for (size_t i = 0; i < id.size(); ++i) {
        auto it = column_factories.find(id[i]);
        HELIOS_ASSERT(it != column_factories.end());
        arch.columns.push_back(it->second());
        arch.column_index[id[i]] = i;
    }
    return arch;
}

} // namespace helios

#pragma once

#include "helios/ecs/world.h"
#include "helios/components/components.h"

#include <algorithm>

namespace helios {

/// Remove an entity from its parent (make it a root entity).
inline void unparent(World& world, Entity child) {
    if (!world.has<Parent>(child)) return;
    auto old_parent = world.get<Parent>(child).entity;
    if (auto* ch = world.try_get<Children>(old_parent)) {
        auto& vec = ch->entities;
        vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
        if (vec.empty()) world.remove<Children>(old_parent);
    }
    world.remove<Parent>(child);
}

/// Set parent of a child entity. Handles removing from old parent,
/// adding Parent component, and updating the new parent's Children list.
inline void set_parent(World& world, Entity child, Entity parent) {
    unparent(world, child);
    world.add(child, Parent{.entity = parent});
    if (auto* ch = world.try_get<Children>(parent)) {
        ch->entities.push_back(child);
    } else {
        world.add(parent, Children{.entities = {child}});
    }
}

} // namespace helios

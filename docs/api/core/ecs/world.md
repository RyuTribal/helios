# World

The `World` class is the central container for all ECS data in Helios. It manages entities, components, resources, and events.

## Methods

### Entity Management

| Method | Description |
| :--- | :--- |
| `spawn()` | Spawns a new entity with no components. |
| `spawn(Ts&&... components)` | Spawns a new entity with an initial set of components. |
| `spawn_with_children(F&& build_children, Ts&&... components)` | Spawns an entity and adds children to it in one call. |
| `spawn_child(Entity parent, Ts&&... components)` | Spawns an entity as a child of the given parent. |
| `despawn(Entity entity)` | Despawn an entity, removing it and all its components. |
| `is_alive(Entity entity)` | Checks whether an entity is still alive. |

### Component Access

| Method | Description |
| :--- | :--- |
| `get<T>(Entity entity)` | Gets a mutable reference to a component (asserts if missing). |
| `try_get<T>(Entity entity)` | Tries to get a pointer to a component (returns `nullptr` if missing). |
| `has<T>(Entity entity)` | Checks whether an entity has a given component. |
| `add<T>(Entity entity, T component)` | Adds or overwrites a component for an entity. |
| `remove<T>(Entity entity)` | Removes a component from an entity. |

### Resources

| Method | Description |
| :--- | :--- |
| `insert_resource<T>(T resource)` | Inserts a unique resource into the world. |
| `resource<T>()` | Gets a reference to a resource (asserts if missing). |
| `try_resource<T>()` | Tries to get a pointer to a resource (returns `nullptr` if missing). |
| `has_resource<T>()` | Checks whether a resource exists. |

### Events

| Method | Description |
| :--- | :--- |
| `register_event<T>()` | Registers a new event type. |
| `event_writer<T>()` | Gets an `EventWriter` for the given event type. |
| `event_reader<T>()` | Gets an `EventReader` for the given event type. |

### Queries & Commands

| Method | Description |
| :--- | :--- |
| `query<Params...>()` | Creates a new [Query](query.md) to iterate over entities matching criteria. |
| `pending_commands()` | Returns a reference to the world-owned pending [Commands](commands.md) buffer. |
| `apply_and_clear_pending_commands()` | Applies all deferred commands and clears the buffer. |

## Usage Example

```cpp
#include "helios/ecs/world.h"

helios::World world;

// Spawn an entity with components
auto entity = world.spawn(Position{0, 0}, Velocity{1, 1});

// Access components
if (auto* pos = world.try_get<Position>(entity)) {
    pos->x += 10;
}

// Manage resources
world.insert_resource(Time{0.0f});
float dt = world.resource<Time>().delta_time;
```

## Related Pages
- [Entity](entity.md)
- [Query](query.md)
- [Commands](commands.md)
- [App](../app/app.md)

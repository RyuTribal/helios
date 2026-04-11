# Commands

The `Commands` class allows for deferred entity and component operations, ensuring structural changes occur outside of query iteration.

## Commands Methods

| Method | Description |
| :--- | :--- |
| `spawn()` | Pre-allocates an entity and returns an `EntityBuilder` for deferred setup. |
| `despawn(Entity entity)` | Queues a deferred entity despawn. |
| `insert<T>(Entity entity, T component)` | Queues a deferred component addition or update. |
| `remove<T>(Entity entity)` | Queues a deferred component removal. |
| `insert_resource<T>(T resource)` | Queues a deferred resource insertion. |
| `apply(World& world)` | Executes all queued commands against the provided world. |
| `pending_count()` | Returns the number of commands currently queued. |

## EntityBuilder Methods

The `EntityBuilder` is returned by `Commands::spawn()` and allows for method chaining.

| Method | Description |
| :--- | :--- |
| `insert<T>(T component)` | Queues a deferred component addition to the new entity. |
| `id()` | Returns the pre-allocated `Entity` ID. |

## Usage Example

```cpp
#include "helios/ecs/commands.h"

void spawning_system(Commands& commands) {
    // Spawn with builder pattern
    auto player = commands.spawn()
        .insert(Position{0, 0})
        .insert(Player{})
        .id();

    // Deferred insertion on an existing entity
    commands.insert(some_entity, Tag{"Updated"});

    // Deferred despawn
    commands.despawn(dead_enemy);
}
```

## Related Pages
- [World](world.md)
- [Entity](entity.md)
- [App](../app/app.md)

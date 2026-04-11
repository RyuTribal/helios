# Query

The `Query<Params...>` class is used to iterate over entities and their components. It captures a snapshot of matching archetypes at construction time.

## Type Parameters

- `T`: Fetches a mutable reference (`T&`).
- `const T`: Fetches a constant reference (`const T&`).
- `With<T>`: Filter requiring a component without fetching it.
- `Without<T>`: Filter excluding a component.
- `Optional<T>`: Fetches an optional pointer (`T*`), which is `nullptr` if the component is absent.
- `Changed<T>`: Filter requiring a component that was modified since the system last ran.

## Methods

| Method | Description |
| :--- | :--- |
| `begin()` / `end()` | Standard iterators yielding component tuples. |
| `with_entity()` | Returns a view that yields `(Entity, components...)` tuples. |
| `count()` | Returns the total number of entities matching the query. |
| `is_empty()` | Returns `true` if no entities match the query. |
| `get(Entity entity)` | Retrieves components for a specific entity if it matches the query. |

## Usage Example

```cpp
#include "helios/ecs/world.h"

void move_system(World& world) {
    // Standard query for Velocity and mutable Position
    auto q = world.query<const Velocity, Position>();

    for (auto [vel, pos] : q) {
        pos.x += vel.x;
        pos.y += vel.y;
    }

    // Query with filters and Entity ID
    auto eq = world.query<With<Player>, Position>().with_entity();

    for (auto [entity, pos] : eq) {
        // ...
    }
}
```

## Related Pages
- [World](world.md)
- [Entity](entity.md)
- [Scheduler](scheduler.md)

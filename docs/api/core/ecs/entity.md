# Entity

The `Entity` struct is a lightweight ID that uniquely identifies an entity within a `World`. It uses a generational indexing system to handle entity reuse safely.

## Members

| Member | Description |
| :--- | :--- |
| `index` | The index of the entity in the global entity pool. |
| `generation` | The generation count of the entity at the time of allocation. |

## Methods & Operators

| Method | Description |
| :--- | :--- |
| `operator==` / `operator!=` | Compares two entity IDs for equality. |
| `operator bool()` | Returns `true` if the entity ID is valid. |
| `Entity::INVALID` | Constant representing an invalid entity ID. |

## Usage Example

```cpp
#include "helios/ecs/entity.h"

// Check for validity
helios::Entity e;
if (e) {
    // This will not execute for default-initialized entities.
}

// Store in a hash-map
std::unordered_map<helios::Entity, int> entity_scores;
entity_scores[e] = 100;
```

## Related Pages
- [World](world.md)
- [Query](query.md)
- [Commands](commands.md)

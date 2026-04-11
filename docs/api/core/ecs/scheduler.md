# Scheduler

The `Scheduler` class manages system registration, dependency resolution, and execution of systems within a `World`.

## Methods

### System Registration

| Method | Description |
| :--- | :--- |
| `add_system(Schedule schedule, F&& system, std::string name = "")` | Registers a system function to a schedule. |
| `add_system(Schedule schedule, SystemSet<F> set, std::string name = "")` | Registers a set of systems with ordering constraints. |
| `remove_systems_by_tag(std::type_index tag)` | Removes systems associated with a specific tag. |
| `id_of(const std::string& name)` | Returns the unique ID for a named system. |

### Execution

| Method | Description |
| :--- | :--- |
| `run(World& world, Schedule schedule)` | Runs all systems in a given schedule. |
| `rebuild_plan(Schedule schedule)` | Manually force a rebuild of the execution plan. |

### Configuration

| Method | Description |
| :--- | :--- |
| `set_parallel(bool enabled)` | Enables or disables parallel system execution. |
| `is_parallel()` | Checks whether parallel execution is currently enabled. |
| `enable_parallel(uint32_t thread_count = 0)` | Enables parallel execution with a specified thread count. |

## Usage Example

```cpp
#include "helios/ecs/scheduler.h"

void some_system(World& world) { /* ... */ }

helios::Scheduler scheduler;

// Add a system to the Update schedule
scheduler.add_system(helios::Schedule::Update, some_system, "my_system");

// Add another system with ordering constraints
scheduler.add_system(helios::Schedule::Update, other_system)
    .after(scheduler.id_of("my_system"));

// Execute a schedule
scheduler.run(world, helios::Schedule::Update);
```

## Related Pages
- [App](../app/app.md)
- [World](world.md)
- [Query](query.md)

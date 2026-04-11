# App

The `App` class is the top-level application builder in Helios. It orchestrates the [World](../ecs/world.md) and [Scheduler](../ecs/scheduler.md).

## Methods

### Setup & Registration

| Method | Description |
| :--- | :--- |
| `add_plugin<P>(P plugin)` | Adds a plugin to the application (prevents double-registration). |
| `insert_resource<T>(T resource)` | Registers a resource in the `World`. |
| `add_event<T>()` | Registers an event type in the `World`. |
| `add_system(Schedule schedule, F&& system)` | Registers a system into the `Scheduler`. |
| `id_of(const std::string& name)` | Returns the system ID for a named system. |

### Execution

| Method | Description |
| :--- | :--- |
| `run()` | Starts the main loop (blocks until the app is stopped). |
| `quit()` | Requests an application shutdown. |
| `tick()` | Runs a single frame (useful for testing). |

### Accessors

| Method | Description |
| :--- | :--- |
| `world()` | Returns a reference to the application's `World`. |
| `scheduler()` | Returns a reference to the application's `Scheduler`. |
| `enable_parallel(uint32_t count = 0)` | Enables parallel system execution with a given thread count. |

## Usage Example

```cpp
#include "helios/ecs/app.h"

int main() {
    helios::App app;
    
    app.add_plugin(DefaultPlugins{})
       .insert_resource(WindowConfig{1280, 720})
       .add_system(helios::Schedule::Update, my_system)
       .run();
       
    return 0;
}
```

## Related Pages
- [World](../ecs/world.md)
- [Scheduler](../ecs/scheduler.md)
- [GameFlow](game_flow.md)

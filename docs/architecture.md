# Helios Architecture

## Module Layout

Helios is a set of static libraries. Every module compiles to its own
`.a`/`.lib` and can be linked independently. The top-level CMake project
exposes each as a CMake target.

| Target | Library | Purpose |
|---|---|---|
| `helios-core` | ECS, assets, windowing, input, scenes, serialization | Required by everything |
| `helios-renderer` | RHI abstraction, Vulkan backend, Forward+ pipeline | GPU rendering |
| `helios-physics` | Jolt Physics integration | Rigid-body simulation |
| `helios-audio` | SoLoud integration | Sound playback, 3D spatial audio |
| `helios-script` | CoreCLR hosting, C# bridge | C# scripting |
| `helios-editor` | ImGui panels, gizmos, project system | Editor application |

### Dependency graph

```
              helios-editor
                    |
              helios-script
              /     |     \
  helios-renderer  helios-physics  helios-audio    (independent peers)
              \     |     /
              helios-core
```

Dependencies flow downward. **Renderer, Physics, and Audio are fully independent** —
they share no code and can be linked in any combination. Scripting sits above them
because it can call into all three. The editor sits at the top, consuming everything.
`helios-core` has zero engine-internal dependencies -- it only uses external
libraries (glm, yaml-cpp, GLFW, spdlog).

### Linking only what you need

A headless physics server can link `helios-core` + `helios-physics` and
nothing else:

```cmake
add_executable(my_server main.cpp)
target_link_libraries(my_server PRIVATE helios-core helios-physics)
```

The editor links everything:

```cmake
target_link_libraries(helios-editor PRIVATE
    helios-core helios-renderer helios-physics helios-audio helios-script)
```

## The Plugin System

Every module exposes a **plugin** -- a plain struct with a
`void build(App& app)` method. Plugins register resources, events, and
systems with the `App`:

```cpp
struct MyPlugin {
    void build(App& app) {
        app.insert_resource<MyConfig>({});
        app.add_event<MyEvent>();
        app.add_system(Schedule::Update, my_system, "my_system");
    }
};
```

Add plugins during setup:

```cpp
helios::App app;
app.add_plugin(RenderPlugin{.backend = rhi::Backend::Vulkan});
app.add_plugin(ForwardPlusPlugin{});
app.add_plugin(PhysicsPlugin<JoltPhysicsWorld>{});
app.add_plugin(AudioPlugin<SoLoudDevice>{});
app.add_plugin(ScriptingPlugin{.config = {/* paths */}});
app.run();
```

`add_plugin` is idempotent -- calling it twice with the same plugin type is a
no-op. This allows plugins to safely depend on each other without worrying
about double-registration.

The `Plugin` concept is minimal:

```cpp
template <typename T>
concept Plugin = requires(T plugin, App& app) {
    { plugin.build(app) } -> std::same_as<void>;
};
```

## App Main Loop

`App::run()` drives the frame loop. Each frame executes schedules in a fixed
order:

```
Startup          (once, before the first frame)
  |
  v
+----- frame loop -----+
| PreUpdate             |  input polling, event processing, physics auto-create
| Update                |  game logic, script execution
| FixedUpdate (N times) |  physics step (fixed timestep, up to 10 ticks/frame)
| PostUpdate            |  physics writeback, transform propagation prep
| [transform propagation]  hierarchy GlobalTransform recomputation
| PreRender             |  render extraction, editor UI, FramePacket submission
| [event buffer swap]
+-------+---------------+
        |
Shutdown                (once, after the loop exits)
```

### Schedule enum

```cpp
enum class Schedule : uint8_t {
    Startup,      // runs once at launch
    PreUpdate,    // input, event processing
    Update,       // game logic
    FixedUpdate,  // physics (fixed timestep)
    PostUpdate,   // cleanup, writeback
    PreRender,    // render data extraction, editor UI
    Shutdown,     // runs once before exit
};
```

### Fixed timestep

`FixedUpdate` runs zero or more times per frame, driven by
`FixedTimeAccumulator`. The accumulator adds the frame delta each frame, then
drains it in `timestep`-sized chunks (default 1/60 s). A cap of 10 ticks per
frame prevents spiral-of-death when the game hitches.

### Transform propagation

After `PostUpdate` and before `PreRender`, the engine walks the entity
hierarchy top-down and recomputes `GlobalTransform` from `Transform` +
parent `GlobalTransform`. This runs outside the scheduler because the
recursive hierarchy walk requires direct `World` access.

### Post-PreRender hook

The renderer installs a callback via `App::set_post_pre_render()` that
submits the `FramePacket` to the render thread after `PreRender` completes.
This keeps `helios-core` free of any renderer dependency.

## Shared Thread Pool

A single `ThreadPool` is created at `App` construction and shared across
the scheduler (parallel system execution), asset server (background loading),
and editor (script builds). Access it as a resource:

```cpp
auto& pool = world.resource<std::shared_ptr<ThreadPool>>();
pool->submit([] { /* background work */ });
```

By default, the pool creates `hardware_concurrency - 1` worker threads
(minimum 1).

## Build Configuration

The root `CMakeLists.txt` uses C++20, fetches dependencies via
`FetchContent`, and exposes two options:

| Option | Default | Description |
|---|---|---|
| `HELIOS_BUILD_TESTS` | ON | Build unit tests (GTest) |
| `HELIOS_BUILD_EDITOR` | ON | Build the editor executable |

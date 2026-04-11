# Helios Architecture

Helios is designed as a modular, high-performance engine inspired by modern ECS architectures. It prioritizes decoupled systems, data-oriented design, and a flexible plugin-based extension model.

## Module Layout & Dependencies

Helios is composed of a collection of **static libraries**. This modularity ensures that the engine is lightweight and that developers only pay for what they use. Every module compiles to its own `.a` (Linux) or `.lib` (Windows) file and is exposed as a distinct CMake target.

| Target | Library | Purpose |
|---|---|---|
| `helios-core` | Core ECS, Asset Server, Windowing, Input, Scenes, Serialization | The foundation of the engine. Required by all other modules. |
| `helios-renderer` | RHI abstraction, Vulkan 1.3 backend, Forward+ pipeline | High-performance GPU rendering and frame-graph management. |
| `helios-physics` | Jolt Physics integration | Rigid-body simulation and collision detection. |
| `helios-audio` | SoLoud integration | Sound playback and 3D spatial audio. |
| `helios-script` | .NET CoreCLR hosting, C# bridge | High-level scripting and hot-reload support. |
| `helios-editor` | ImGui panels, gizmos, project management | The engine's visual development environment. |

### Dependency Graph

The architecture follows a strict "core-outward" dependency model:

```
              helios-editor
                    |
              helios-script
              /     |     \
  helios-renderer  helios-physics  helios-audio    (independent peers)
              \     |     /
              helios-core
```

*   **`helios-core` is the bedrock:** It contains the ECS, the main `App` loop, and core utilities. It has zero engine-internal dependencies, relying only on external primitives like GLM, spdlog, and GLFW.
*   **Independent Peers:** The Renderer, Physics, and Audio modules are **fully independent**. They share no code with each other and only depend on `helios-core`. This allows for specialized builds—such as a headless physics server that links only `core` and `physics`.
*   **Bridge Layers:** `helios-script` sits above the peers to provide a unified C# API. `helios-editor` sits at the top, consuming all modules to provide the full engine experience.

## The Plugin System

Plugins are the primary mechanism for extending Helios. A **Plugin** is a simple struct that implements a `build(App& app)` method, where it registers resources, events, and systems.

### Custom Plugin Example

Here is how you might implement a plugin that adds a custom gameplay resource and a system:

```cpp
struct GameplayPlugin {
    void build(App& app) {
        // Register a resource used by systems
        app.insert_resource<ScoreCounter>({ .points = 0 });
        
        // Register an event type
        app.add_event<LevelUpEvent>();
        
        // Add a system to the Update schedule
        app.add_system(Schedule::Update, update_score_system, "update_score");
    }
    
    static void update_score_system(ResMut<ScoreCounter> score, EventWriter<LevelUpEvent> events) {
        // System logic here...
    }
};
```

### Idempotency and Composition

The `App::add_plugin` method is **idempotent**. If a plugin type is added multiple times, only the first call executes. This is crucial for composition: if `PluginB` depends on `PluginA`, it can safely call `app.add_plugin<PluginA>()` in its own `build` method without risking double-registration.

```cpp
helios::App app;
app.add_plugin(RenderPlugin{.backend = rhi::Backend::Vulkan});
app.add_plugin(ForwardPlusPlugin{}); // ForwardPlus internally adds RenderPlugin if missing
app.run();
```

## App Lifecycle & Main Loop

`App::run()` is the entry point that drives the engine's execution. It initializes the `Startup` schedule and then enters a continuous loop that calls `tick()` until the application is closed.

### The Schedule Pipeline

Each frame follows a deterministic execution order across several specialized schedules:

1.  **`Startup`**: Runs exactly once when the app starts. Used for one-time initialization (e.g., spawning a camera).
2.  **`PreUpdate`**: Input polling, event processing, and engine-internal preparation (e.g., physics body creation).
3.  **`Update`**: The main stage for game logic and script execution.
4.  **`FixedUpdate`**: Driven by a **Fixed Timestep**. Used for physics simulation and deterministic logic.
5.  **`PostUpdate`**: Final cleanup, physics state write-back to ECS, and preparation for rendering.
6.  **`PreRender`**: Extraction of ECS data into render-friendly structures (Frame Packets) and Editor UI submission.
7.  **`Shutdown`**: Runs once after the loop exits to flush the GPU and cleanup resources.

### Fixed Timestep Logic

Helios uses a `FixedTimeAccumulator` to decouple simulation logic from the variable frame rate.

*   **`remaining`**: The "bank" of time accumulated from frame deltas.
*   **`timestep`**: The target interval for each tick (e.g., 1/60s).
*   **The 10-Tick Cap**: To prevent the "spiral of death" (where a slow frame causes more physics ticks, which slows down the next frame), `FixedUpdate` is capped at 10 ticks per frame. If the cap is hit, the accumulator is cleared to allow the engine to catch up.

### Transform Propagation

Between `PostUpdate` and `PreRender`, the engine performs **Transform Propagation**. It traverses the entity hierarchy and recomputes `GlobalTransform` components based on their local `Transform` and their parent's `GlobalTransform`. This ensures that the renderer always receives up-to-date world-space coordinates.

### Render Submission Hook

To keep `helios-core` decoupled from the renderer, the `App` provides a **Post-PreRender hook**. The renderer installs a callback via `set_post_pre_render()` that submits the extracted `FramePacket` to the render thread immediately after the `PreRender` schedule completes.

## Shared Thread Pool

Helios utilizes a single, globally-shared `ThreadPool` created at `App` construction. This pool is a critical resource used by several engine subsystems:

*   **Scheduler**: Parallel execution of ECS systems that don't have data dependencies.
*   **Asset Server**: Background loading and decoding of textures, meshes, and audio files.
*   **Editor**: Parallel compilation of C# scripts and asset importing.

### Accessing the Pool

You can access the thread pool from any system by requesting it as a resource:

```cpp
void background_task_system(World& world) {
    auto& pool = world.resource<std::shared_ptr<ThreadPool>>();
    
    pool->submit([] {
        // Perform heavy background work here
        HELIOS_LOG(Game, Info, "Task running in background!");
    });
}
```

By default, the pool initializes with `hardware_concurrency - 1` worker threads, ensuring the main thread remains responsive for window events and OS interaction.

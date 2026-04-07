# Helios Engine Rewrite - Architecture Design Spec

## Goal

Rewrite the Helios engine from a singleton-heavy, single-threaded, tightly-coupled architecture to a modular, data-oriented, multi-threaded engine with clean abstractions. The engine should be usable standalone (no editor dependency), support multiple rendering/physics/audio backends, and enable automatic system parallelism through a Bevy-inspired ECS.

## Architecture Overview

```
App
 ├── World (ECS)
 │    ├── Archetypes (component storage)
 │    ├── Resources (Time, Input, AssetServer, RenderSettings...)
 │    ├── Events (typed channels)
 │    └── Scheduler (auto-parallel system execution)
 │
 ├── Plugins
 │    ├── WindowPlugin (GLFW, multi-window)
 │    ├── InputPlugin (action mapping)
 │    ├── VulkanRenderPlugin (RHI + render graph + Forward+)
 │    ├── PhysicsPlugin<JoltBackend>
 │    ├── AudioPlugin<SoLoudBackend>
 │    ├── ScriptingPlugin (C# CoreCLR)
 │    └── EditorPlugin (ImGui, panels, gizmos - EDITOR ONLY)
 │
 └── Render Thread (consumes RenderQueue, fully independent)
```

## Design Principles

1. **No global state.** No singletons, no static mutable variables. Everything accessed through system parameters (queries, resources, events, commands).
2. **RAII everywhere.** Construct in constructor, destroy in destructor. No `Init()`/`Shutdown()`. Resize = destroy + reconstruct unless performance demands otherwise.
3. **Power bottom, convenience top.** Every subsystem exposes low-level primitives AND a convenient high-level API. Both are public. Users can hijack any layer.
4. **Components are plain data.** No inheritance, no virtual methods, no smart pointers. Use `AssetHandle` for references. Aggregates only (enables automatic reflection).
5. **Systems are free functions (or state class methods).** They declare data access through parameters. The scheduler uses this for automatic parallelism.
6. **Engine knows nothing about the editor.** ImGui, inspector panels, gizmos, undo/redo are all editor-only plugins. The engine is a pure runtime.

---

## 1. ECS Core

### 1.1 Archetype Storage

Entities with the same component set share an archetype. Each archetype stores components in contiguous arrays (Structure of Arrays within each archetype).

```
Archetype<Transform, MeshRenderer>:
  Entity IDs:  [E1, E2, E3, ...]     (dense array)
  Transforms:  [T1, T2, T3, ...]     (dense array, same indices)
  MeshRenderers: [M1, M2, M3, ...]   (dense array, same indices)

Archetype<Transform, PointLight>:
  Entity IDs:  [E4, E5, ...]
  Transforms:  [T4, T5, ...]
  PointLights: [L4, L5, ...]
```

**Entity:** A lightweight ID (generational index internally, UUID for serialization).

```cpp
struct Entity {
    uint32_t index;
    uint32_t generation;
};
```

**Archetype operations:**
- Spawn: append to matching archetype's arrays (or create new archetype)
- Despawn: swap-remove from archetype arrays
- Add component: move entity from old archetype to new archetype (old + new component)
- Remove component: move entity from old archetype to new archetype (old - removed component)

**Structural changes** (spawn, despawn, add/remove component) are deferred via `Commands` and applied between system executions. This ensures iteration is never invalidated mid-system.

### 1.2 Components

Components are plain aggregate structs. No inheritance, no constructors, no smart pointers.

```cpp
struct Transform {
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1};

    glm::mat4 to_mat4() const; // utility method is fine, no state
};

struct MeshRenderer {
    AssetHandle mesh;
    AssetHandle material;
};

struct PointLight {
    glm::vec3 color{1};
    float intensity = 1.0f;
    float radius = 10.0f;
};

struct RigidBody {
    BodyType type = BodyType::Dynamic;
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
};

struct Parent { Entity entity; };
struct Children { std::vector<Entity> entities; };
struct Tag { std::string name; };
```

### 1.3 Reflection

Primary: **qlibs/reflect** for automatic zero-boilerplate reflection on aggregate components. Fallback: **entt::meta** for complex types that cannot be aggregates.

```cpp
// Automatic - works on any aggregate
reflect::for_each([](auto I) {
    auto name = reflect::member_name<I>(component);
    auto& value = reflect::get<I>(component);
}, component);

// Editor inspector, serializer, scripting bridge all use this
// No registration code needed
```

### 1.4 Systems

Systems are free functions. Parameters declare data access. The scheduler analyzes parameters for automatic parallelism.

```cpp
// Read Transform and Enemy, write nothing else
void move_enemies(Query<Transform, const Enemy> query, Res<Time> time) {
    for (auto [transform, enemy] : query) {
        transform.position += enemy.direction * enemy.speed * time->delta();
    }
}

// Query filters
void render_active_meshes(
    Query<const Transform, const MeshRenderer, Without<Disabled>> query
) { ... }
```

**Parameter types:**
- `Query<T...>` - iterate entities with these components. Mutable unless `const`.
- `Res<T>` - read-only resource access.
- `ResMut<T>` - mutable resource access.
- `Commands&` - deferred entity/component operations.
- `EventReader<T>` - receive events.
- `EventWriter<T>` - send events.

### 1.5 Resources

World-owned singletons. Replace all current global singletons.

```cpp
struct Time {
    float m_delta = 0;
    float m_elapsed = 0;
    float delta() const { return m_delta; }
    float elapsed() const { return m_elapsed; }
};

struct InputState { ... };
struct RenderSettings { ... };
struct PhysicsConfig { ... };
struct AssetServer { ... };
```

Inserted via plugins: `app.insert_resource<Time>(Time{});`
Accessed via system parameters: `Res<Time>`, `ResMut<Time>`

### 1.6 Events

Typed channels. Events live for one frame (read during the frame they're sent, cleared at frame end).

```cpp
struct CollisionEvent { Entity a, b; glm::vec3 point; glm::vec3 normal; };
struct WindowResized { uint32_t width, height; };
struct AssetLoaded { AssetHandle handle; AssetType type; };

// Send
void detect_hits(EventWriter<CollisionEvent> writer) {
    writer.send(CollisionEvent{ ... });
}

// Receive
void play_impact(EventReader<CollisionEvent> reader, Res<AudioDevice> audio) {
    for (auto& e : reader) {
        audio->play_at(e.point, "impact.wav");
    }
}
```

### 1.7 Scheduler

Systems are grouped into schedules that run in a fixed order. Within a schedule, systems run in parallel when their data access doesn't conflict.

```cpp
enum class Schedule {
    Startup,       // once at app launch
    PreUpdate,     // input polling, event dispatch
    Update,        // game logic
    FixedUpdate,   // physics (fixed timestep, N ticks per frame)
    PostUpdate,    // transform propagation, cleanup
    PreRender,     // render extraction, editor UI
};
```

**Parallelism rules:**
- Two systems that both READ the same component: parallel.
- Two systems where one WRITES a component the other reads: sequential.
- Two systems that write different components: parallel.
- `Commands&` is always exclusive (applied between systems).

**Explicit ordering when needed:**
```cpp
app.add_system(Schedule::Update, spawn_bullets.after(player_input));
```

**FixedUpdate:** Runs N times per frame to maintain a fixed timestep (e.g., 60Hz). Accumulates frame delta, ticks physics in fixed increments. Interpolation between physics states for smooth rendering.

---

## 2. State Management

### 2.1 State Stack

States are classes with RAII lifecycle. A stack determines what's active.

```cpp
class Playing : public State<GameState> {
public:
    Playing(World& world) { /* spawn gameplay entities */ }
    ~Playing() { /* auto-despawn tracked entities */ }

    void player_movement(Query<Transform, const Player> q, Res<InputState> input) { ... }
    void enemy_ai(Query<Transform, const Enemy> q, Res<Time> time) { ... }

    static void describe(StateBuilder<Playing>& s) {
        s.opaque();  // states below don't update
        s.system(&Playing::player_movement);
        s.system(&Playing::enemy_ai);
    }
};
```

**Stack modifiers:**
- `opaque()` - states below stop updating and their entities are hidden
- `transparent()` - states below keep running (HUD overlay)
- `pause_below()` - states below stop updating but entities remain visible (pause menu)

**Stack operations:**
- `flow->go_to<Playing, HUD>()` - clear stack, push these
- `flow->push<PauseMenu>()` - push on top
- `flow->pop()` - remove top
- `flow->switch_to<GameOver>()` - replace top

### 2.2 Transitions

```cpp
app.transition(GameState::MainMenu, GameState::Playing)
    .via<LoadingScreen>();

class LoadingScreen : public State<GameState> {
public:
    LoadingScreen(World& world) {
        m_batch = world.resource<AssetServer>().load_batch()
            .add<Mesh>("level.gltf")
            .add<Texture>("terrain.png")
            .submit();
    }

    void check_progress(Res<LoadBatch> batch, ResMut<GameFlow<GameState>> flow) {
        draw_progress(batch->progress());
        if (batch->is_complete()) flow->pop(); // proceeds to Playing
    }

    static void describe(StateBuilder<LoadingScreen>& s) {
        s.opaque();
        s.system(&LoadingScreen::check_progress);
    }

private:
    LoadBatchHandle m_batch;
};
```

---

## 3. Rendering

### 3.1 RHI (Rendering Hardware Interface)

Thin Forge-style abstraction. GPU concepts are explicit (command buffers, descriptor sets, pipelines) but unified across backends. Compile-time backend selection.

```cpp
// Core RHI types (backend-agnostic)
class Device;
class CommandBuffer;
class Texture;
class Buffer;
class Pipeline;
class DescriptorSet;
class RenderPass;
class Swapchain;  // RAII: construct = create, destruct = destroy, resize = reconstruct

// Device is the factory
class Device {
public:
    Texture create_texture(const TextureDesc& desc, const void* initial_data = nullptr);
    Buffer create_buffer(const BufferDesc& desc, const void* initial_data = nullptr);
    Pipeline create_graphics_pipeline(const GraphicsPipelineDesc& desc);
    Pipeline create_compute_pipeline(const ComputePipelineDesc& desc);
    DescriptorSet create_descriptor_set(const DescriptorSetLayout& layout);
    CommandBuffer create_command_buffer();
    Swapchain create_swapchain(const SwapchainDesc& desc);

    void submit(CommandBuffer& cmd, const SubmitInfo& info);
    void wait_idle();

    // Escape hatch
    template<typename T> T* native_handle();
    // e.g., native_handle<VkDevice>() returns raw Vulkan device
};
```

**RAII resources:** All RHI types own their GPU resources. Destructor frees them. No `Destroy()` methods. Move-only (no copy).

```cpp
{
    auto texture = device.create_texture(desc);
    // use texture...
} // texture destroyed here, GPU resource freed

// Resize = reconstruct
swapchain = device.create_swapchain(new_desc); // old swapchain destroyed by move-assignment
```

**Compile-time backend:**
```cpp
// CMake selects the backend at build time
// Only one backend compiled in (no virtual dispatch overhead)
#if defined(HELIOS_BACKEND_VULKAN)
using Device = VulkanDevice;
using Texture = VulkanTexture;
// ...
#elif defined(HELIOS_BACKEND_D3D12)
using Device = D3D12Device;
// ...
#endif
```

### 3.2 Render Graph

Frostbite-style frame graph. Passes are nodes, resources are edges. Auto-manages barriers, lifetimes, and memory aliasing.

```cpp
struct RenderGraph {
    // Declare a transient texture (lifetime managed by graph)
    TextureHandle create_texture(const TextureDesc& desc);

    // Import an external texture (swapchain, persistent texture)
    TextureHandle import_texture(Texture& external);

    // Add a render pass
    template<typename Data, typename Setup, typename Execute>
    void add_pass(const char* name, Setup&& setup, Execute&& execute);
};
```

**Pass declaration:**
```cpp
struct ForwardPassData {
    TextureHandle color;
    TextureHandle depth;
    BufferHandle global_ubo;
};

graph.add_pass<ForwardPassData>("ForwardPass",
    // Setup: declare inputs/outputs
    [&](ForwardPassData& data, RenderGraphBuilder& builder) {
        data.depth = builder.read(depth_prepass_output);      // input
        data.color = builder.create(color_desc);              // output
        data.global_ubo = builder.read(global_ubo_handle);
    },
    // Execute: record GPU commands (only runs if pass isn't culled)
    [&](const ForwardPassData& data, RenderContext& ctx) {
        ctx.bind_pipeline(forward_pipeline);
        ctx.bind_descriptor_set(0, global_descriptors);
        for (auto& draw : ctx.draw_list()) {
            ctx.bind_descriptor_set(1, draw.material_descriptors);
            ctx.draw_indexed(draw.mesh);
        }
    }
);
```

**Three-phase execution:**
1. **Setup:** All passes declare their resource requirements.
2. **Compile:** Graph computes execution order, inserts barriers, culls unused passes, identifies memory aliasing.
3. **Execute:** Surviving passes record GPU commands.

The graph is rebuilt every frame (cheap - it's just metadata).

### 3.3 ForwardPlus Plugin

Default rendering pipeline as a plugin. Builds the standard render graph.

```cpp
struct ForwardPlusPlugin {
    void build(App& app) {
        app.insert_resource<ForwardPlusConfig>({
            .hdr_format = TextureFormat::RGBA16F,
            .shadow_resolution = 4096,
            .shadow_cascades = 4,
        });
        app.add_system(Schedule::PreRender, build_forward_plus_graph);
    }
};

void build_forward_plus_graph(
    ResMut<RenderGraph> graph,
    Res<RenderQueue> queue,
    Res<ForwardPlusConfig> config
) {
    auto depth = add_depth_prepass(graph, queue);
    auto shadows = add_shadow_pass(graph, queue, config);
    auto culling = add_light_culling(graph, depth, queue);
    auto hdr = add_forward_pass(graph, depth, shadows, culling, queue);
    auto skybox = add_skybox_pass(graph, hdr, queue);
    auto ldr = add_tonemap_pass(graph, hdr, config);
    graph->set_output(ldr);
}
```

### 3.4 Render Thread

Separate thread that consumes frame packets. Never touches the World.

**Frame flow:**
1. Main thread: ECS systems run, including render extraction system.
2. Render extraction system builds a `FramePacket` (transforms, meshes, materials, lights, camera).
3. Main thread submits `FramePacket` to render thread via double-buffered swap.
4. Render thread: builds render graph from `FramePacket`, compiles, executes on GPU.
5. Render thread can lag 1-2 frames behind main thread.

```cpp
struct FramePacket {
    CameraData camera;
    std::vector<MeshDraw> mesh_draws;     // transform + mesh + material
    std::vector<LightData> point_lights;
    std::vector<LightData> dir_lights;
    SkyboxData skybox;
    // ... everything the GPU needs, no World references
};
```

### 3.5 Shader System

Cross-backend shader abstraction. Source shaders compile to SPIR-V (Vulkan), DXIL (D3D12), MSL (Metal) at build time.

```
Editor/Resources/Shaders/
  forward.hlsl          (source, cross-backend HLSL)
  forward.vert.spv      (compiled Vulkan)
  forward.frag.spv      (compiled Vulkan)
  forward.vert.dxil     (compiled D3D12, future)
```

Build-time compilation via CMake custom commands. Shader hot-reload in development (file watcher triggers recompile + pipeline recreation).

---

## 4. Physics

### 4.1 Physics Interface

Backend-abstracted. Engine defines the interface, Jolt implements it.

```cpp
// Engine interface
class PhysicsWorld {
public:
    virtual ~PhysicsWorld() = default;
    virtual BodyHandle create_body(const BodyDesc& desc) = 0;
    virtual void destroy_body(BodyHandle handle) = 0;
    virtual void set_transform(BodyHandle handle, const glm::vec3& pos, const glm::quat& rot) = 0;
    virtual Transform get_transform(BodyHandle handle) const = 0;
    virtual void step(float dt) = 0;
    virtual std::vector<ContactEvent> get_contacts() const = 0;
};

// Jolt backend
class JoltPhysicsWorld : public PhysicsWorld { ... };
```

### 4.2 Physics Plugin

```cpp
template<typename Backend>
struct PhysicsPlugin {
    void build(App& app) {
        app.insert_resource<PhysicsConfig>({ .gravity = {0, -9.81f, 0} });
        app.insert_resource<PhysicsWorld>(Backend::create(config));
        app.add_system(Schedule::FixedUpdate, physics_step);
        app.add_system(Schedule::PostUpdate, sync_physics_transforms);
        app.add_event<CollisionEvent>();
    }
};

void physics_step(ResMut<PhysicsWorld> world, Res<PhysicsConfig> config) {
    world->step(config.fixed_timestep);
    // emit collision events from world->get_contacts()
}

void sync_physics_transforms(
    Query<Transform, const RigidBody> bodies,
    Res<PhysicsWorld> world
) {
    for (auto [transform, rb] : bodies) {
        transform = world->get_transform(rb.body_handle);
    }
}
```

**FixedUpdate interpolation:** The renderer reads interpolated transforms (blend between previous and current physics state based on accumulator remainder) for smooth visual output at variable framerates.

---

## 5. Audio

### 5.1 Audio Interface

```cpp
class AudioDevice {
public:
    virtual ~AudioDevice() = default;
    virtual SoundHandle play(AssetHandle clip, const PlayParams& params = {}) = 0;
    virtual SoundHandle play_at(AssetHandle clip, const glm::vec3& position, const PlayParams& params = {}) = 0;
    virtual void stop(SoundHandle handle) = 0;
    virtual void set_listener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up) = 0;
};

class SoLoudDevice : public AudioDevice { ... };
```

### 5.2 Audio Plugin

```cpp
template<typename Backend>
struct AudioPlugin {
    void build(App& app) {
        app.insert_resource<AudioDevice>(Backend::create());
        app.add_system(Schedule::PostUpdate, update_audio_listener);
        app.add_system(Schedule::PostUpdate, update_spatial_audio);
    }
};
```

---

## 6. Scripting

### 6.1 Per-Entity Scripts, Batched as Systems

C# scripts are per-entity (familiar Unity/Godot model). The engine batches all instances of the same script type into one system for parallel execution.

```csharp
// C# user code
[System(Schedule.Update)]
public class EnemyAI : Script
{
    public float Speed = 5.0f;

    public override void OnUpdate(float delta) {
        var transform = Entity.Get<Transform>();
        transform.Position += Entity.Forward * Speed * delta;
    }
}
```

**Engine-side execution:**
1. Group all entities with `ScriptComponent<EnemyAI>` into one batch.
2. Schedule as a system with declared access (Transform write, EnemyAI read).
3. Different script types run in parallel when data access doesn't conflict.

### 6.2 Script Hot Reload

File watcher detects C# changes. Reload via collectible AssemblyLoadContext (already implemented). Script state is serialized before unload, deserialized after reload.

---

## 7. Asset System

### 7.1 Async Default

```cpp
// Async (default) - returns handle immediately
AssetHandle mesh = assets.load<Mesh>("helmet.gltf");

// Sync (opt-in) - blocks until loaded
AssetHandle splash = assets.load_sync<Texture>("splash.png");
```

### 7.2 Batch Loading with Progress

```cpp
auto batch = assets.load_batch()
    .add<Mesh>("helmet.gltf")
    .add<Texture>("terrain.png")
    .add<AudioClip>("music.ogg")
    .submit();

// In loading screen system
float progress = batch.progress();     // 0.0 - 1.0
int remaining = batch.remaining();     // count
bool done = batch.is_complete();
auto errors = batch.failed();          // list of failed assets
```

### 7.3 Asset Events

```cpp
app.add_event<AssetLoaded>();

void on_mesh_ready(EventReader<AssetLoaded> events, Res<AssetServer> assets) {
    for (auto& e : events) {
        if (e.type == AssetType::Mesh) {
            auto& mesh = assets.get<Mesh>(e.handle);
            // mesh is ready to use
        }
    }
}
```

### 7.4 Serialization

- **YAML** for development (human-readable, git-diffable).
- **Binary** for runtime (fast load, compact).
- Editor exports YAML to binary on build.
- Component serialization auto-generated from reflection (qlibs/reflect).

---

## 8. Input System

### 8.1 Raw Input (Power Layer)

```cpp
struct RawInput {
    bool key_pressed(KeyCode key) const;
    bool key_just_pressed(KeyCode key) const;
    bool key_just_released(KeyCode key) const;
    glm::vec2 mouse_position() const;
    glm::vec2 mouse_delta() const;
    float scroll_delta() const;
    bool mouse_button(MouseButton btn) const;
};
```

### 8.2 Action Mapping (Convenience Layer)

```cpp
// Define actions
app.insert_resource<InputMap>(InputMap{}
    .action("jump", Key::Space, GamepadButton::A)
    .action("fire", MouseButton::Left, GamepadButton::RightTrigger)
    .axis("move_x", Key::D, Key::A, GamepadAxis::LeftX)
    .axis("move_y", Key::W, Key::S, GamepadAxis::LeftY)
);

// Use in systems
void player_input(Res<InputMap> input) {
    if (input->just_pressed("jump")) { ... }
    float move_x = input->axis("move_x"); // -1.0 to 1.0
}
```

---

## 9. Window System

### 9.1 Multi-Window Support

```cpp
struct WindowPlugin {
    WindowDesc primary_window;

    void build(App& app) {
        app.insert_resource<Windows>(Windows{});
        auto& windows = app.resource_mut<Windows>();
        windows.create(primary_window);
        app.add_system(Schedule::PreUpdate, poll_window_events);
        app.add_event<WindowResized>();
        app.add_event<WindowClosed>();
    }
};

// Create additional windows (e.g., detachable editor viewports)
void open_viewport(ResMut<Windows> windows) {
    windows->create(WindowDesc{ .title = "Viewport 2", .width = 800, .height = 600 });
}
```

---

## 10. Engine/Editor Separation

### 10.1 Engine

The engine is a set of CMake library targets. It knows nothing about ImGui, editor panels, gizmos, or undo/redo.

**Engine public API:** World, App, Plugin system, ECS, RHI, Render Graph, Physics/Audio/Script interfaces, Asset system, Input, Window.

### 10.2 Editor

The editor is a separate CMake executable target that links against engine libraries. It adds editor-specific plugins.

```cpp
struct EditorPlugin {
    void build(App& app) {
        app.add_plugin<ImGuiPlugin>();
        app.insert_resource<EditorState>(EditorState{});
        app.add_system(Schedule::PreRender, scene_hierarchy_panel);
        app.add_system(Schedule::PreRender, inspector_panel);
        app.add_system(Schedule::PreRender, content_browser_panel);
        app.add_system(Schedule::PreRender, viewport_panel);
        app.add_system(Schedule::PreRender, gizmo_system);
    }
};
```

### 10.3 Editor Undo/Redo

`EditorCommands` wraps `Commands` with diff recording. Engine doesn't know undo exists.

```cpp
struct EditorCommands {
    void set(Entity e, const Transform& new_val);
    // internally: saves old value, applies new, pushes to undo stack

    void undo(); // replays in reverse
    void redo(); // replays forward
};
```

---

## 11. CMake Module Structure

```
helios/
  CMakeLists.txt (root)
  helios-core/          → libhelios-core.a
    ecs/                (World, Archetype, Query, System, Scheduler)
    math/               (glm wrappers, BoundingBox, Ray)
    core/               (App, Plugin, Window, Input, Logging, Timer)
    assets/             (AssetServer, async loading, serialization)
    platform/           (GLFW window, platform input)

  helios-renderer/      → libhelios-renderer.a
    rhi/                (Device, Texture, Buffer, Pipeline - backend-agnostic types)
    vulkan/             (VulkanDevice, VulkanTexture, etc.)
    graph/              (RenderGraph, RenderPass, ResourceLifetime)
    forward_plus/       (ForwardPlusPlugin, standard pass implementations)
    shaders/            (cross-backend shader sources)

  helios-physics/       → libhelios-physics.a
    interface/          (PhysicsWorld, BodyHandle, ContactEvent)
    jolt/               (JoltPhysicsWorld backend)

  helios-audio/         → libhelios-audio.a
    interface/          (AudioDevice, SoundHandle)
    soloud/             (SoLoudDevice backend)

  helios-script/        → libhelios-script.a
    interface/          (ScriptRuntime, ScriptComponent)
    coreclr/            (CoreCLR backend, HostFXR bridge)

  helios-editor/        → helios-editor executable
    imgui/              (ImGui integration, ImGui render backend)
    panels/             (Hierarchy, Inspector, ContentBrowser, Viewport)
    gizmo/              (ImGuizmo integration)
    commands/           (EditorCommands, undo/redo)

  vendor/               (third-party: GLFW, imgui, Jolt, SoLoud, glm, yaml-cpp, Tracy, etc.)
  scripts/              (CMake helpers, shader compilation)
```

**Dependency graph:**
```
helios-core         (no engine deps, only vendor: glm, yaml-cpp, GLFW)
helios-renderer     → helios-core
helios-physics      → helios-core
helios-audio        → helios-core
helios-script       → helios-core
helios-editor       → all of the above + imgui + ImGuizmo
```

---

## 12. Testing

### 12.1 Headless Mode

The engine can run without a window or GPU. Systems that don't touch rendering work normally. The render extraction system produces a FramePacket but nothing consumes it.

```cpp
App app;
app.add_plugin<HeadlessPlugin>(); // no window, no GPU
app.add_plugin<PhysicsPlugin<JoltBackend>>();
// physics systems run, renderer doesn't
```

### 12.2 Mock Backends

Physics, Audio, and RHI interfaces can be mocked for unit testing.

```cpp
app.add_plugin<PhysicsPlugin<MockPhysicsBackend>>();
// test game logic without Jolt
```

### 12.3 ECS Testing

```cpp
// Create a test world, add systems, step manually
World world;
world.spawn().insert(Transform{}).insert(Enemy{ .speed = 5.0f });
world.insert_resource<Time>(Time{ .m_delta = 0.016f });
world.run_system(move_enemies);

auto& t = world.query_single<Transform, const Enemy>();
EXPECT_NEAR(t.position.x, 0.08f, 0.001f);
```

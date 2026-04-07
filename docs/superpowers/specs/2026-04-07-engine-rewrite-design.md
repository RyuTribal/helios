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
 └── Render Thread (consumes FramePacket, fully independent)
```

---

## Design Principles

1. **No global state.** No singletons, no static mutable variables. Everything accessed through system parameters (queries, resources, events, commands).
2. **RAII everywhere.** Construct in constructor, destroy in destructor. No `Init()`/`Shutdown()`. Resize = destroy + reconstruct unless performance demands otherwise.
3. **Prefer std library.** Use `std::unique_ptr`, `std::shared_ptr`, `std::vector`, `std::unordered_map`, `std::optional`, `std::variant`, `std::any`, `std::function`, `std::thread`, `std::mutex`, `std::atomic` directly. No custom aliases (`Ref<T>`, `Scope<T>`). Only create custom types when std doesn't cover the need.
4. **Minimize smart pointer usage.** Components: owned by archetypes (plain data in arrays). Resources: owned by World (stored as `std::any`). Assets: owned by AssetServer, referenced by `AssetHandle` (just an ID). RHI objects: RAII value types with move semantics. `std::unique_ptr`: only for polymorphism (backend interfaces). `std::shared_ptr`: rare, only genuine shared ownership.
5. **Components are plain data.** No inheritance, no virtual methods, no smart pointers. Use `AssetHandle` for asset references. Aggregates only (enables automatic reflection via qlibs/reflect).
6. **Systems are free functions (or state class methods).** They declare data access through parameters. The scheduler uses this for automatic parallelism.
7. **Power bottom, convenience top.** Every subsystem exposes low-level primitives AND a convenient high-level API. Both are public. Users can hijack any layer.
8. **Engine knows nothing about the editor.** ImGui, inspector panels, gizmos, undo/redo are all editor-only plugins. The engine is a pure runtime.

---

## 1. ECS Core

### 1.1 World

The `World` is the central data store. It owns all entities, component data, resources, and event channels. There is no global access - systems receive the World's data through typed parameters.

```cpp
class World {
public:
    World() = default;
    ~World() = default;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;
    World(const World&) = delete;            // no copy - one World per App
    World& operator=(const World&) = delete;

    // Entity operations (immediate - use Commands in systems for deferred)
    Entity spawn();
    void despawn(Entity entity);
    bool is_alive(Entity entity) const;

    // Component access (immediate)
    template<typename T> T& get(Entity entity);
    template<typename T> const T& get(Entity entity) const;
    template<typename T> T* try_get(Entity entity);
    template<typename T> bool has(Entity entity) const;
    template<typename T> T& add(Entity entity, T component);
    template<typename T> void remove(Entity entity);

    // Resources
    template<typename T> void insert_resource(T resource);
    template<typename T> T& resource();
    template<typename T> const T& resource() const;
    template<typename T> T* try_resource();
    template<typename T> bool has_resource() const;

    // Events
    template<typename T> void register_event();
    template<typename T> EventWriter<T> event_writer();
    template<typename T> EventReader<T> event_reader();

    // Queries (typically used via system parameters, but available directly)
    template<typename... T> QueryState<T...> query();

    // Direct system execution (for testing)
    template<typename F> void run_system(F&& system);

private:
    ArchetypeStorage m_archetypes;
    ResourceStorage m_resources;    // internally uses std::unordered_map<std::type_index, std::any>
    EventStorage m_events;          // internally uses std::unordered_map<std::type_index, std::any>
    EntityAllocator m_entities;     // generational index allocator
};
```

### 1.2 Entity

Lightweight generational index. 32-bit index + 32-bit generation packed into 64 bits. The generation prevents use-after-free (if entity 5 is despawned and slot 5 is reused, the old Entity{5, gen=1} won't match the new Entity{5, gen=2}).

For serialization and editor references, entities also have an optional UUID mapping maintained by a `UuidMap` resource.

```cpp
struct Entity {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool operator==(const Entity&) const = default;
    explicit operator bool() const { return generation != 0; }

    static constexpr Entity INVALID = { 0, 0 };
};

// Specialization for hashing
template<> struct std::hash<Entity> {
    size_t operator()(Entity e) const noexcept {
        return std::hash<uint64_t>{}(
            (static_cast<uint64_t>(e.generation) << 32) | e.index
        );
    }
};
```

**EntityAllocator** maintains a free list of reusable indices and increments generation on reuse:

```cpp
class EntityAllocator {
public:
    Entity allocate();          // returns next free index with incremented generation
    void deallocate(Entity e);  // pushes index to free list, bumps generation
    bool is_alive(Entity e) const;

private:
    struct Entry { uint32_t generation; bool alive; };
    std::vector<Entry> m_entries;
    std::vector<uint32_t> m_free_list;
};
```

### 1.3 Archetype Storage

Entities with the same component set share an archetype. Each archetype stores components in contiguous `std::vector<std::byte>` arrays (type-erased for storage, typed access via `reinterpret_cast` in query iterators).

```
Archetype { components: [Transform, MeshRenderer] }
  entities:       std::vector<Entity>      = [E1, E2, E3]
  column[0]:      std::vector<std::byte>   = [T1|T2|T3]  (sizeof(Transform) * count)
  column[1]:      std::vector<std::byte>   = [M1|M2|M3]  (sizeof(MeshRenderer) * count)
```

**ArchetypeId:** A sorted set of `std::type_index` values that uniquely identifies an archetype.

```cpp
using ComponentId = std::type_index;
using ArchetypeId = std::vector<ComponentId>; // sorted

struct Archetype {
    ArchetypeId id;
    std::vector<Entity> entities;
    std::vector<std::vector<std::byte>> columns; // one column per component type
    std::unordered_map<ComponentId, size_t> column_index; // type → column position

    size_t count() const { return entities.size(); }

    template<typename T> T* get_column() {
        auto it = column_index.find(typeid(T));
        if (it == column_index.end()) return nullptr;
        return reinterpret_cast<T*>(columns[it->second].data());
    }
};
```

**ArchetypeStorage** is the top-level container:

```cpp
class ArchetypeStorage {
public:
    // Find or create archetype for the given component set
    Archetype& get_or_create(const ArchetypeId& id);

    // Move entity from one archetype to another (add/remove component)
    void move_entity(Entity entity, Archetype& from, Archetype& to);

    // Iterate all archetypes that contain a given set of components
    template<typename... T>
    void for_each_matching(auto&& callback);

    // Entity → archetype location lookup
    struct EntityLocation { Archetype* archetype; size_t row; };
    EntityLocation locate(Entity entity) const;

private:
    std::unordered_map<ArchetypeId, std::unique_ptr<Archetype>> m_archetypes;
    std::unordered_map<uint32_t, EntityLocation> m_entity_map; // entity index → location
};
```

**Archetype operations:**
- **Spawn:** Find archetype matching the component set (or create it). Append entity + component data to the archetype's arrays. O(1) amortized.
- **Despawn:** Swap-remove entity from archetype arrays (swap with last element, pop). O(1).
- **Add component:** Move entity from old archetype to new archetype (old components + new one). Involves one swap-remove from old + one append to new. O(component count).
- **Remove component:** Move entity from old archetype to new archetype (old components - removed one). Same cost.

**Structural changes** (spawn, despawn, add/remove component) are deferred via `Commands` when called from systems, and applied between system executions. This ensures no iterator invalidation during system execution.

### 1.4 Components

Components are plain aggregate structs. No inheritance, no constructors, no virtual methods, no smart pointers. This enables automatic reflection via qlibs/reflect and cache-friendly archetype storage.

```cpp
struct Transform {
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1};

    glm::mat4 to_mat4() const; // utility methods are fine, they're const
};

struct GlobalTransform {
    glm::mat4 matrix{1};       // computed from hierarchy each frame
};

struct MeshRenderer {
    AssetHandle mesh{};
    AssetHandle material{};
};

struct PointLight {
    glm::vec3 color{1};
    float intensity = 1.0f;
    float radius = 10.0f;
};

struct DirectionalLight {
    glm::vec3 color{1};
    float intensity = 1.0f;
    glm::vec3 direction{0, -1, 0};
    bool cast_shadows = true;
};

struct Camera {
    float fov_y = 45.0f;            // degrees
    float near_plane = 0.1f;
    float far_plane = 500.0f;
    ProjectionType projection = ProjectionType::Perspective;
    float ortho_size = 10.0f;
};

struct ActiveCamera {};              // marker component - tag the active camera

struct RigidBody {
    BodyType type = BodyType::Dynamic;
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
    BodyHandle body_handle{};        // opaque handle into physics backend
};

struct BoxCollider {
    glm::vec3 half_extents{0.5f};
    glm::vec3 offset{0};
};

struct SphereCollider {
    float radius = 0.5f;
    glm::vec3 offset{0};
};

struct AudioSource {
    AssetHandle clip{};
    float volume = 1.0f;
    bool loop = false;
    bool spatial = false;            // 3D positional audio
    SoundHandle playing_handle{};    // opaque handle into audio backend
};

struct ScriptInstance {
    uint32_t script_type_id = 0;     // identifies the C# class type
    uint64_t managed_handle = 0;     // handle to managed object
};

struct Parent { Entity entity; };
struct Children { std::vector<Entity> entities; };  // auto-maintained by hierarchy system
struct Tag { std::string name; };
struct Disabled {};                  // marker - excluded from most queries via Without<Disabled>
```

**AssetHandle** is a simple typed ID, not a smart pointer:

```cpp
struct AssetHandle {
    uint64_t id = 0;
    explicit operator bool() const { return id != 0; }
    bool operator==(const AssetHandle&) const = default;
};
```

Assets are owned by `AssetServer` (a World resource). Components hold `AssetHandle` values (plain integers). The AssetServer resolves handles to actual data. No reference counting in components.

### 1.5 Reflection

Primary: **qlibs/reflect** for automatic zero-boilerplate reflection on aggregate components. Fallback: **entt::meta** for complex types that cannot be aggregates.

```cpp
// Works automatically on any aggregate type - no registration
template<typename T>
void serialize_component(const T& component, YamlEmitter& out) {
    reflect::for_each([&](auto I) {
        constexpr auto name = reflect::member_name<I>(T{});
        const auto& value = reflect::get<I>(component);
        out.write(name, value);
    }, component);
}

template<typename T>
T deserialize_component(const YamlNode& node) {
    T component{};
    reflect::for_each([&](auto I) {
        constexpr auto name = reflect::member_name<I>(T{});
        auto& value = reflect::get<I>(component);
        node.read(name, value);
    }, component);
    return component;
}

// Editor inspector generation - also automatic
template<typename T>
void draw_inspector(T& component) {
    reflect::for_each([&](auto I) {
        constexpr auto name = reflect::member_name<I>(T{});
        auto& value = reflect::get<I>(component);
        draw_field(name, value);  // overloaded for float, vec3, string, etc.
    }, component);
}
```

This means: add a field to a component struct, and serialization + editor inspector + scripting bridge automatically pick it up. Zero registration code.

### 1.6 Queries

Queries iterate entities matching a component set. They return typed tuples of references.

```cpp
// Query type - constructed from World, caches matching archetypes
template<typename... Components>
class Query {
public:
    // Iterate all matching entities
    // Each element is a tuple of references to the requested components
    class Iterator;
    Iterator begin();
    Iterator end();

    // Range-based for loop support
    // for (auto [transform, mesh] : query) { ... }

    // Single entity access
    std::tuple<Components&...> get(Entity e);

    // Count
    size_t count() const;

    // Check if empty
    bool is_empty() const;
};
```

**Filters:**
```cpp
// With<T> - entity must have T, but T is not returned
Query<Transform, const MeshRenderer, With<Visible>>

// Without<T> - entity must NOT have T
Query<Transform, Without<Disabled>>

// Optional<T> - T may or may not exist, returned as pointer (nullptr if absent)
Query<Transform, Optional<RigidBody>>
```

**Const correctness:** `const T` in query means read-only access. The scheduler uses this to determine parallelism. `Query<const Transform>` can run parallel with `Query<const Transform>`, but not with `Query<Transform>` (mutable).

**Implementation:** Query iterates matching archetypes. For `Query<Transform, const MeshRenderer>`, it finds all archetypes that contain both Transform and MeshRenderer, then iterates their arrays sequentially. Cache-friendly linear scan within each archetype.

### 1.7 Systems

Systems are free functions. Their parameters declare what data they access. The scheduler inspects parameter types at registration time to build a dependency graph.

```cpp
// Simple system - iterate entities
void move_enemies(Query<Transform, const Velocity> query, Res<Time> time) {
    for (auto [transform, velocity] : query) {
        transform.position += velocity.direction * velocity.speed * time->delta();
    }
}

// System with commands (deferred entity operations)
void spawn_bullets(
    Query<const Transform, const Shooter, With<Firing>> query,
    Commands& cmd,
    Res<Time> time
) {
    for (auto [transform, shooter] : query) {
        cmd.spawn()
            .insert(Transform{ .position = transform.position })
            .insert(Velocity{ .direction = transform.forward(), .speed = 50.0f })
            .insert(Bullet{ .damage = shooter.damage });
    }
}

// System that sends events
void detect_collisions(
    Res<PhysicsWorld> physics,
    EventWriter<CollisionEvent> writer
) {
    for (auto& contact : physics->get_contacts()) {
        writer.send(CollisionEvent{
            .a = contact.entity_a,
            .b = contact.entity_b,
            .point = contact.world_point,
            .normal = contact.normal,
        });
    }
}

// System that receives events
void on_collision(
    EventReader<CollisionEvent> events,
    Query<AudioSource, const Transform> audio_entities,
    ResMut<AudioDevice> audio
) {
    for (auto& event : events) {
        audio->play_at(event.point, "sounds/impact.wav");
    }
}
```

**System parameter types:**

| Parameter | Access | Scheduler effect |
|-----------|--------|-----------------|
| `Query<T, U>` | Read/write T and U | Exclusive access to T, U |
| `Query<const T, const U>` | Read-only T and U | Shared read access |
| `Query<T, const U>` | Write T, read U | Exclusive T, shared U |
| `Res<T>` | Read resource T | Shared read |
| `ResMut<T>` | Write resource T | Exclusive access |
| `Commands&` | Deferred operations | No conflict (applied later) |
| `EventReader<T>` | Read events | Shared read |
| `EventWriter<T>` | Write events | Exclusive write on channel T |

### 1.8 Commands

Deferred operations applied between system runs. Commands buffer structural changes so systems never see partially-modified state.

```cpp
class Commands {
public:
    // Spawn entity with components
    EntityBuilder spawn();

    // Despawn entity (deferred)
    void despawn(Entity entity);

    // Add/remove components (deferred)
    template<typename T> void insert(Entity entity, T component);
    template<typename T> void remove(Entity entity);

    // Insert/modify resources
    template<typename T> void insert_resource(T resource);
};

class EntityBuilder {
public:
    template<typename T> EntityBuilder& insert(T component);
    Entity id() const; // get the (reserved) entity ID before commands are applied
};
```

Commands are collected during system execution and applied in order after the system completes. This means:
- A spawned entity is not visible to the current system, only to subsequent systems.
- A despawned entity remains visible to the current system.
- No iterator invalidation during iteration.

### 1.9 Resources

Resources are typed singletons owned by the World. They replace all current global singletons (`Renderer::Get()`, `Input::s_Instance`, `PhysicsEngine::Get()`, etc.).

Internally stored as `std::unordered_map<std::type_index, std::any>`.

```cpp
// Insertion
world.insert_resource<Time>(Time{});
world.insert_resource<RenderSettings>(RenderSettings{ .exposure = 1.0f });

// Polymorphic resources (backend interfaces)
world.insert_resource<std::unique_ptr<PhysicsWorld>>(
    std::make_unique<JoltPhysicsWorld>(config)
);

// Access in systems
void physics_step(ResMut<std::unique_ptr<PhysicsWorld>> world, Res<Time> time) {
    (*world)->step(time->delta());
}
```

For polymorphic backends (physics, audio), the resource is a `std::unique_ptr<Interface>`. The World owns it. Systems receive `Res<std::unique_ptr<PhysicsWorld>>` or we provide a convenience wrapper:

```cpp
// Convenience: PhysicsRes wraps the unique_ptr access
using PhysicsRes = Res<std::unique_ptr<PhysicsWorld>>;
```

### 1.10 Events

Typed channels with frame-scoped lifetime. Events sent during frame N are readable during frame N, cleared at the start of frame N+1.

```cpp
template<typename T>
class EventWriter {
public:
    void send(T event);
};

template<typename T>
class EventReader {
public:
    // Iterate unread events
    class Iterator;
    Iterator begin() const;
    Iterator end() const;

    // Check if empty
    bool is_empty() const;
};
```

Internally, each event channel is a `std::vector<T>` double-buffered: writers push to the current buffer, readers iterate the previous buffer. Buffer swap happens at frame boundary.

### 1.11 Scheduler

The scheduler owns a `std::vector<SystemDescriptor>` per schedule. Each descriptor stores the system function, its parameter access metadata (which components/resources it reads/writes), and ordering constraints.

```cpp
enum class Schedule {
    Startup,        // runs once at app launch
    PreUpdate,      // input polling, event processing
    Update,         // game logic
    FixedUpdate,    // physics (fixed timestep, ticks N times per frame)
    PostUpdate,     // transform propagation, hierarchy, cleanup
    PreRender,      // render extraction, editor UI
};

struct SystemDescriptor {
    std::function<void(World&)> run;            // type-erased system call
    std::vector<AccessDescriptor> reads;         // component/resource types read
    std::vector<AccessDescriptor> writes;        // component/resource types written
    std::optional<SystemId> after;               // explicit ordering
    std::optional<SystemId> before;              // explicit ordering
};
```

**Parallel execution:** At schedule run time, the scheduler builds a DAG from access metadata:
1. Systems with conflicting access (one writes what another reads/writes) get an edge.
2. Systems with explicit `.after()`/`.before()` get an edge.
3. Independent systems (no conflicting access, no explicit ordering) are dispatched to a `std::thread` pool.

The thread pool is a simple work-stealing pool using `std::thread` + `std::mutex` + `std::condition_variable`. No custom threading primitives.

**FixedUpdate accumulator:**

```cpp
void run_fixed_update(World& world, float frame_delta) {
    auto& accumulator = world.resource<FixedTimeAccumulator>();
    accumulator.remaining += frame_delta;

    while (accumulator.remaining >= accumulator.timestep) {
        run_schedule(world, Schedule::FixedUpdate);
        accumulator.remaining -= accumulator.timestep;
    }

    // Store interpolation alpha for render systems
    accumulator.alpha = accumulator.remaining / accumulator.timestep;
}
```

Physics systems use `Res<FixedTimeAccumulator>` to access the fixed delta. The render extraction system uses `accumulator.alpha` to interpolate between previous and current physics transforms for smooth rendering.

---

## 2. App & Plugin System

### 2.1 App

The `App` owns the `World`, the `Scheduler`, and the plugin registry. It drives the main loop.

```cpp
class App {
public:
    App() = default;

    // Plugin registration
    template<typename P> App& add_plugin(P plugin = {});

    // Convenience methods that delegate to World/Scheduler
    template<typename T> App& insert_resource(T resource);
    template<typename T> App& add_event();
    App& add_system(Schedule schedule, auto&& system);

    // State management
    template<typename S> App& state(auto state_id);

    // Run the main loop (blocks until exit)
    void run();

    // Access (for plugin setup)
    World& world() { return m_world; }

private:
    World m_world;
    Scheduler m_scheduler;
    bool m_running = true;
};
```

**Main loop (`App::run()`):**

```cpp
void App::run() {
    m_scheduler.run(m_world, Schedule::Startup);

    while (m_running) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        m_scheduler.run(m_world, Schedule::PreUpdate);
        m_scheduler.run(m_world, Schedule::Update);
        run_fixed_update(m_world, m_world.resource<Time>().delta());
        m_scheduler.run(m_world, Schedule::PostUpdate);
        m_scheduler.run(m_world, Schedule::PreRender);

        // Submit FramePacket to render thread
        if (auto* rq = m_world.try_resource<RenderQueue>()) {
            m_render_thread.submit(std::move(*rq));
        }

        // Swap event buffers
        m_world.flush_events();

        // Frame timing
        auto frame_end = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(frame_end - frame_start).count();
        m_world.resource<Time>().m_delta = dt;
        m_world.resource<Time>().m_elapsed += dt;
    }
}
```

### 2.2 Plugins

A plugin is any type with a `void build(App& app)` method. Plugins register systems, resources, events, and other plugins.

```cpp
struct WindowPlugin {
    WindowDesc primary_window = { .title = "Helios", .width = 1280, .height = 720 };

    void build(App& app) {
        auto window = std::make_unique<Window>(primary_window);
        app.insert_resource<Windows>(Windows{ std::move(window) });
        app.add_system(Schedule::PreUpdate, poll_window_events);
        app.add_event<WindowResized>();
        app.add_event<WindowClosed>();
    }
};

struct InputPlugin {
    void build(App& app) {
        app.insert_resource<RawInput>(RawInput{});
        app.insert_resource<InputMap>(InputMap{});
        app.add_system(Schedule::PreUpdate, update_raw_input);
        app.add_system(Schedule::PreUpdate, update_action_map.after(update_raw_input));
    }
};
```

Plugins can depend on other plugins:

```cpp
struct ForwardPlusPlugin {
    void build(App& app) {
        app.add_plugin<VulkanRenderPlugin>();  // ensure RHI exists
        app.insert_resource<ForwardPlusConfig>({ ... });
        app.add_system(Schedule::PreRender, build_forward_plus_graph);
    }
};
```

---

## 3. State Management

### 3.1 State Classes

States are RAII classes. Constructor = enter, destructor = exit. Methods = systems for that state.

```cpp
template<typename StateEnum>
class State {
public:
    virtual ~State() = default;

protected:
    // Spawn an entity tracked by this state (auto-despawned when state exits)
    Entity spawn_tracked(World& world);

    // Access to the world for setup
    World& world();
};
```

```cpp
class Playing : public State<GameState> {
public:
    Playing(World& world) {
        // Constructor IS on_enter
        m_player = spawn_tracked(world);
        world.add(m_player, Transform{ .position = {0, 1, 0} });
        world.add(m_player, MeshRenderer{ .mesh = player_mesh });
        world.add(m_player, RigidBody{ .type = BodyType::Dynamic });
    }

    // Destructor IS on_exit
    // Tracked entities are auto-despawned by State<> base class

    void player_movement(Query<Transform, const Player> q, Res<InputMap> input, Res<Time> time) {
        for (auto [transform, player] : q) {
            float x = input->axis("move_x");
            float z = input->axis("move_z");
            transform.position += glm::vec3{x, 0, z} * player.speed * time->delta();
        }
    }

    void check_pause(Res<InputMap> input, ResMut<GameFlow<GameState>> flow) {
        if (input->just_pressed("pause")) {
            flow->push<PauseMenu>();
        }
    }

    static void describe(StateBuilder<Playing>& s) {
        s.opaque();
        s.system(&Playing::player_movement);
        s.system(&Playing::check_pause);
    }

private:
    Entity m_player;
};
```

### 3.2 State Stack

```cpp
template<typename StateEnum>
class GameFlow {
public:
    // Clear stack, push new states
    template<typename... States> void go_to();

    // Push on top (existing states stay)
    template<typename S> void push();

    // Remove top state
    void pop();

    // Replace top state
    template<typename S> void switch_to();

    // Query
    bool is_in(StateEnum state) const;
    StateEnum current() const;
};
```

**Stack modifiers (set in `describe()`):**

| Modifier | Systems below | Entities below | Use case |
|----------|--------------|----------------|----------|
| `opaque()` | stopped | despawned on next transition | Full screen change |
| `transparent()` | keep running | visible | HUD overlay, debug tools |
| `pause_below()` | stopped | visible but frozen | Pause menu |

Combine: `transparent()` + `pause_below()` = you see the game world but it's frozen (pause screen with visible background).

### 3.3 Transitions

```cpp
// In plugin setup:
app.transition(GameState::MainMenu, GameState::Playing)
    .via<LoadingScreen>();  // LoadingScreen is another State class

class LoadingScreen : public State<GameState> {
public:
    LoadingScreen(World& world) {
        auto& assets = world.resource<AssetServer>();
        m_batch = assets.load_batch()
            .add<Mesh>("level_geometry.gltf")
            .add<Texture>("terrain_albedo.png")
            .add<AudioClip>("background_music.ogg")
            .submit();
    }

    void render_progress(ResMut<GameFlow<GameState>> flow) {
        float progress = m_batch.progress();  // 0.0 - 1.0
        // ... draw loading bar ...
        if (m_batch.is_complete()) {
            flow->pop();  // proceeds to next state in transition chain
        }
    }

    static void describe(StateBuilder<LoadingScreen>& s) {
        s.opaque();
        s.system(&LoadingScreen::render_progress);
    }

private:
    LoadBatch m_batch;
};
```

---

## 4. Rendering

### 4.1 RHI (Rendering Hardware Interface)

Thin, Forge-style abstraction. GPU concepts are explicit but unified across backends. Compile-time backend selection (no virtual dispatch overhead for RHI calls).

**Compile-time backend selection:**
```cpp
// In helios-renderer CMakeLists.txt:
// option(HELIOS_BACKEND_VULKAN "Use Vulkan backend" ON)
// Compiles only the selected backend

// In rhi/rhi_types.h:
namespace helios::rhi {
#if defined(HELIOS_BACKEND_VULKAN)
    using Device = vulkan::VulkanDevice;
    using CommandBuffer = vulkan::VulkanCommandBuffer;
    using Texture = vulkan::VulkanTexture;
    using Buffer = vulkan::VulkanBuffer;
    using Pipeline = vulkan::VulkanPipeline;
    using DescriptorSet = vulkan::VulkanDescriptorSet;
    using Swapchain = vulkan::VulkanSwapchain;
#endif
}
```

**Core types are RAII, move-only:**

```cpp
namespace helios::rhi {

class Texture {
public:
    Texture() = default;                          // null/empty state
    Texture(Device& device, const TextureDesc& desc, const void* data = nullptr);
    ~Texture();                                   // frees GPU resource
    Texture(Texture&& other) noexcept;            // move
    Texture& operator=(Texture&& other) noexcept; // move-assign (old resource freed)
    Texture(const Texture&) = delete;             // no copy
    Texture& operator=(const Texture&) = delete;

    uint32_t width() const;
    uint32_t height() const;
    TextureFormat format() const;
    explicit operator bool() const; // check if valid

    // Escape hatch
    template<typename T> T native_handle() const;
    // e.g., texture.native_handle<VkImage>()
};

// Same pattern for Buffer, Pipeline, DescriptorSet, Swapchain, CommandBuffer...

class Device {
public:
    Device(const DeviceDesc& desc);
    ~Device();
    Device(Device&&) noexcept;
    Device(const Device&) = delete;

    Texture create_texture(const TextureDesc& desc, const void* data = nullptr);
    Buffer create_buffer(const BufferDesc& desc, const void* data = nullptr);
    Pipeline create_graphics_pipeline(const GraphicsPipelineDesc& desc);
    Pipeline create_compute_pipeline(const ComputePipelineDesc& desc);
    DescriptorSet create_descriptor_set(const DescriptorSetLayoutDesc& desc);
    CommandBuffer create_command_buffer();
    Swapchain create_swapchain(const SwapchainDesc& desc);

    void submit(const CommandBuffer& cmd, const SubmitInfo& info);
    void wait_idle();

    template<typename T> T* native_handle();
};

} // namespace helios::rhi
```

**Swapchain RAII reconstruct-on-resize:**
```cpp
void handle_resize(ResMut<RenderContext> ctx, EventReader<WindowResized> events) {
    for (auto& e : events) {
        ctx->device.wait_idle();
        // Old swapchain destroyed by move-assignment, new one created
        ctx->swapchain = ctx->device.create_swapchain(SwapchainDesc{
            .width = e.width, .height = e.height,
            .surface = ctx->surface,
            .present_mode = PresentMode::Fifo,
        });
    }
}
```

**Descriptor types and enums** use the same backend-agnostic style as current RHITypes.h but with `enum class` consistently:

```cpp
enum class TextureFormat : uint8_t {
    R8, RG8, RGBA8,
    RG16F, RGBA16F,
    R32F, RG32F, RGB32F, RGBA32F,
    Depth32F, Depth24Stencil8,
};

enum class BufferUsage : uint32_t {
    Vertex   = 1 << 0,
    Index    = 1 << 1,
    Uniform  = 1 << 2,
    Storage  = 1 << 3,
    Transfer = 1 << 4,
};
// operator| and operator& for bitfield

enum class TextureUsage : uint32_t {
    Sampled         = 1 << 0,
    Storage         = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthAttachment = 1 << 3,
    Transfer        = 1 << 4,
};

enum class ShaderStage : uint32_t {
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
    Geometry = 1 << 3,
};

struct TextureDesc {
    uint32_t width = 1;
    uint32_t height = 1;
    TextureFormat format = TextureFormat::RGBA8;
    TextureType type = TextureType::Texture2D;
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    TextureUsage usage = TextureUsage::Sampled;
    SamplerMode sampler = SamplerMode::Repeat;  // or ClampToEdge
    std::string debug_name;
};
```

### 4.2 Render Graph

Frostbite-style frame graph. Rebuilt every frame (cheap - just metadata). Auto-manages barriers, resource lifetimes, and memory aliasing.

**Resource handles** are lightweight indices into the graph's resource table. Not GPU resources - those are allocated during compile phase.

```cpp
struct TextureHandle { uint32_t index; };
struct BufferHandle { uint32_t index; };

class RenderGraphBuilder {
public:
    // Declare reads/writes
    TextureHandle read(TextureHandle input);
    TextureHandle write(TextureHandle output);
    TextureHandle create(const TextureDesc& desc);  // transient resource

    BufferHandle read(BufferHandle input);
    BufferHandle write(BufferHandle output);
    BufferHandle create(const BufferDesc& desc);
};

class RenderContext {
public:
    // Resolve handles to actual GPU resources during execute phase
    rhi::Texture& resolve(TextureHandle handle);
    rhi::Buffer& resolve(BufferHandle handle);

    // GPU command recording
    rhi::CommandBuffer& cmd();
};

class RenderGraph {
public:
    // Import persistent resources
    TextureHandle import_texture(rhi::Texture& external);
    BufferHandle import_buffer(rhi::Buffer& external);

    // Add a render pass
    template<typename Data>
    void add_pass(
        const char* name,
        std::function<void(Data&, RenderGraphBuilder&)> setup,
        std::function<void(const Data&, RenderContext&)> execute
    );

    // Set final output (determines which passes are needed)
    void set_output(TextureHandle final_color);

    // Execute the graph (called by render thread)
    void compile_and_execute(rhi::Device& device);
};
```

**Three-phase execution:**
1. **Setup:** All passes run their setup lambdas, declaring reads/writes/creates through `RenderGraphBuilder`.
2. **Compile:** Graph walks backwards from `set_output()`, marks needed passes, culls unreferenced passes and resources. Computes execution order (topological sort). Inserts barriers between passes based on resource transitions. Identifies memory aliasing opportunities for transient resources.
3. **Execute:** Surviving passes run their execute lambdas with `RenderContext`. Transient resources are allocated just-in-time from a pool. Barriers are inserted automatically.

### 4.3 ForwardPlus Plugin

Default rendering pipeline. Builds the standard render graph from a `FramePacket`.

```cpp
struct ForwardPlusPlugin {
    ForwardPlusConfig config = {
        .hdr_format = TextureFormat::RGBA16F,
        .shadow_resolution = 4096,
        .shadow_cascades = 4,
        .depth_format = TextureFormat::Depth32F,
    };

    void build(App& app) {
        app.add_plugin<VulkanRenderPlugin>();
        app.insert_resource(config);
        app.add_system(Schedule::PreRender, extract_render_data);
        app.add_system(Schedule::PreRender, build_forward_plus_graph.after(extract_render_data));
    }
};

// Render extraction - runs on main thread, builds FramePacket from ECS data
void extract_render_data(
    Query<const Transform, const MeshRenderer, Without<Disabled>> meshes,
    Query<const Transform, const PointLight> point_lights,
    Query<const Transform, const DirectionalLight> dir_lights,
    Query<const Transform, const Camera, With<ActiveCamera>> camera,
    ResMut<FramePacket> packet
) {
    packet->clear();

    for (auto [t, cam] : camera) {
        packet->camera = CameraData{
            .view = glm::inverse(t.to_mat4()),
            .projection = cam.projection_matrix(),
            .position = t.position,
            .near_plane = cam.near_plane,
            .far_plane = cam.far_plane,
        };
    }

    for (auto [t, mr] : meshes) {
        packet->mesh_draws.push_back(MeshDraw{
            .transform = t.to_mat4(),
            .mesh = mr.mesh,
            .material = mr.material,
        });
    }

    for (auto [t, pl] : point_lights) {
        packet->point_lights.push_back(LightData{
            .position = t.position,
            .color = pl.color,
            .intensity = pl.intensity,
            .radius = pl.radius,
        });
    }

    for (auto [t, dl] : dir_lights) {
        packet->dir_lights.push_back(DirLightData{
            .direction = dl.direction,
            .color = dl.color,
            .intensity = dl.intensity,
            .cast_shadows = dl.cast_shadows,
        });
    }
}

// Graph builder - constructs the render graph from the packet
void build_forward_plus_graph(
    Res<FramePacket> packet,
    Res<ForwardPlusConfig> config,
    ResMut<RenderGraph> graph
) {
    auto depth = add_depth_prepass(graph, packet);
    auto shadows = add_shadow_pass(graph, packet, config);
    auto light_cull = add_light_culling_pass(graph, depth, packet);
    auto hdr = add_forward_pass(graph, depth, shadows, light_cull, packet);
    auto skybox = add_skybox_pass(graph, hdr, packet);
    auto ldr = add_tonemap_pass(graph, skybox, config);
    graph->set_output(ldr);
}
```

### 4.4 Render Thread

Separate `std::thread` that consumes `FramePacket`s. Owns the `RenderGraph` executor and GPU submission. Never touches the World.

```cpp
class RenderThread {
public:
    RenderThread(rhi::Device& device, rhi::Swapchain& swapchain);
    ~RenderThread(); // signals shutdown, joins thread

    RenderThread(const RenderThread&) = delete;
    RenderThread& operator=(const RenderThread&) = delete;

    // Called by main thread - swaps packet into render thread's queue
    void submit(FramePacket packet);

private:
    void thread_main();

    rhi::Device& m_device;
    rhi::Swapchain& m_swapchain;

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::optional<FramePacket> m_pending_packet;
    std::atomic<bool> m_running{true};
};
```

**Frame flow:**
1. Main thread runs ECS systems (Update, FixedUpdate, PostUpdate, PreRender).
2. `extract_render_data` system builds `FramePacket` from ECS queries.
3. `build_forward_plus_graph` builds the graph specification.
4. Main thread calls `render_thread.submit(packet)` - moves packet to render thread.
5. Main thread continues to frame N+1 immediately.
6. Render thread wakes up, acquires swapchain image, compiles graph, executes passes, presents.
7. Render thread can lag 1-2 frames behind main thread.

### 4.5 Shader System

Cross-backend shader sources compiled at build time via CMake custom commands.

```cmake
# In helios-renderer/CMakeLists.txt:
compile_shaders(
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders
    BACKEND vulkan   # compiles GLSL/HLSL → SPIR-V
)
```

**Hot reload in development:** A file watcher system detects shader source changes, triggers recompile via `glslc`, and the render thread recreates affected pipelines. This is an editor-only feature (development builds).

### 4.6 IBL Pipeline

The IBL generation (equirect-to-cube, irradiance convolution, prefilter, BRDF LUT) is implemented as compute dispatches on the render thread. When a skybox texture is loaded, an `IBLGenerationRequest` event is sent. The render thread picks it up and generates the IBL textures.

---

## 5. Physics

### 5.1 Physics Interface

Backend-abstracted. The engine defines the interface. Jolt implements it behind `std::unique_ptr<PhysicsWorld>`.

```cpp
namespace helios::physics {

using BodyHandle = uint64_t;

struct BodyDesc {
    BodyType type = BodyType::Static;
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
    ColliderShape shape;     // variant: Box, Sphere, Capsule, Mesh
};

struct ContactEvent {
    Entity entity_a;
    Entity entity_b;
    glm::vec3 world_point;
    glm::vec3 normal;
    float impulse;
};

class PhysicsWorld {
public:
    virtual ~PhysicsWorld() = default;

    virtual BodyHandle create_body(const BodyDesc& desc) = 0;
    virtual void destroy_body(BodyHandle handle) = 0;
    virtual void set_transform(BodyHandle handle, const glm::vec3& pos, const glm::quat& rot) = 0;
    virtual glm::vec3 get_position(BodyHandle handle) const = 0;
    virtual glm::quat get_rotation(BodyHandle handle) const = 0;
    virtual void set_velocity(BodyHandle handle, const glm::vec3& linear) = 0;
    virtual void apply_force(BodyHandle handle, const glm::vec3& force) = 0;
    virtual void apply_impulse(BodyHandle handle, const glm::vec3& impulse) = 0;

    virtual void step(float dt) = 0;
    virtual std::vector<ContactEvent> drain_contacts() = 0;

    // Raycasting
    struct RayHit { Entity entity; glm::vec3 point; glm::vec3 normal; float distance; };
    virtual std::optional<RayHit> raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist) const = 0;
    virtual std::vector<RayHit> raycast_all(const glm::vec3& origin, const glm::vec3& dir, float max_dist) const = 0;
};

} // namespace helios::physics
```

### 5.2 Physics Systems

```cpp
// FixedUpdate: step simulation
void physics_step(
    ResMut<std::unique_ptr<PhysicsWorld>> world,
    Res<PhysicsConfig> config,
    EventWriter<CollisionEvent> collisions
) {
    world->get()->step(config->fixed_timestep);
    for (auto& contact : world->get()->drain_contacts()) {
        collisions.send(CollisionEvent{
            .a = contact.entity_a,
            .b = contact.entity_b,
            .point = contact.world_point,
            .normal = contact.normal,
        });
    }
}

// PostUpdate: sync transforms back from physics
void sync_physics_transforms(
    Query<Transform, const RigidBody> bodies,
    Res<std::unique_ptr<PhysicsWorld>> world
) {
    for (auto [transform, rb] : bodies) {
        transform.position = world->get()->get_position(rb.body_handle);
        transform.rotation = world->get()->get_rotation(rb.body_handle);
    }
}

// PostUpdate: push ECS transforms to physics (for kinematic bodies)
void push_kinematic_transforms(
    Query<const Transform, const RigidBody> bodies,
    ResMut<std::unique_ptr<PhysicsWorld>> world
) {
    for (auto [transform, rb] : bodies) {
        if (rb.type == BodyType::Kinematic) {
            world->get()->set_transform(rb.body_handle, transform.position, transform.rotation);
        }
    }
}
```

---

## 6. Audio

### 6.1 Audio Interface

```cpp
namespace helios::audio {

using SoundHandle = uint64_t;

struct PlayParams {
    float volume = 1.0f;
    bool loop = false;
    float pitch = 1.0f;
};

class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    virtual SoundHandle play(const void* pcm_data, size_t size, const PlayParams& params = {}) = 0;
    virtual SoundHandle play_at(const void* pcm_data, size_t size, const glm::vec3& pos, const PlayParams& params = {}) = 0;
    virtual void stop(SoundHandle handle) = 0;
    virtual void pause(SoundHandle handle) = 0;
    virtual void resume(SoundHandle handle) = 0;
    virtual bool is_playing(SoundHandle handle) const = 0;
    virtual void set_volume(SoundHandle handle, float volume) = 0;
    virtual void set_position(SoundHandle handle, const glm::vec3& pos) = 0;
    virtual void set_listener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up) = 0;
    virtual void update() = 0;  // per-frame update (e.g., stream buffers)
};

} // namespace helios::audio
```

### 6.2 Audio Systems

```cpp
void update_audio_listener(
    Query<const Transform, With<ActiveCamera>> camera,
    ResMut<std::unique_ptr<AudioDevice>> audio
) {
    for (auto [transform] : camera) {
        auto forward = transform.rotation * glm::vec3{0, 0, -1};
        auto up = transform.rotation * glm::vec3{0, 1, 0};
        audio->get()->set_listener(transform.position, forward, up);
    }
}

void update_spatial_sources(
    Query<const Transform, AudioSource> sources,
    ResMut<std::unique_ptr<AudioDevice>> audio
) {
    for (auto [transform, source] : sources) {
        if (source.spatial && source.playing_handle) {
            audio->get()->set_position(source.playing_handle, transform.position);
        }
    }
}
```

---

## 7. Scripting

### 7.1 Script Execution Model

C# scripts are per-entity instances. The engine groups entities by script type and batches execution.

```csharp
// C# side
[System(Schedule.Update)]
public class EnemyAI : Script
{
    // Serialized per-instance fields
    public float Speed = 5.0f;
    public float DetectRange = 15.0f;

    public override void OnUpdate(float delta)
    {
        var transform = Entity.Get<Transform>();
        var player = World.QuerySingle<Transform, With<Player>>();

        if (player != null)
        {
            float dist = Vector3.Distance(transform.Position, player.Position);
            if (dist < DetectRange)
            {
                var dir = Vector3.Normalize(player.Position - transform.Position);
                transform.Position += dir * Speed * delta;
            }
        }
    }
}
```

**C++ side execution flow:**
1. `ScriptExecutionSystem` runs in `Schedule::Update`.
2. Groups all entities by their `ScriptInstance.script_type_id`.
3. For each group, calls across the C++/C# bridge: `bridge.invoke_update(script_type_id, entity_handles[], delta)`.
4. The C# runtime iterates the handles, creates `Script` wrappers, calls `OnUpdate()`.
5. Different script types are independent batches. The scheduler can run non-conflicting batches in parallel.

### 7.2 Hot Reload

Same mechanism as current: collectible `AssemblyLoadContext`, file watcher, serialize state → unload → reload → deserialize state. Integrated as a system:

```cpp
void check_script_reload(
    ResMut<ScriptRuntime> runtime,
    EventReader<FileChanged> file_events
) {
    for (auto& e : file_events) {
        if (e.path.extension() == ".cs" || e.path.extension() == ".dll") {
            runtime->request_reload();
        }
    }
}
```

---

## 8. Asset System

### 8.1 AssetServer Resource

Owned by the World. Manages async loading, caching, and handle resolution.

```cpp
class AssetServer {
public:
    AssetServer(const std::filesystem::path& asset_root);
    ~AssetServer() = default;

    // Async load (returns handle immediately)
    template<typename T>
    AssetHandle load(const std::string& path);

    // Sync load (blocks)
    template<typename T>
    AssetHandle load_sync(const std::string& path);

    // Batch loading with progress tracking
    LoadBatchBuilder load_batch();

    // Resolve handle to loaded asset (returns nullptr if not loaded yet)
    template<typename T>
    const T* get(AssetHandle handle) const;

    // Check status
    AssetStatus status(AssetHandle handle) const; // Loading, Loaded, Failed
    bool is_loaded(AssetHandle handle) const;

    // Hot reload support
    void watch_for_changes(bool enable);

private:
    struct AssetEntry {
        std::any data;                          // the actual asset
        AssetStatus status;
        std::filesystem::path path;
    };

    std::unordered_map<uint64_t, AssetEntry> m_assets;
    std::mutex m_mutex;                          // protects m_assets
    std::vector<std::jthread> m_loader_threads;  // background loading
    std::filesystem::path m_root;
};
```

### 8.2 LoadBatch

```cpp
class LoadBatchBuilder {
public:
    template<typename T> LoadBatchBuilder& add(const std::string& path);
    LoadBatch submit();
};

class LoadBatch {
public:
    float progress() const;      // 0.0 to 1.0
    int total() const;
    int remaining() const;
    bool is_complete() const;
    std::vector<std::string> failed() const;  // paths that failed to load
};
```

### 8.3 Serialization

**YAML (development):**
- Auto-generated from reflection (qlibs/reflect iterates fields, writes name:value pairs).
- Human-readable, git-diffable.
- Used by editor for scene files, project settings, asset metadata.

**Binary (runtime):**
- Auto-generated from reflection (qlibs/reflect iterates fields, writes raw bytes + size headers).
- Fast load, compact.
- Editor exports YAML to binary on build.

Scene file format:
```yaml
scene:
  name: "Main Level"
  entities:
    - id: "a1b2c3d4"
      tag: "Player"
      components:
        Transform:
          position: [0, 1, 0]
          rotation: [1, 0, 0, 0]
          scale: [1, 1, 1]
        MeshRenderer:
          mesh: "meshes/player.gltf"
          material: "materials/player.mat"
        RigidBody:
          type: Dynamic
          mass: 70.0
```

---

## 9. Input System

### 9.1 Raw Input (Power Layer)

Direct keyboard/mouse/gamepad state. Updated by `WindowPlugin` each frame.

```cpp
struct RawInput {
    // Keyboard
    bool key_pressed(KeyCode key) const;
    bool key_just_pressed(KeyCode key) const;
    bool key_just_released(KeyCode key) const;

    // Mouse
    glm::vec2 mouse_position() const;
    glm::vec2 mouse_delta() const;
    float scroll_delta() const;
    bool mouse_button_pressed(MouseButton btn) const;
    bool mouse_button_just_pressed(MouseButton btn) const;

    // Gamepad (future)
    float gamepad_axis(int pad, GamepadAxis axis) const;
    bool gamepad_button(int pad, GamepadButton btn) const;

    // Internal state (updated by input system)
    std::array<bool, 512> m_keys_current{};
    std::array<bool, 512> m_keys_previous{};
    glm::vec2 m_mouse_pos{};
    glm::vec2 m_mouse_pos_prev{};
    float m_scroll{};
    std::array<bool, 8> m_mouse_buttons_current{};
    std::array<bool, 8> m_mouse_buttons_previous{};
};
```

### 9.2 Action Mapping (Convenience Layer)

```cpp
class InputMap {
public:
    // Define actions (can bind multiple inputs to one action)
    InputMap& action(const std::string& name, KeyCode key);
    InputMap& action(const std::string& name, MouseButton btn);
    InputMap& action(const std::string& name, GamepadButton btn);

    // Define axes (positive/negative keys, or analog stick)
    InputMap& axis(const std::string& name, KeyCode positive, KeyCode negative);
    InputMap& axis(const std::string& name, GamepadAxis stick);

    // Query (used in systems)
    bool pressed(const std::string& action) const;
    bool just_pressed(const std::string& action) const;
    bool just_released(const std::string& action) const;
    float axis_value(const std::string& axis_name) const;  // -1.0 to 1.0

private:
    struct ActionBinding {
        std::vector<KeyCode> keys;
        std::vector<MouseButton> mouse_buttons;
        std::vector<GamepadButton> gamepad_buttons;
    };
    struct AxisBinding {
        KeyCode positive = KeyCode::Unknown;
        KeyCode negative = KeyCode::Unknown;
        GamepadAxis gamepad_axis = GamepadAxis::None;
    };

    std::unordered_map<std::string, ActionBinding> m_actions;
    std::unordered_map<std::string, AxisBinding> m_axes;
    const RawInput* m_raw = nullptr;  // set by input update system
};
```

---

## 10. Window System

Multi-window support via GLFW.

```cpp
class Window {
public:
    Window(const WindowDesc& desc);
    ~Window();                        // glfwDestroyWindow
    Window(Window&& other) noexcept;
    Window(const Window&) = delete;

    uint32_t width() const;
    uint32_t height() const;
    bool should_close() const;
    void poll_events();
    void* native_handle() const;      // GLFWwindow*

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;     // pimpl to hide GLFW
};

class Windows {
public:
    WindowId create(const WindowDesc& desc);
    void destroy(WindowId id);
    Window& get(WindowId id);
    Window& primary();
    void poll_all();

    // Iteration
    auto begin() { return m_windows.begin(); }
    auto end() { return m_windows.end(); }

private:
    std::unordered_map<WindowId, Window> m_windows;
    WindowId m_primary;
    WindowId m_next_id = 1;
};
```

---

## 11. Engine/Editor Separation

### 11.1 Engine

CMake library targets. Zero ImGui, zero editor knowledge.

**Public API surface:**
- `helios-core`: World, App, Plugin, Entity, Component, System, Query, Resource, Event, Commands, Scheduler, Time, Input, Window, AssetServer, Logging
- `helios-renderer`: RHI types, Device, Texture, Buffer, Pipeline, RenderGraph, ForwardPlusPlugin
- `helios-physics`: PhysicsWorld interface, BodyHandle, CollisionEvent
- `helios-audio`: AudioDevice interface, SoundHandle
- `helios-script`: ScriptRuntime interface, ScriptInstance component

### 11.2 Editor

Separate executable. Links all engine libraries + ImGui + ImGuizmo.

```cpp
// editor/main.cpp
int main() {
    helios::App app;

    // Engine plugins
    app.add_plugin<helios::WindowPlugin>({ .title = "Helios Editor", .width = 1920, .height = 1080 });
    app.add_plugin<helios::InputPlugin>();
    app.add_plugin<helios::ForwardPlusPlugin>();
    app.add_plugin<helios::PhysicsPlugin<helios::JoltBackend>>();
    app.add_plugin<helios::AudioPlugin<helios::SoLoudBackend>>();
    app.add_plugin<helios::ScriptingPlugin>();

    // Editor-only plugins
    app.add_plugin<editor::EditorPlugin>();

    app.run();
}
```

```cpp
struct EditorPlugin {
    void build(App& app) {
        app.add_plugin<ImGuiRenderPlugin>();  // ImGui backend (uses RHI escape hatch)
        app.insert_resource<EditorState>(EditorState{});
        app.insert_resource<EditorCommands>(EditorCommands{});

        app.add_system(Schedule::PreRender, scene_hierarchy_panel);
        app.add_system(Schedule::PreRender, inspector_panel);
        app.add_system(Schedule::PreRender, content_browser_panel);
        app.add_system(Schedule::PreRender, viewport_panel);
        app.add_system(Schedule::PreRender, gizmo_system);
        app.add_system(Schedule::PreRender, debug_visualization);
    }
};
```

### 11.3 Editor Undo/Redo

`EditorCommands` wraps `Commands` with diff tracking.

```cpp
class EditorCommands {
public:
    // Modify a component (records old value for undo)
    template<typename T>
    void set(Entity entity, World& world, const T& new_value) {
        auto old_value = world.get<T>(entity);
        m_undo_stack.push(UndoEntry{
            .entity = entity,
            .apply = [=](World& w) { w.get<T>(entity) = new_value; },
            .revert = [=](World& w) { w.get<T>(entity) = old_value; },
        });
        world.get<T>(entity) = new_value;
    }

    void undo(World& world);
    void redo(World& world);
    bool can_undo() const;
    bool can_redo() const;

private:
    struct UndoEntry {
        Entity entity;
        std::function<void(World&)> apply;
        std::function<void(World&)> revert;
    };
    std::vector<UndoEntry> m_undo_stack;
    size_t m_cursor = 0;
};
```

### 11.4 ImGui Integration

ImGui lives entirely in the editor. It uses the RHI escape hatch for backend initialization:

```cpp
struct ImGuiRenderPlugin {
    void build(App& app) {
        app.add_system(Schedule::Startup, init_imgui);
        app.add_system(Schedule::PreRender, begin_imgui_frame);
        // ImGui draw data is submitted to render thread as part of FramePacket
    }
};

void init_imgui(Res<rhi::Device> device, Res<Windows> windows) {
    ImGui::CreateContext();

    // Escape hatch: access native handles for ImGui backend init
    auto* vk_device = device->native_handle<VkDevice>();
    auto* glfw_window = static_cast<GLFWwindow*>(windows->primary().native_handle());
    ImGui_ImplGlfw_InitForVulkan(glfw_window, true);
    // ... ImGui Vulkan backend init using native handles ...
}
```

---

## 12. CMake Module Structure

```
helios/
  CMakeLists.txt                    (root: project options, find_package, add_subdirectory)

  helios-core/
    CMakeLists.txt                  → libhelios-core.a
    src/
      ecs/
        world.h / world.cpp
        archetype.h / archetype.cpp
        entity.h
        query.h
        commands.h / commands.cpp
        scheduler.h / scheduler.cpp
        resource_storage.h
        event_storage.h
      app/
        app.h / app.cpp
        plugin.h
        state.h / state.cpp
        game_flow.h / game_flow.cpp
      core/
        logging.h / logging.cpp       (spdlog wrapper, RAII)
        timer.h
        uuid.h / uuid.cpp             (thread-safe RNG via thread_local)
        profiler.h                     (Tracy macros)
      math/
        math.h                         (glm utilities, BoundingBox, Ray)
      input/
        raw_input.h
        input_map.h / input_map.cpp
        key_codes.h
      window/
        window.h / window.cpp          (GLFW pimpl)
        windows.h / windows.cpp
      assets/
        asset_server.h / asset_server.cpp
        asset_handle.h
        load_batch.h / load_batch.cpp
        importers/
          texture_importer.h / .cpp
          mesh_importer.h / .cpp
          audio_importer.h / .cpp
      serialization/
        yaml_serializer.h / .cpp
        binary_serializer.h / .cpp
        reflect_helpers.h              (serialize/deserialize via qlibs/reflect)

  helios-renderer/
    CMakeLists.txt                  → libhelios-renderer.a
    src/
      rhi/
        rhi_types.h                    (enums, descriptors - backend-agnostic)
        rhi.h                          (type aliases: Device, Texture, etc.)
      vulkan/
        vulkan_device.h / .cpp
        vulkan_texture.h / .cpp
        vulkan_buffer.h / .cpp
        vulkan_pipeline.h / .cpp
        vulkan_command_buffer.h / .cpp
        vulkan_swapchain.h / .cpp
        vulkan_descriptor.h / .cpp
        vulkan_context.h / .cpp        (instance, validation)
        vulkan_utils.h                 (format conversion helpers)
      graph/
        render_graph.h / render_graph.cpp
        render_graph_builder.h
        render_context.h
        resource_pool.h / .cpp         (transient resource allocation)
      forward_plus/
        forward_plus_plugin.h / .cpp
        depth_prepass.h / .cpp
        shadow_pass.h / .cpp
        light_culling.h / .cpp
        forward_pass.h / .cpp
        skybox_pass.h / .cpp
        tonemap_pass.h / .cpp
        ibl_generation.h / .cpp
      render_thread.h / render_thread.cpp
      frame_packet.h
      pipeline_cache.h / .cpp
      default_textures.h / .cpp

  helios-physics/
    CMakeLists.txt                  → libhelios-physics.a
    src/
      interface/
        physics_world.h
        body_types.h
        contact_event.h
        physics_plugin.h
      jolt/
        jolt_physics_world.h / .cpp
        jolt_utils.h

  helios-audio/
    CMakeLists.txt                  → libhelios-audio.a
    src/
      interface/
        audio_device.h
        audio_types.h
        audio_plugin.h
      soloud/
        soloud_device.h / .cpp

  helios-script/
    CMakeLists.txt                  → libhelios-script.a
    src/
      interface/
        script_runtime.h
        script_component.h
        scripting_plugin.h
      coreclr/
        coreclr_runtime.h / .cpp
        hostfxr_bridge.h / .cpp
        script_glue.h / .cpp

  helios-editor/
    CMakeLists.txt                  → helios-editor (executable)
    src/
      main.cpp
      editor_plugin.h / .cpp
      editor_state.h
      editor_commands.h / .cpp       (undo/redo)
      imgui/
        imgui_render_plugin.h / .cpp  (ImGui init via RHI escape hatch)
      panels/
        scene_hierarchy.h / .cpp
        inspector.h / .cpp
        content_browser.h / .cpp
        viewport.h / .cpp
        project_settings.h / .cpp
        scene_settings.h / .cpp
      gizmo/
        gizmo_system.h / .cpp        (ImGuizmo integration)

  vendor/                            (third-party dependencies)
    glfw/
    imgui/
    jolt/
    soloud/
    glm/
    yaml-cpp/
    tracy/
    vma/
    vk-bootstrap/
    stb/
    assimp/
    qlibs-reflect/
    entt-meta/                       (just the meta module, not full entt)

  scripts/
    compile_shaders.cmake
    generate_reflection.cmake         (future: if we add codegen)

  shaders/                           (cross-backend shader sources)
    forward.vert / forward.frag
    depth_prepass.vert / depth_prepass.frag
    shadow.vert / shadow.frag
    light_culling.comp
    tonemap.comp
    skybox.vert / skybox.frag
    equirect_to_cube.comp
    irradiance_convolution.comp
    prefilter_envmap.comp
    brdf_lut.comp
```

**Dependency graph:**
```
helios-core         depends on: glm, yaml-cpp, glfw, spdlog, tracy, qlibs-reflect
helios-renderer     depends on: helios-core, vulkan, vma, vk-bootstrap
helios-physics      depends on: helios-core, jolt
helios-audio        depends on: helios-core, soloud
helios-script       depends on: helios-core, hostfxr/.NET
helios-editor       depends on: all above + imgui + imgui_impl_vulkan + imguizmo + nfd
```

---

## 13. Testing

### 13.1 Headless Mode

```cpp
struct HeadlessPlugin {
    void build(App& app) {
        // No window, no GPU. Inserts stub resources.
        app.insert_resource<Windows>(Windows{});  // empty, no real window
        // RenderThread not started - FramePackets are dropped
    }
};

// Test:
App app;
app.add_plugin<HeadlessPlugin>();
app.add_plugin<PhysicsPlugin<JoltBackend>>();
// Physics works, rendering skipped
```

### 13.2 ECS Unit Testing

```cpp
// Direct World manipulation for testing
TEST(ECS, MoveSystem) {
    World world;
    world.insert_resource<Time>(Time{ .m_delta = 1.0f / 60.0f });

    auto e = world.spawn();
    world.add(e, Transform{ .position = {0, 0, 0} });
    world.add(e, Velocity{ .direction = {1, 0, 0}, .speed = 60.0f });

    world.run_system(move_entities);

    auto& t = world.get<Transform>(e);
    EXPECT_NEAR(t.position.x, 1.0f, 0.001f);  // 60 * (1/60) = 1.0
}
```

### 13.3 Mock Backends

```cpp
class MockPhysicsWorld : public PhysicsWorld {
    // Implement interface with in-memory state for testing
    // No Jolt dependency
};

app.add_plugin<PhysicsPlugin<MockPhysicsBackend>>();
```

---

## 14. Migration Path (from current codebase)

This is a big-bang rewrite. The new codebase starts from scratch with the new directory structure and CMake build. The old premake + source structure is preserved in git history.

**What carries over (adapted, not copy-pasted):**
- Shader GLSL source (adapted for new pipeline)
- Vulkan backend implementation (refactored into new RHI)
- Jolt physics integration (refactored behind PhysicsWorld interface)
- SoLoud audio integration (refactored behind AudioDevice interface)
- CoreCLR/HostFXR bridge (refactored behind ScriptRuntime interface)
- Asset importers (assimp mesh loader, stbi texture loader)
- Editor panel logic (refactored into ECS systems)
- ImGuizmo integration (moved to editor)

**What is rewritten from scratch:**
- ECS (World, Archetype, Query, Scheduler, Commands)
- App + Plugin system
- State management (GameFlow, State stack)
- Render graph
- RHI layer (compile-time backend selection, RAII types)
- Render thread
- Input system (raw + action mapping)
- Window system (multi-window)
- Asset system (async, batch, progress)
- Serialization (reflection-based)
- CMake build system

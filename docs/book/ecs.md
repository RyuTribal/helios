# ECS Deep Dive

Helios is built on a high-performance, **Archetype-based Entity Component System (ECS)**. This architecture ensures optimal cache locality, efficient memory usage, and automatic parallelization of game logic.

---

## 1. Archetype-based Storage

Unlike "sparse set" or "array-of-structs" ECS implementations, Helios uses **Archetypes**.

### What is an Archetype?
An Archetype is a unique collection of component types. Every entity that possesses the *exact same set* of components belongs to the same Archetype. For example:
- Entity A has `{Transform, MeshRenderer}` -> Archetype 1
- Entity B has `{Transform, MeshRenderer}` -> Archetype 1
- Entity C has `{Transform, MeshRenderer, PointLight}` -> Archetype 2

### Memory Layout
Each Archetype stores its components in **contiguous columns**. If an archetype has 1,000 entities with `Transform` and `Velocity`, it maintains two tightly packed arrays of 1,000 elements each.
- **Benefits:**
    - **Cache Efficiency:** Iterating over a component is a simple linear scan through memory.
    - **No Pointer Chasing:** Systems access data directly from packed arrays.
    - **Fast Batching:** The renderer and physics engine can process entire columns in bulk.

### Structural Changes
When you `add` or `remove` a component, the entity is moved from its current archetype to a different one. This is a "structural change." While moving an entity has a small cost, the resulting contiguous storage makes every subsequent iteration much faster.

---

## 2. Entities and Components

### Entities
An `Entity` is a lightweight, 64-bit identifier. It contains a 32-bit index and a 32-bit generation to safely handle reuse after an entity is despawned.

### Components
Components in Helios are plain-old-data (POD) or aggregate structs. They should represent "data" rather than "behavior."

```cpp
struct Velocity {
    vec3 linear = {0, 0, 0};
    float speed = 1.0f;
};

struct Health {
    float current = 100.0f;
    float max = 100.0f;
};
```

**Requirements:**
- Must be move-constructible and destructible.
- Must be aggregates (no virtual methods).
- RAII types (like `std::vector` or `Handle<T>`) are allowed and their destructors will run when the entity is despawned.

### Spawning Entities
The `World` provides several ways to create entities:

```cpp
// 1. Spawn empty
Entity e = world.spawn();

// 2. Spawn with components (most common)
Entity player = world.spawn(
    Tag{.name = "Player"},
    Transform{.position = {0, 5, 0}},
    Velocity{.speed = 10.0f},
    Health{100, 100}
);
```

### Hierarchy
Helios supports a built-in Parent/Child hierarchy.

```cpp
// Spawn with a builder lambda
Entity parent = world.spawn_with_children(
    [](World& w, Entity p) {
        w.spawn_child(p, Tag{"Left Wing"}, Transform{});
        w.spawn_child(p, Tag{"Right Wing"}, Transform{});
    },
    Tag{"Airplane"}, Transform{}
);

// Manually spawn a child
Entity child = world.spawn_child(parent, Tag{"Propeller"});
```
*Note: Parenting adds a `Parent` component to the child and a `Children` component to the parent.*

### Mutating Components
```cpp
// Add a component (moves entity to a new archetype)
world.add(entity, PointLight{.color = {1, 0, 0}});

// Remove a component
world.remove<Velocity>(entity);

// Access (asserts existence)
auto& t = world.get<Transform>(entity);

// Safe access (returns nullptr if missing)
if (auto* hp = world.try_get<Health>(entity)) {
    hp->current -= 10;
}
```

---

## 3. Systems and Parameters

Systems are the logic of your game. They are usually free functions or lambdas.

```cpp
void movement_system(Query<Transform, const Velocity> query, Res<Time> time) {
    for (auto [transform, velocity] : query) {
        transform.position += velocity.linear * velocity.speed * time->delta();
    }
}
```

### System Parameters
Helios automatically injects data into systems based on their parameter types:

| Parameter | Access | Description |
|---|---|---|
| `Query<Ts...>` | Mixed | Iterates over entities matching constraints. |
| `Res<T>` | Read | Immutable access to a global resource. |
| `ResMut<T>` | Write | Mutable access to a global resource. |
| `Commands` | Write | Defer structural changes (spawn, despawn, add/remove). |
| `EventReader<T>` | Read | Read events sent in previous stages/frames. |
| `EventWriter<T>` | Write | Send events to be read by other systems. |
| `World&` | Exclusive | Full, direct access. Forces serial execution of the system. |

### Commands (Deferred Changes)
You **cannot** structurally modify the world (spawn/add/remove) while iterating over a `Query` because it would invalidate the archetype storage. Instead, use `Commands`:

```cpp
void spawner_system(Commands cmds, Query<Entity, const Transform> q) {
    for (auto [entity, transform] : q.with_entity()) {
        if (should_split(transform)) {
            cmds.spawn(Transform{transform.position}); // Queued for later
            cmds.despawn(entity);                      // Queued for later
        }
    }
}
```

---

## 4. Advanced Queries

Queries allow you to precisely filter which entities you want to process.

### Query Filters
- `T`: Fetch mutable reference.
- `const T`: Fetch immutable reference.
- `With<T>`: Require `T`, but don't fetch it (filter only).
- `Without<T>`: Exclude entities that have `T`.
- `Optional<T>`: Fetch `T*` (will be `nullptr` if the entity lacks `T`).
- `Changed<T>`: Only yield entities where `T` was modified since this system last ran.

```cpp
// Entities with Transform AND MeshRenderer, but WITHOUT Disabled
using MyQuery = Query<Transform, With<MeshRenderer>, Without<Disabled>>;
```

### Iteration Styles
```cpp
// Standard (components only)
for (auto [transform, velocity] : query) { ... }

// With Entity ID
for (auto [entity, transform] : query.with_entity()) { ... }
```

### Change Detection
Helios tracks "ticks" for every component mutation. `Changed<T>` is extremely powerful for optimization (e.g., only updating physics bodies when their transform moves).

```cpp
void sync_physics(Query<const Transform, Changed<Transform>, ResMut<PhysicsWorld>> query) {
    for (auto [transform] : query) {
        // Only runs for entities whose Transform actually changed!
    }
}
```

---

## 5. Scheduling and Parallelism

### Registering Systems
Systems are added to the `App` and assigned to a `Schedule`.

```cpp
app.add_system(Schedule::Update, movement_system, "movement");
```

### Ordering
By default, the scheduler runs systems in parallel. You can enforce order using `.before()` and `.after()`:

```cpp
app.add_system(Schedule::Update, input_system, "input");
app.add_system(Schedule::Update, move_system, "move").after("input");
app.add_system(Schedule::Update, post_move, "post").after("move");
```

### Parallel Execution
Helios analyzes the parameters of every system to build a **Dependency Graph (DAG)**.
- If System A writes to `Transform` (`Query<Transform>`) and System B reads `Transform` (`Query<const Transform>`), the scheduler ensures System A finishes before B starts (or vice versa, depending on registration).
- If two systems have no data conflicts (e.g., one works on `Audio` and another on `Physics`), they will **automatically run in parallel** on different CPU cores.

This "Parallel by Default" approach allows Helios to scale across modern multi-core processors without the developer manually managing threads or mutexes.

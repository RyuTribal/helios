# ECS

Helios uses an archetype-based Entity Component System. Entities are
lightweight IDs, components are plain data structs, and systems are free
functions whose parameters are automatically injected.

## Entities

An `Entity` is a 64-bit identity -- 32-bit index + 32-bit generation counter.
The generation prevents ABA problems after despawn/realloc.

```cpp
struct Entity {
    uint32_t index = 0;
    uint32_t generation = 0;
};
```

Entities have no behavior and hold no data. Components are stored in
archetype tables, not on the entity.

## Components

A component is any type that satisfies:

```cpp
template <typename T>
concept Component = std::is_move_constructible_v<T>
                 && std::is_destructible_v<T>
                 && !std::is_polymorphic_v<T>;
```

Plain aggregates, `std::string`, `std::vector`, and RAII types like
`Handle<T>` all qualify. Virtual classes do not.

### Built-in components

| Component | Fields |
|---|---|
| `Transform` | `position` (vec3), `rotation` (quat), `scale` (vec3) |
| `GlobalTransform` | `matrix` (mat4) -- computed by hierarchy propagation |
| `MeshRenderer` | `mesh` (Handle), `material` (Handle), `flags`, `sort_order` |
| `Camera` | `projection`, `fov_degrees`, `near_plane`, `far_plane`, `order`, `viewport_*` |
| `ActiveCamera` | Tag -- marks the primary camera |
| `PointLight` | `color`, `intensity`, `radius` |
| `DirectionalLight` | `color`, `intensity` |
| `RigidBody` | `body_type`, `mass`, `friction`, `restitution`, `flags` |
| `BoxCollider` | `half_extents`, `offset`, `flags` |
| `SphereCollider` | `radius`, `offset`, `flags` |
| `AudioSource` | `clip`, `volume`, `pitch`, `flags` |
| `Parent` | `entity` -- the parent Entity |
| `Children` | `entities` -- vector of child Entities |
| `Tag` | `name` (string) |
| `SceneRoot` | `scene_name`, `scene_path` |
| `Disabled` | Tag -- excluded from most queries |
| `EditorOnly` | Tag -- skipped by serializer, physics, scripts |
| `RenderLayers` | `mask` (uint32 bitmask) |

## World

The `World` owns all entity storage, resources, and events. Core operations:

### Spawning

```cpp
// Empty entity
Entity e = world.spawn();

// With components
Entity e = world.spawn(
    Tag{.name = "Player"},
    Transform{.position = {0, 1, 0}},
    MeshRenderer{.mesh = mesh_handle}
);
```

All component types in a single `spawn()` call must be distinct.

### Children

```cpp
Entity child = world.spawn_child(parent,
    Tag{.name = "Weapon"},
    Transform{.position = {0.5f, 0, 0}}
);

// Or with a builder callback
Entity parent = world.spawn_with_children(
    [&](World& w, Entity p) {
        w.spawn_child(p, Tag{.name = "Wheel_FL"}, Transform{});
        w.spawn_child(p, Tag{.name = "Wheel_FR"}, Transform{});
    },
    Tag{.name = "Car"}, Transform{}
);
```

### Component access

```cpp
// Get (asserts component exists)
auto& t = world.get<Transform>(entity);

// Try-get (returns nullptr if absent)
auto* rb = world.try_get<RigidBody>(entity);

// Has
if (world.has<Camera>(entity)) { ... }

// Add / overwrite
world.add(entity, PointLight{.radius = 15.0f});

// Remove
world.remove<PointLight>(entity);

// Despawn
world.despawn(entity);
```

`add()` and `remove()` move the entity between archetypes. They are
structural changes -- do not call them during iteration. Use `Commands`
instead.

## Queries

A `Query` iterates entities that match a set of component constraints.

```cpp
auto q = world.query<Transform, const MeshRenderer>();
for (auto [t, mr] : q) {
    t.position.y += 1.0f;  // mutable
    draw(mr);               // const
}
```

### With entity ID

```cpp
for (auto [entity, t, mr] : q.with_entity()) {
    if (entity == selected) { ... }
}
```

### Parameter types

| Parameter | Effect |
|---|---|
| `T` | Mutable reference `T&`, requires component |
| `const T` | Const reference `const T&`, requires component |
| `With<T>` | Requires component, does not fetch |
| `Without<T>` | Excludes entities with component |
| `Optional<T>` | Yields `T*` (nullptr if absent) |
| `Changed<T>` | Per-entity filter: only yields if T was modified since the system last ran |

### Single-entity lookup

```cpp
auto result = q.get(entity);
if (result.has_value()) {
    auto& [t, mr] = *result;
}
```

### Snapshot semantics

Matching archetypes are cached at query construction. Archetypes created
after construction are invisible to that query instance.

## Systems

A system is a free function whose parameters are automatically injected
from the `World`:

```cpp
void move_system(Query<Transform, const Velocity> movers, Res<Time> time) {
    for (auto [t, v] : movers) {
        t.position += v.direction * v.speed * time->delta();
    }
}

app.add_system(Schedule::Update, move_system, "move_system");
```

### System parameters

| Type | Access | Description |
|---|---|---|
| `Query<Ts...>` | Per component | Iterate matching entities |
| `Res<T>` | Read | Immutable resource reference |
| `ResMut<T>` | Write | Mutable resource reference |
| `Commands` | Write (deferred) | Queue spawn/despawn/add/remove for end of stage |
| `EventReader<T>` | Read | Read events sent last frame |
| `EventWriter<T>` | Write | Send events (visible next frame) |
| `World&` | Exclusive | Direct world access, forces serial execution |

### Commands (deferred mutations)

Structural changes during system execution must go through `Commands`:

```cpp
void spawner(Commands& cmds) {
    auto builder = cmds.spawn();
    builder.insert(Tag{.name = "Bullet"});
    builder.insert(Transform{});

    cmds.despawn(old_entity);
    cmds.insert(entity, PointLight{});
    cmds.remove<Disabled>(entity);
    cmds.insert_resource(MyNewResource{});
}
```

Commands are applied after each execution stage, before the next stage
starts.

### Events

Register an event type, then send and receive:

```cpp
struct DamageEvent { Entity target; float amount; };

// Plugin setup
app.add_event<DamageEvent>();

// Writer system
void deal_damage(EventWriter<DamageEvent> writer) {
    writer.send(DamageEvent{target, 25.0f});
}

// Reader system (sees events from PREVIOUS frame)
void apply_damage(EventReader<DamageEvent> events, Query<Health> healths) {
    for (const auto& ev : events) {
        auto result = healths.get(ev.target);
        if (result) { auto& [hp] = *result; hp.current -= ev.amount; }
    }
}
```

Events use double buffering. Writers push into the write buffer; readers
iterate the read buffer (last frame's writes). Buffers swap at the end of
each frame.

## Scheduling

### DAG builder

The scheduler builds a **directed acyclic graph** from system access
declarations and explicit ordering constraints.

1. Each system declares its reads/writes via `SystemParam` traits (automatic).
2. Two systems conflict if they access the same type and at least one is a write.
3. Conflicting systems get a dependency edge.
4. Explicit `.after(id)` / `.before(id)` adds additional edges.
5. Topological sort (Kahn's algorithm) produces a layer assignment.
6. Systems at the same topological depth form a **parallel stage**.

```cpp
auto id_a = app.add_system(Schedule::Update, system_a, "system_a").id();
app.add_system(Schedule::Update, system_b, "system_b").after(id_a);
```

Cross-plugin ordering uses named lookup:

```cpp
app.add_system(Schedule::PreRender, my_extract, "my_extract")
   .after(app.id_of("extract_render_data"));
```

### SystemSet helper

```cpp
app.add_system(Schedule::Update, sys(my_system).after(id_a).before(id_b));
```

### Parallel execution

Enable with:

```cpp
app.enable_parallel();  // uses hardware_concurrency - 1 threads
app.enable_parallel(4); // explicit thread count
```

Systems in the same stage run in parallel on the shared `ThreadPool`.
`Commands` and `World&` parameters force serial execution (they report
`AccessMode::Write` or `AccessMode::Exclusive`).

### Access modes

| Mode | Behavior |
|---|---|
| `Read` | Shared -- multiple readers can run in parallel |
| `Write` | Exclusive -- conflicts with other reads and writes on the same type |
| `Exclusive` | Conflicts with every other system (forces its own stage) |

## Time

The `Time` resource is updated at the end of each frame:

```cpp
void my_system(Res<Time> time) {
    float dt = time->delta();       // seconds since last frame
    float elapsed = time->elapsed(); // seconds since app start
    uint64_t frame = time->frame_count();
}
```

### Fixed timestep

`FixedTimeAccumulator` drives `Schedule::FixedUpdate`:

```cpp
struct FixedTimeAccumulator {
    float timestep            = 1.0f / 60.0f;  // 60 Hz
    float remaining           = 0.0f;
    float alpha               = 0.0f;           // interpolation fraction
    uint32_t max_ticks_per_frame = 10;
};
```

Modify `timestep` at runtime to change the physics tick rate. `alpha` gives
the fractional remainder for visual interpolation.

## Hierarchy

### Parent / Children components

Parenting creates a `Parent` component on the child and a `Children`
component on the parent. Use the free functions:

```cpp
#include <helios/ecs/hierarchy.h>

set_parent(world, child, parent);
unparent(world, child);
```

`set_parent` handles removing from the old parent, adding the `Parent`
component, and updating the new parent's `Children` list.

### Transform propagation

After `PostUpdate`, the engine walks the hierarchy top-down and recomputes
`GlobalTransform = parent.GlobalTransform * child.Transform.to_mat4()`.
Change detection skips subtrees where neither parent nor child changed.

## Change Detection

Every component column tracks a per-row **change tick**. Mutating a component
(through a query or direct access) stamps the current world tick. The
`Changed<T>` query filter compares this against the system's `last_run_tick`
to skip unchanged entities.

```cpp
void on_transform_changed(Query<const Transform, Changed<Transform>> changed) {
    for (auto [t] : changed) {
        // Only entities whose Transform was modified since we last ran
    }
}
```

# State Management (GameFlow) --- Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the full state management subsystem: `State<StateEnum>` base class with entity tracking and auto-despawn, `StateBuilder<S>` for declarative state configuration, `GameFlow<StateEnum>` resource (state stack with push/pop/go_to/switch_to and opaque/transparent/pause_below modifiers), state transitions through intermediate states, `GameFlowPlugin<StateEnum>`, and dynamic scheduler integration so only active-state systems run.

**Architecture:** States are RAII classes (constructor = enter, destructor = exit). Each state declares its systems and stack behavior via a static `describe()` method and `StateBuilder`. The `GameFlow` resource manages an ordered stack of active states. Stack modifiers control whether states below the top continue running (transparent), stop running (opaque/pause_below), or have their entities hidden (opaque). The `GameFlowPlugin` inserts the `GameFlow` resource and runs a state-transition system each frame that applies pending operations, constructs/destroys state objects, and registers/unregisters their systems with the `Scheduler`.

**Tech Stack:** C++20, helios-core (ECS, App, Plugin, Scheduler)

**Spec:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` -- Section 3

**Dependencies:** Plans 1-2 (ECS core: World, Entity, Commands, Scheduler, App, Plugin). These are assumed to exist and compile.

**Target files:**
- `helios-core/src/app/state.h`
- `helios-core/src/app/state.cpp`
- `helios-core/src/app/state_builder.h`
- `helios-core/src/app/game_flow.h`
- `helios-core/src/app/game_flow.cpp`
- `helios-core/src/app/game_flow_plugin.h`
- `helios-core/src/app/transition.h`
- `helios-core/tests/test_state.cpp`
- `helios-core/tests/test_game_flow.cpp`
- `helios-core/tests/test_transitions.cpp`
- `helios-core/CMakeLists.txt` (modify)

---

### Task 1: State modifier enum and forward declarations

**Files:**
- Create: `helios-core/src/app/state.h`

- [ ] **Step 1: Create the StateModifier enum and forward declarations header content**

This enum is used by `StateBuilder` and `GameFlow` to determine how stacked states affect each other. Define it at the top of `state.h` alongside the `State` base class template (Task 2 adds the class body).

```cpp
// helios-core/src/app/state.h
#pragma once

#include "ecs/entity.h"
#include "ecs/world.h"
#include "ecs/commands.h"

#include <vector>
#include <functional>
#include <typeindex>
#include <string>

namespace helios {

// Forward declarations
class World;
class Commands;
class Scheduler;
template<typename S> class StateBuilder;

/// Determines how a state on the stack affects states below it.
enum class StateModifier {
    /// States below stop updating, their tracked entities are hidden
    /// (despawned on next transition). Full-screen change.
    Opaque,

    /// States below keep running and their entities remain visible.
    /// Used for HUD overlays, debug tools.
    Transparent,

    /// States below stop updating but their entities remain visible
    /// (frozen in place). Used for pause menus.
    PauseBelow,
};

} // namespace helios
```

- [ ] **Step 2: Verify the file compiles**

```bash
# From helios-core/
# Ensure the header is self-contained (no missing includes).
cd helios-core && g++ -std=c++20 -fsyntax-only -Isrc src/app/state.h
```

---

### Task 2: State base class template with entity tracking

**Files:**
- Modify: `helios-core/src/app/state.h`
- Create: `helios-core/src/app/state.cpp`

- [ ] **Step 1: Add the State base class template to state.h**

Append below the `StateModifier` enum (still inside `namespace helios`):

```cpp
/// Type-erased interface for state stack entries. GameFlow stores
/// std::unique_ptr<StateBase> so it can hold any State<E> in one container.
class StateBase {
public:
    virtual ~StateBase() = default;

    /// Returns the type_index of the concrete state class.
    virtual std::type_index type_id() const = 0;

    /// Returns the modifier declared via StateBuilder::describe().
    virtual StateModifier modifier() const = 0;

    /// Despawn all tracked entities through the world. Called automatically
    /// by GameFlow when the state is popped/replaced.
    void despawn_tracked(World& world);

protected:
    /// Spawn an entity owned by this state. It will be automatically
    /// despawned when the state exits (destructor or explicit pop).
    Entity spawn_tracked(World& world);

    /// Direct world access for constructor setup.
    /// Only valid during construction (the World& passed to the constructor).
    World& world();

    /// Set by GameFlow before constructing the state.
    void bind_world(World& world);

private:
    World* m_world = nullptr;
    std::vector<Entity> m_tracked_entities;
};

/// CRTP base for concrete states. StateEnum is the user's enum that names
/// all possible states (e.g., enum class GameState { MainMenu, Playing, ... }).
///
/// Concrete states derive from this:
///   class Playing : public State<GameState> { ... };
///
/// Each concrete state MUST provide:
///   static void describe(StateBuilder<Playing>& s);
template<typename StateEnum>
class State : public StateBase {
public:
    using enum_type = StateEnum;

    ~State() override = default;

    std::type_index type_id() const override {
        // Returns the type of the most-derived class via CRTP trick:
        // Concrete states override nothing -- type_id is resolved from
        // the static type at registration time (see StateBuilder).
        // This default returns the State<E> type, which is overridden
        // per-concrete-state in StateEntry (Task 5).
        return std::type_index(typeid(*this));
    }

    StateModifier modifier() const override {
        return m_modifier;
    }

    void set_modifier(StateModifier mod) {
        m_modifier = mod;
    }

private:
    StateModifier m_modifier = StateModifier::Opaque;
};
```

- [ ] **Step 2: Implement StateBase methods in state.cpp**

```cpp
// helios-core/src/app/state.cpp
#include "app/state.h"
#include "ecs/world.h"

namespace helios {

void StateBase::bind_world(World& world) {
    m_world = &world;
}

World& StateBase::world() {
    assert(m_world != nullptr && "State::world() called outside of construction");
    return *m_world;
}

Entity StateBase::spawn_tracked(World& world) {
    Entity e = world.spawn();
    m_tracked_entities.push_back(e);
    return e;
}

void StateBase::despawn_tracked(World& world) {
    for (Entity e : m_tracked_entities) {
        if (world.is_alive(e)) {
            world.despawn(e);
        }
    }
    m_tracked_entities.clear();
}

} // namespace helios
```

- [ ] **Step 3: Verify compilation**

```bash
cd helios-core && g++ -std=c++20 -c -Isrc src/app/state.cpp -o /dev/null
```

---

### Task 3: StateBuilder -- declarative state configuration

**Files:**
- Create: `helios-core/src/app/state_builder.h`

- [ ] **Step 1: Write the StateBuilder class template**

`StateBuilder<S>` collects the modifier and system registrations for a concrete state `S`. The static `S::describe(StateBuilder<S>&)` method calls `system()`, `opaque()`, `transparent()`, `pause_below()` on it. `GameFlow` reads the collected data to configure the scheduler.

```cpp
// helios-core/src/app/state_builder.h
#pragma once

#include "app/state.h"
#include "ecs/scheduler.h"

#include <vector>
#include <functional>
#include <string>
#include <typeindex>

namespace helios {

/// Descriptor for one system method belonging to a state.
struct StateSystemDescriptor {
    /// Type-erased function that, given a StateBase* (downcast to S*) and
    /// a World&, invokes the member function with its declared parameters
    /// extracted from the World.
    std::function<void(StateBase*, World&)> invoke;

    /// The schedule this system runs in (default: Update).
    Schedule schedule = Schedule::Update;

    /// Human-readable name for debugging (e.g., "Playing::player_movement").
    std::string name;

    /// Access descriptors extracted from the member function's parameters.
    /// Used by the Scheduler for dependency-graph construction.
    std::vector<AccessDescriptor> reads;
    std::vector<AccessDescriptor> writes;
};

/// Collects configuration for a concrete state S during S::describe().
/// S must derive from State<SomeEnum>.
template<typename S>
class StateBuilder {
public:
    StateBuilder() = default;

    // ── Modifiers ──────────────────────────────────────────────────────

    /// States below stop updating, their entities are hidden.
    StateBuilder& opaque() {
        m_modifier = StateModifier::Opaque;
        return *this;
    }

    /// States below keep running and remain visible.
    StateBuilder& transparent() {
        m_modifier = StateModifier::Transparent;
        return *this;
    }

    /// States below stop updating but their entities remain visible.
    StateBuilder& pause_below() {
        m_modifier = StateModifier::PauseBelow;
        return *this;
    }

    // ── System registration ────────────────────────────────────────────

    /// Register a member function of S as a system for this state.
    ///
    /// Usage:
    ///   s.system(&Playing::player_movement);
    ///   s.system(&Playing::check_pause);
    ///
    /// The member function's parameters are resolved from the World just
    /// like free-function systems (Query<>, Res<>, ResMut<>, Commands&, etc.).
    /// The Scheduler extracts read/write access from the parameter types.
    template<typename MemFn>
    StateBuilder& system(MemFn fn) {
        StateSystemDescriptor desc;
        desc.name = std::string(typeid(S).name()) + "::<member>";
        desc.schedule = Schedule::Update; // default schedule

        // Build the type-erased invoker.
        // MemFn is a pointer-to-member like: void (S::*)(Query<T>, Res<U>)
        // We wrap it so that the scheduler can call it with (StateBase*, World&).
        desc.invoke = [fn](StateBase* base, World& world) {
            S* self = static_cast<S*>(base);
            // invoke_member_system introspects the member function's parameter
            // list, extracts each parameter from the World (same mechanism as
            // free-function systems), and calls (self->*fn)(args...).
            invoke_member_system(self, fn, world);
        };

        // Extract access metadata from MemFn's parameter types.
        // Uses the same compile-time parameter introspection as the Scheduler
        // uses for free functions, but skips the implicit this pointer.
        extract_member_system_access<MemFn>(desc.reads, desc.writes);

        m_systems.push_back(std::move(desc));
        return *this;
    }

    /// Register a member function on a specific schedule (not Update).
    ///
    /// Usage:
    ///   s.system(&Playing::player_movement, Schedule::FixedUpdate);
    template<typename MemFn>
    StateBuilder& system(MemFn fn, Schedule sched) {
        system(fn);
        m_systems.back().schedule = sched;
        return *this;
    }

    // ── Accessors (read by GameFlow) ───────────────────────────────────

    StateModifier get_modifier() const { return m_modifier; }
    const std::vector<StateSystemDescriptor>& get_systems() const { return m_systems; }

private:
    StateModifier m_modifier = StateModifier::Opaque;
    std::vector<StateSystemDescriptor> m_systems;
};

// ── Helper: invoke a member-function system with World-extracted params ──
//
// This uses the same parameter-extraction machinery as the Scheduler's
// free-function system invocation. The implementation relies on the
// function_traits utility (assumed from Plan 1/2) that decomposes a
// callable's parameter list at compile time.
//
// Sketch (actual implementation lives in ecs/system_traits.h from Plan 1):

/// Extracts each parameter type from MemFn, fetches the value from World,
/// and invokes (obj->*fn)(param0, param1, ...).
template<typename S, typename Ret, typename... Params>
void invoke_member_system_impl(S* obj, Ret(S::*fn)(Params...), World& world) {
    // Each param is resolved: Query<T> -> world.query<T>(),
    // Res<T> -> world.resource<T>(), etc.
    // Uses the same resolve_param<P>(world) helper from the Scheduler.
    (obj->*fn)(resolve_param<Params>(world)...);
}

template<typename S, typename MemFn>
void invoke_member_system(S* obj, MemFn fn, World& world) {
    invoke_member_system_impl(obj, fn, world);
}

/// Extracts read/write AccessDescriptors from a member function's params.
/// Delegates to the same extract_access<Params...>() used by the Scheduler.
template<typename MemFn>
void extract_member_system_access(
    std::vector<AccessDescriptor>& reads,
    std::vector<AccessDescriptor>& writes
) {
    // Uses function_traits<MemFn>::param_types to get the parameter pack,
    // then calls extract_access_from_params<Params...>(reads, writes).
    // This is the same compile-time introspection the Scheduler uses.
    extract_access_from_member<MemFn>(reads, writes);
}

} // namespace helios
```

- [ ] **Step 2: Verify the header is syntactically valid**

```bash
cd helios-core && g++ -std=c++20 -fsyntax-only -Isrc src/app/state_builder.h
```

---

### Task 4: GameFlow resource -- state stack core

**Files:**
- Create: `helios-core/src/app/game_flow.h`
- Create: `helios-core/src/app/game_flow.cpp`

- [ ] **Step 1: Define the StateEntry struct and GameFlow class in game_flow.h**

`StateEntry` wraps a live state instance with its metadata (modifier, systems, enum value). `GameFlow<StateEnum>` owns the stack of entries and queues operations that are applied during the transition system.

```cpp
// helios-core/src/app/game_flow.h
#pragma once

#include "app/state.h"
#include "app/state_builder.h"

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

namespace helios {

// Forward declarations
class World;
class Scheduler;

/// Metadata for a registered state type. Created once when the user
/// registers a state via GameFlowPlugin or App::state<S>().
template<typename StateEnum>
struct StateRegistration {
    /// The enum value associated with this state type.
    StateEnum id;

    /// Modifier declared in S::describe().
    StateModifier modifier = StateModifier::Opaque;

    /// System descriptors collected from S::describe().
    std::vector<StateSystemDescriptor> systems;

    /// Factory: constructs a new instance of the state, bound to a World.
    std::function<std::unique_ptr<StateBase>(World&)> factory;

    /// type_index of the concrete state class.
    std::type_index type = typeid(void);
};

/// A live state on the stack: owns the state object and references its
/// registration for modifier/system info.
template<typename StateEnum>
struct StateEntry {
    std::unique_ptr<StateBase> instance;
    const StateRegistration<StateEnum>* registration = nullptr;

    StateEnum id() const { return registration->id; }
    StateModifier modifier() const { return registration->modifier; }
};

// ── Pending operations (queued, applied during transition system) ───────

namespace detail {

template<typename StateEnum>
struct OpGoTo {
    std::vector<std::type_index> state_types;
};

template<typename StateEnum>
struct OpPush {
    std::type_index state_type;
};

struct OpPop {};

template<typename StateEnum>
struct OpSwitchTo {
    std::type_index state_type;
};

template<typename StateEnum>
using PendingOp = std::variant<
    OpGoTo<StateEnum>,
    OpPush<StateEnum>,
    OpPop,
    OpSwitchTo<StateEnum>
>;

} // namespace detail

/// Manages a stack of active game states.
///
/// Usage (as a resource accessed through ResMut<GameFlow<GameState>>):
///   flow->push<PauseMenu>();
///   flow->pop();
///   flow->go_to<Playing>();
///   flow->switch_to<MainMenu>();
///   flow->is_in(GameState::Playing);
///   flow->current();
template<typename StateEnum>
class GameFlow {
public:
    GameFlow() = default;

    // ── Stack operations (queue a pending operation) ───────────────────

    /// Clear the entire stack and push one or more new states.
    /// The states are pushed left-to-right: the rightmost ends up on top.
    ///
    ///   flow->go_to<MainMenu>();           // stack: [MainMenu]
    ///   flow->go_to<WorldMap, Playing>();   // stack: [WorldMap, Playing]
    template<typename... States>
    void go_to() {
        detail::OpGoTo<StateEnum> op;
        (op.state_types.push_back(std::type_index(typeid(States))), ...);
        m_pending.push_back(std::move(op));
    }

    /// Push a state on top of the stack. Existing states stay.
    template<typename S>
    void push() {
        m_pending.push_back(detail::OpPush<StateEnum>{
            .state_type = std::type_index(typeid(S))
        });
    }

    /// Remove the top state from the stack.
    void pop() {
        m_pending.push_back(detail::OpPop{});
    }

    /// Replace the top state with a new state.
    template<typename S>
    void switch_to() {
        m_pending.push_back(detail::OpSwitchTo<StateEnum>{
            .state_type = std::type_index(typeid(S))
        });
    }

    // ── Queries ────────────────────────────────────────────────────────

    /// Returns the enum value of the topmost state.
    StateEnum current() const {
        assert(!m_stack.empty() && "GameFlow::current() called on empty stack");
        return m_stack.back().id();
    }

    /// Returns true if the given enum value is anywhere on the stack.
    bool is_in(StateEnum state) const {
        for (const auto& entry : m_stack) {
            if (entry.id() == state) return true;
        }
        return false;
    }

    /// Returns true if the stack is empty.
    bool is_empty() const {
        return m_stack.empty();
    }

    /// Returns the number of states on the stack.
    size_t depth() const {
        return m_stack.size();
    }

    // ── Registration (called by GameFlowPlugin / App during setup) ─────

    /// Register a concrete state type S associated with a given enum value.
    /// Calls S::describe() to collect modifier and system info.
    template<typename S>
    void register_state(StateEnum id) {
        StateRegistration<StateEnum> reg;
        reg.id = id;
        reg.type = std::type_index(typeid(S));

        // Collect modifier and systems from S::describe().
        StateBuilder<S> builder;
        S::describe(builder);
        reg.modifier = builder.get_modifier();
        reg.systems = builder.get_systems();

        // Factory: constructs S, passing World& to its constructor.
        reg.factory = [mod = reg.modifier](World& world) -> std::unique_ptr<StateBase> {
            auto state = std::make_unique<S>(world);
            state->set_modifier(mod);
            return state;
        };

        m_registrations[reg.type] = std::move(reg);
    }

    // ── Transition processing (called by the state management system) ──

    /// Returns true if there are pending operations to process.
    bool has_pending() const {
        return !m_pending.empty();
    }

    /// Apply all pending operations. This is called once per frame by
    /// the state management system (see GameFlowPlugin).
    ///
    /// For each operation:
    ///   - Constructs/destructs state objects (RAII enter/exit).
    ///   - Calls despawn_tracked() on exiting states.
    ///   - Registers/unregisters systems with the Scheduler.
    void apply_pending(World& world, Scheduler& scheduler);

    // ── Active systems query (used by Scheduler integration) ───────────

    /// Computes which states on the stack are "active" (should have their
    /// systems running) based on stack modifiers.
    ///
    /// Walk from top of stack downward:
    ///   - The top state is always active.
    ///   - If top state is Transparent: state below is also active, continue.
    ///   - If top state is PauseBelow: stop (below is visible but frozen).
    ///   - If top state is Opaque: stop (below is hidden).
    std::vector<const StateEntry<StateEnum>*> active_states() const {
        std::vector<const StateEntry<StateEnum>*> result;
        if (m_stack.empty()) return result;

        // Walk from top of stack downward
        for (int i = static_cast<int>(m_stack.size()) - 1; i >= 0; --i) {
            result.push_back(&m_stack[i]);

            // If this state is not transparent, states below it are not active
            if (m_stack[i].modifier() != StateModifier::Transparent) {
                break;
            }
        }

        return result;
    }

    /// Returns the full stack (bottom to top) for inspection/debugging.
    const std::vector<StateEntry<StateEnum>>& stack() const {
        return m_stack;
    }

private:
    /// Find the registration for a given type_index.
    const StateRegistration<StateEnum>& find_registration(std::type_index type) const {
        auto it = m_registrations.find(type);
        assert(it != m_registrations.end() && "State type not registered with GameFlow");
        return it->second;
    }

    /// Construct a state from its registration and push it onto the stack.
    void push_state(const StateRegistration<StateEnum>& reg, World& world);

    /// Pop the top state, despawn its tracked entities, destroy it.
    void pop_state(World& world);

    /// Register systems for a state entry with the scheduler.
    void activate_state_systems(const StateEntry<StateEnum>& entry, Scheduler& scheduler);

    /// Unregister systems for a state entry from the scheduler.
    void deactivate_state_systems(const StateEntry<StateEnum>& entry, Scheduler& scheduler);

    /// Recompute which states are active and sync the scheduler.
    void sync_scheduler(Scheduler& scheduler);

    std::vector<StateEntry<StateEnum>> m_stack;
    std::unordered_map<std::type_index, StateRegistration<StateEnum>> m_registrations;
    std::vector<detail::PendingOp<StateEnum>> m_pending;

    /// Tracks which state entries currently have their systems registered
    /// in the scheduler, so we can diff on state changes.
    std::vector<std::type_index> m_active_system_types;
};

} // namespace helios
```

- [ ] **Step 2: Implement GameFlow methods in game_flow.cpp**

```cpp
// helios-core/src/app/game_flow.cpp
#include "app/game_flow.h"
#include "ecs/world.h"
#include "ecs/scheduler.h"

namespace helios {

// ── Template instantiation note ────────────────────────────────────────
//
// GameFlow<StateEnum> is a class template. Most methods are defined in
// game_flow.h (inline/template). The methods below that need non-trivial
// logic are implemented as template methods in this file and explicitly
// instantiated for the user's StateEnum type via GameFlowPlugin.
//
// For a header-only approach, move these into game_flow.h. The split is
// shown here for clarity; in practice the template methods must be
// visible at instantiation, so they live in game_flow.h or a .inl file.

} // namespace helios
```

- [ ] **Step 3: Add the template method implementations at the bottom of game_flow.h**

Append before the closing `} // namespace helios`:

```cpp
// ── Template method implementations ────────────────────────────────────

template<typename StateEnum>
void GameFlow<StateEnum>::push_state(
    const StateRegistration<StateEnum>& reg,
    World& world
) {
    StateEntry<StateEnum> entry;
    entry.registration = &m_registrations.at(reg.type);

    // Construct the state (RAII enter).
    // bind_world() gives the state access to World during construction.
    auto instance = reg.factory(world);
    instance->bind_world(world);
    entry.instance = std::move(instance);

    m_stack.push_back(std::move(entry));
}

template<typename StateEnum>
void GameFlow<StateEnum>::pop_state(World& world) {
    assert(!m_stack.empty() && "GameFlow::pop_state() called on empty stack");

    auto& top = m_stack.back();

    // Despawn all entities tracked by this state.
    top.instance->despawn_tracked(world);

    // Destroy the state object (RAII exit -- destructor runs here).
    m_stack.pop_back();
}

template<typename StateEnum>
void GameFlow<StateEnum>::activate_state_systems(
    const StateEntry<StateEnum>& entry,
    Scheduler& scheduler
) {
    const auto& reg = *entry.registration;
    for (const auto& sys_desc : reg.systems) {
        // Wrap the StateSystemDescriptor into a Scheduler-compatible
        // SystemDescriptor. The system captures the state instance pointer.
        SystemDescriptor sd;
        sd.name = sys_desc.name;
        sd.reads = sys_desc.reads;
        sd.writes = sys_desc.writes;

        // Capture raw pointer to the state instance. This is safe because
        // the state is alive on the stack for as long as its systems are
        // registered. We tag the SystemDescriptor with the state's type
        // so we can remove it later.
        StateBase* raw = entry.instance.get();
        sd.run = [raw, invoke = sys_desc.invoke](World& world) {
            invoke(raw, world);
        };
        sd.owner_tag = reg.type;  // used for bulk removal

        scheduler.add_system(sys_desc.schedule, std::move(sd));
    }
}

template<typename StateEnum>
void GameFlow<StateEnum>::deactivate_state_systems(
    const StateEntry<StateEnum>& entry,
    Scheduler& scheduler
) {
    // Remove all systems tagged with this state's type_index.
    scheduler.remove_systems_by_tag(entry.registration->type);
}

template<typename StateEnum>
void GameFlow<StateEnum>::sync_scheduler(Scheduler& scheduler) {
    // Determine which states should be active.
    auto active = active_states();

    // Build set of type_indices for active states.
    std::vector<std::type_index> new_active;
    new_active.reserve(active.size());
    for (const auto* entry : active) {
        new_active.push_back(entry->registration->type);
    }

    // Deactivate states that were active but are no longer.
    for (const auto& old_type : m_active_system_types) {
        bool still_active = false;
        for (const auto& new_type : new_active) {
            if (new_type == old_type) { still_active = true; break; }
        }
        if (!still_active) {
            // Find entry on the stack (it might still be there but inactive).
            for (const auto& entry : m_stack) {
                if (entry.registration->type == old_type) {
                    deactivate_state_systems(entry, scheduler);
                    break;
                }
            }
        }
    }

    // Activate states that are newly active.
    for (const auto* entry : active) {
        bool was_active = false;
        for (const auto& old_type : m_active_system_types) {
            if (old_type == entry->registration->type) { was_active = true; break; }
        }
        if (!was_active) {
            activate_state_systems(*entry, scheduler);
        }
    }

    m_active_system_types = std::move(new_active);
}

template<typename StateEnum>
void GameFlow<StateEnum>::apply_pending(World& world, Scheduler& scheduler) {
    if (m_pending.empty()) return;

    // Process all pending operations in order.
    for (auto& op : m_pending) {
        std::visit([&](auto& concrete_op) {
            using T = std::decay_t<decltype(concrete_op)>;

            if constexpr (std::is_same_v<T, detail::OpGoTo<StateEnum>>) {
                // Clear entire stack.
                while (!m_stack.empty()) {
                    deactivate_state_systems(m_stack.back(), scheduler);
                    pop_state(world);
                }
                m_active_system_types.clear();

                // Push new states in order (left-to-right, rightmost = top).
                for (const auto& type : concrete_op.state_types) {
                    const auto& reg = find_registration(type);
                    push_state(reg, world);
                }
            }
            else if constexpr (std::is_same_v<T, detail::OpPush<StateEnum>>) {
                const auto& reg = find_registration(concrete_op.state_type);
                push_state(reg, world);
            }
            else if constexpr (std::is_same_v<T, detail::OpPop>) {
                if (!m_stack.empty()) {
                    deactivate_state_systems(m_stack.back(), scheduler);
                    pop_state(world);
                }
            }
            else if constexpr (std::is_same_v<T, detail::OpSwitchTo<StateEnum>>) {
                // Replace top: pop then push.
                if (!m_stack.empty()) {
                    deactivate_state_systems(m_stack.back(), scheduler);
                    pop_state(world);
                }
                const auto& reg = find_registration(concrete_op.state_type);
                push_state(reg, world);
            }
        }, op);
    }

    m_pending.clear();

    // After all operations, recompute active states and sync the scheduler.
    sync_scheduler(scheduler);
}
```

- [ ] **Step 4: Verify compilation**

```bash
cd helios-core && g++ -std=c++20 -fsyntax-only -Isrc src/app/game_flow.h
```

---

### Task 5: Opaque modifier -- entity visibility control

**Files:**
- Modify: `helios-core/src/app/game_flow.h`

This task adds the logic for the opaque modifier's entity-hiding behavior. When an opaque state is pushed, entities tracked by states below it are hidden (via a `Hidden` marker component). When the opaque state is popped, they are restored.

- [ ] **Step 1: Define the Hidden marker component**

Add to the top of `game_flow.h` (or a shared components header):

```cpp
/// Marker component added to entities that should be hidden from rendering
/// because an opaque state is stacked above their owning state.
struct Hidden {};
```

- [ ] **Step 2: Add hide/unhide helpers to GameFlow**

Add as private methods in the `GameFlow` class:

```cpp
    /// Hide all tracked entities of states below the given stack index
    /// by adding a Hidden marker component.
    void hide_entities_below(size_t index, World& world) {
        for (size_t i = 0; i < index; ++i) {
            auto& entry = m_stack[i];
            // The state's tracked entities get a Hidden component.
            // This is read by the render extraction system to skip them.
            // We access the tracked entities through the StateBase interface.
            // StateBase exposes tracked entities for this purpose.
            mark_tracked_hidden(entry, world, true);
        }
    }

    /// Unhide all tracked entities of states below the given stack index
    /// by removing the Hidden marker component.
    void unhide_entities_below(size_t index, World& world) {
        for (size_t i = 0; i < index; ++i) {
            mark_tracked_hidden(m_stack[i], world, false);
        }
    }

    void mark_tracked_hidden(StateEntry<StateEnum>& entry, World& world, bool hide) {
        // StateBase provides access to its tracked entity list for this purpose.
        for (Entity e : entry.instance->tracked_entities()) {
            if (!world.is_alive(e)) continue;
            if (hide) {
                if (!world.has<Hidden>(e)) {
                    world.add(e, Hidden{});
                }
            } else {
                if (world.has<Hidden>(e)) {
                    world.remove<Hidden>(e);
                }
            }
        }
    }
```

- [ ] **Step 3: Add tracked_entities() accessor to StateBase**

In `state.h`, add to `StateBase`'s public interface:

```cpp
    /// Returns the list of entities tracked by this state.
    /// Used by GameFlow for opaque-state entity hiding.
    const std::vector<Entity>& tracked_entities() const {
        return m_tracked_entities;
    }
```

- [ ] **Step 4: Integrate hide/unhide into apply_pending**

After `sync_scheduler(scheduler)` at the bottom of `apply_pending()`, add:

```cpp
    // Handle opaque modifier entity visibility.
    // Walk the stack top-down: if the top state is opaque, hide entities below.
    if (!m_stack.empty()) {
        const auto& top = m_stack.back();
        if (top.modifier() == StateModifier::Opaque) {
            hide_entities_below(m_stack.size() - 1, world);
        } else {
            // Transparent or PauseBelow: entities below should be visible.
            unhide_entities_below(m_stack.size(), world);
        }
    }
```

---

### Task 6: Transition chains -- intermediate states

**Files:**
- Create: `helios-core/src/app/transition.h`
- Modify: `helios-core/src/app/game_flow.h`

- [ ] **Step 1: Define the TransitionDef and TransitionBuilder**

Transitions define that going from state A to state B should route through an intermediate state (e.g., a loading screen). The intermediate state calls `flow->pop()` when done, which then proceeds to the target state.

```cpp
// helios-core/src/app/transition.h
#pragma once

#include <typeindex>
#include <unordered_map>
#include <vector>
#include <optional>

namespace helios {

/// Describes a transition chain: From -> [Via...] -> To.
/// When GameFlow detects a go_to/switch_to that matches (From, To),
/// it inserts the intermediate states.
template<typename StateEnum>
struct TransitionDef {
    StateEnum from;
    StateEnum to;
    /// Intermediate state types to push between From and To.
    /// The chain is: pop From, push Via[0], ... Via[N], push To.
    /// Each Via state pops itself when done. When the last Via pops,
    /// To becomes the active top.
    std::vector<std::type_index> via_types;
};

/// Builder returned by app.transition(From, To).
/// Allows chaining .via<LoadingScreen>().
template<typename StateEnum>
class TransitionBuilder {
public:
    TransitionBuilder(StateEnum from, StateEnum to,
                      std::vector<TransitionDef<StateEnum>>& registry)
        : m_from(from), m_to(to), m_registry(registry) {}

    /// Add an intermediate state to the transition chain.
    ///   app.transition(GameState::MainMenu, GameState::Playing)
    ///       .via<LoadingScreen>();
    template<typename ViaState>
    TransitionBuilder& via() {
        m_via_types.push_back(std::type_index(typeid(ViaState)));
        return *this;
    }

    /// Finalizes the transition definition when the builder is destroyed.
    ~TransitionBuilder() {
        if (!m_via_types.empty()) {
            TransitionDef<StateEnum> def;
            def.from = m_from;
            def.to = m_to;
            def.via_types = std::move(m_via_types);
            m_registry.push_back(std::move(def));
        }
    }

    // Non-copyable, movable
    TransitionBuilder(const TransitionBuilder&) = delete;
    TransitionBuilder& operator=(const TransitionBuilder&) = delete;
    TransitionBuilder(TransitionBuilder&& other) noexcept
        : m_from(other.m_from), m_to(other.m_to),
          m_registry(other.m_registry),
          m_via_types(std::move(other.m_via_types)) {
        other.m_via_types.clear();  // prevent double-registration
    }

private:
    StateEnum m_from;
    StateEnum m_to;
    std::vector<TransitionDef<StateEnum>>& m_registry;
    std::vector<std::type_index> m_via_types;
};

} // namespace helios
```

- [ ] **Step 2: Add transition storage and lookup to GameFlow**

Add the following members and methods to the `GameFlow` class:

```cpp
    // ── Transition registration ────────────────────────────────────────

    /// Define a transition chain from one state to another.
    ///   flow->transition(GameState::MainMenu, GameState::Playing)
    ///       .via<LoadingScreen>();
    TransitionBuilder<StateEnum> transition(StateEnum from, StateEnum to) {
        return TransitionBuilder<StateEnum>(from, to, m_transitions);
    }

    // ... (private section)

    /// Look up whether a transition chain exists for (from, to).
    /// Returns nullptr if no chain is defined (direct transition).
    const TransitionDef<StateEnum>* find_transition(StateEnum from, StateEnum to) const {
        for (const auto& def : m_transitions) {
            if (def.from == from && def.to == to) {
                return &def;
            }
        }
        return nullptr;
    }

    std::vector<TransitionDef<StateEnum>> m_transitions;
```

- [ ] **Step 3: Integrate transitions into go_to and switch_to processing**

Inside `apply_pending()`, modify the `OpGoTo` and `OpSwitchTo` handlers so that before pushing the target state, they check for a registered transition chain. If a chain exists, they push the intermediate states first, then push the target state under them so that when the last intermediate pops, the target is revealed.

In the `OpGoTo` handler, after clearing the stack but before pushing new states:

```cpp
    // Check for transition chains.
    // If the current top (before clear) was state From and the new
    // target includes state To, look up From->To chains.
    // For go_to with multiple states, check the last state as the target.
    if (concrete_op.state_types.size() == 1) {
        StateEnum target_id = find_registration(concrete_op.state_types[0]).id;
        StateEnum from_id = /* captured before stack clear */;
        const auto* chain = find_transition(from_id, target_id);
        if (chain) {
            // Push target first (bottom), then intermediates on top.
            const auto& target_reg = find_registration(concrete_op.state_types[0]);
            push_state(target_reg, world);
            // Push Via states on top (last via = top of stack).
            for (const auto& via_type : chain->via_types) {
                const auto& via_reg = find_registration(via_type);
                push_state(via_reg, world);
            }
            // Skip the normal push below.
            return;  // (inside the lambda -- use a flag in practice)
        }
    }
```

- [ ] **Step 4: Document the transition lifecycle**

Add a comment block in `transition.h` explaining the full lifecycle:

```cpp
// Transition lifecycle example:
//
//   app.transition(GameState::MainMenu, GameState::Playing)
//       .via<LoadingScreen>();
//
//   // User calls: flow->go_to<Playing>();  (while in MainMenu)
//
//   Frame N:
//     1. MainMenu destructor runs (RAII exit, tracked entities despawned).
//     2. Playing is constructed and pushed (but will be below LoadingScreen).
//     3. LoadingScreen is constructed and pushed on top.
//     4. Stack: [Playing, LoadingScreen]  (LoadingScreen is top)
//     5. LoadingScreen is opaque, so Playing's systems do NOT run yet.
//
//   Frame N+1 ... N+K:
//     LoadingScreen::render_progress() runs, shows loading bar.
//
//   Frame N+K (loading complete):
//     LoadingScreen calls flow->pop().
//
//   Frame N+K+1:
//     1. LoadingScreen destructor runs.
//     2. Stack: [Playing]
//     3. Playing's systems are now activated.
```

---

### Task 7: GameFlowPlugin -- plugin wiring

**Files:**
- Create: `helios-core/src/app/game_flow_plugin.h`

- [ ] **Step 1: Write the GameFlowPlugin class template**

This plugin inserts the `GameFlow<StateEnum>` resource and registers the state-management system that processes pending operations each frame.

```cpp
// helios-core/src/app/game_flow_plugin.h
#pragma once

#include "app/game_flow.h"
#include "app/state.h"
#include "app/state_builder.h"
#include "app/transition.h"
#include "ecs/scheduler.h"

namespace helios {

/// Plugin that provides state management for a given StateEnum.
///
/// Usage:
///   enum class GameState { MainMenu, Playing, Paused };
///
///   app.add_plugin(GameFlowPlugin<GameState>{}
///       .state<MainMenu>(GameState::MainMenu)
///       .state<Playing>(GameState::Playing)
///       .state<PauseMenu>(GameState::Paused)
///       .initial<MainMenu>()
///   );
template<typename StateEnum>
class GameFlowPlugin {
public:
    GameFlowPlugin() = default;

    /// Register a concrete state class S with an enum value.
    /// S must have: static void describe(StateBuilder<S>& s);
    template<typename S>
    GameFlowPlugin& state(StateEnum id) {
        m_registrations.push_back([id](GameFlow<StateEnum>& flow) {
            flow.template register_state<S>(id);
        });
        return *this;
    }

    /// Set the initial state that is pushed at startup.
    template<typename S>
    GameFlowPlugin& initial() {
        m_initial_type = std::type_index(typeid(S));
        return *this;
    }

    /// Define a transition chain (forwarded to GameFlow).
    TransitionBuilder<StateEnum> transition(StateEnum from, StateEnum to) {
        // We can't call this on GameFlow yet (it doesn't exist until build()),
        // so we store the from/to and return a builder that writes into our
        // local storage, which we transfer to GameFlow in build().
        return TransitionBuilder<StateEnum>(from, to, m_transitions);
    }

    /// Plugin interface: called by App::add_plugin().
    void build(App& app) {
        // 1. Create the GameFlow resource.
        GameFlow<StateEnum> flow;

        // 2. Register all state types.
        for (auto& reg_fn : m_registrations) {
            reg_fn(flow);
        }

        // 3. Transfer transition definitions.
        flow.m_transitions = std::move(m_transitions);
        // NOTE: GameFlowPlugin is a friend of GameFlow, or m_transitions
        // is accessible via a setter. We use a public method:
        // flow.set_transitions(std::move(m_transitions));

        // 4. Insert the resource into the World.
        app.insert_resource<GameFlow<StateEnum>>(std::move(flow));

        // 5. Register the state management system.
        // This system runs at the start of each frame (PreUpdate) and
        // applies any pending state operations from the previous frame.
        app.add_system(Schedule::PreUpdate, process_state_transitions<StateEnum>);

        // 6. Push the initial state if one was set.
        if (m_initial_type.has_value()) {
            // Queue the initial push. It will be applied on the first frame
            // by the state management system.
            auto& flow_ref = app.world().resource<GameFlow<StateEnum>>();
            // We need to push by type_index. Use go_to with the initial type.
            // Since we can't use template push<S>() with a runtime type,
            // we store a lambda that does it.
            if (m_initial_push) {
                m_initial_push(flow_ref);
            }
        }
    }

    // Overload of initial() that also captures the push lambda.
    // (This replaces the simpler initial() above with one that stores the push.)
    // Revised: merge into single initial<S>():
    // template<typename S>
    // GameFlowPlugin& initial() {
    //     m_initial_type = std::type_index(typeid(S));
    //     m_initial_push = [](GameFlow<StateEnum>& flow) {
    //         flow.template go_to<S>();
    //     };
    //     return *this;
    // }

private:
    std::vector<std::function<void(GameFlow<StateEnum>&)>> m_registrations;
    std::optional<std::type_index> m_initial_type;
    std::function<void(GameFlow<StateEnum>&)> m_initial_push;
    std::vector<TransitionDef<StateEnum>> m_transitions;
};

/// System that processes pending state transitions.
/// Registered at Schedule::PreUpdate by GameFlowPlugin.
template<typename StateEnum>
void process_state_transitions(
    ResMut<GameFlow<StateEnum>> flow,
    ResMut<World> world,
    ResMut<Scheduler> scheduler
) {
    if (flow->has_pending()) {
        flow->apply_pending(*world, *scheduler);
    }
}

} // namespace helios
```

- [ ] **Step 2: Revise initial() to properly capture the push**

Replace the `initial()` method body with:

```cpp
    template<typename S>
    GameFlowPlugin& initial() {
        m_initial_type = std::type_index(typeid(S));
        m_initial_push = [](GameFlow<StateEnum>& flow) {
            flow.template go_to<S>();
        };
        return *this;
    }
```

- [ ] **Step 3: Add the App::transition convenience method**

In the `App` class (from Plan 2), add:

```cpp
    /// Define a state transition chain. Delegates to the GameFlow resource.
    ///   app.transition(GameState::MainMenu, GameState::Playing)
    ///       .via<LoadingScreen>();
    template<typename StateEnum>
    TransitionBuilder<StateEnum> transition(StateEnum from, StateEnum to) {
        auto& flow = m_world.resource<GameFlow<StateEnum>>();
        return flow.transition(from, to);
    }
```

---

### Task 8: Scheduler integration -- dynamic system registration

**Files:**
- Modify: `helios-core/src/ecs/scheduler.h`
- Modify: `helios-core/src/ecs/scheduler.cpp`

The Scheduler (from Plan 1) must support dynamic addition/removal of systems at runtime, and tag-based bulk removal. This task adds those capabilities.

- [ ] **Step 1: Add owner_tag to SystemDescriptor**

In `scheduler.h`, add to `SystemDescriptor`:

```cpp
struct SystemDescriptor {
    std::function<void(World&)> run;
    std::vector<AccessDescriptor> reads;
    std::vector<AccessDescriptor> writes;
    std::optional<SystemId> after;
    std::optional<SystemId> before;

    // NEW: optional tag for bulk removal (used by state management).
    std::optional<std::type_index> owner_tag;

    // NEW: human-readable name for debugging.
    std::string name;
};
```

- [ ] **Step 2: Add add_system(Schedule, SystemDescriptor) overload to Scheduler**

```cpp
class Scheduler {
public:
    // Existing: add a free-function system.
    SystemId add_system(Schedule schedule, auto&& system);

    // NEW: add a pre-built SystemDescriptor (used by state management).
    SystemId add_system(Schedule schedule, SystemDescriptor descriptor);

    // NEW: remove all systems tagged with a given owner_tag.
    // Returns the number of systems removed.
    size_t remove_systems_by_tag(std::type_index tag);

    // NEW: remove a specific system by ID.
    void remove_system(SystemId id);

    // Existing: run all systems in a schedule.
    void run(World& world, Schedule schedule);

    // ...
};
```

- [ ] **Step 3: Implement remove_systems_by_tag**

In `scheduler.cpp`:

```cpp
size_t Scheduler::remove_systems_by_tag(std::type_index tag) {
    size_t removed = 0;
    for (auto& [schedule, systems] : m_systems) {
        auto it = std::remove_if(systems.begin(), systems.end(),
            [&](const SystemDescriptor& desc) {
                if (desc.owner_tag.has_value() && *desc.owner_tag == tag) {
                    ++removed;
                    return true;
                }
                return false;
            });
        systems.erase(it, systems.end());
    }

    // Invalidate the dependency graph cache since systems changed.
    m_graph_dirty = true;

    return removed;
}
```

- [ ] **Step 4: Add graph invalidation flag**

Add `bool m_graph_dirty = true;` to the Scheduler private members. Set it to `true` when systems are added or removed. Check it at the start of `run()` and rebuild the DAG if dirty.

```cpp
void Scheduler::run(World& world, Schedule schedule) {
    auto& systems = m_systems[schedule];

    if (m_graph_dirty) {
        rebuild_dependency_graph();
        m_graph_dirty = false;
    }

    // ... existing parallel dispatch logic ...
}
```

---

### Task 9: Full state lifecycle integration -- process_state_transitions system

**Files:**
- Modify: `helios-core/src/app/game_flow_plugin.h`

This task finalizes the frame-level integration. The `process_state_transitions` system is the single point where state changes take effect. All state operations (push, pop, go_to, switch_to) are deferred to this system.

- [ ] **Step 1: Refine the system to handle World/Scheduler access patterns**

The system needs mutable access to both World and Scheduler. Since the Scheduler is owned by App (not stored as a World resource), we need an approach. Two options:

Option A: Store a `Scheduler*` inside `GameFlow` at plugin build time.
Option B: Make the state management system a special "exclusive system" that receives `App&`.

We go with **Option A** for simplicity:

```cpp
template<typename StateEnum>
class GameFlow {
public:
    // ... existing ...

    /// Called by GameFlowPlugin::build() to give GameFlow a reference
    /// to the scheduler for dynamic system registration.
    void bind_scheduler(Scheduler& scheduler) {
        m_scheduler = &scheduler;
    }

    /// Apply pending operations using the bound scheduler.
    void apply_pending(World& world) {
        assert(m_scheduler && "GameFlow::apply_pending() called before bind_scheduler()");
        apply_pending(world, *m_scheduler);
    }

private:
    Scheduler* m_scheduler = nullptr;
    // ... existing ...
};
```

- [ ] **Step 2: Simplify the process_state_transitions system**

```cpp
/// System registered at Schedule::PreUpdate.
/// Only needs ResMut<GameFlow<E>> and access to World (passed by scheduler).
template<typename StateEnum>
void process_state_transitions(ResMut<GameFlow<StateEnum>> flow, World& world) {
    if (flow->has_pending()) {
        flow->apply_pending(world);
    }
}
```

- [ ] **Step 3: Update GameFlowPlugin::build() to bind the scheduler**

```cpp
void build(App& app) {
    GameFlow<StateEnum> flow;

    for (auto& reg_fn : m_registrations) {
        reg_fn(flow);
    }

    // Bind the scheduler so GameFlow can add/remove systems dynamically.
    flow.bind_scheduler(app.scheduler());

    // Transfer transitions.
    for (auto& t : m_transitions) {
        flow.m_transitions.push_back(std::move(t));
    }

    app.insert_resource<GameFlow<StateEnum>>(std::move(flow));

    // Register the transition processing system.
    app.add_system(Schedule::PreUpdate, process_state_transitions<StateEnum>);

    // Queue the initial state push if set.
    if (m_initial_push) {
        auto& flow_ref = app.world().resource<GameFlow<StateEnum>>();
        flow_ref.bind_scheduler(app.scheduler());
        m_initial_push(flow_ref);
    }
}
```

- [ ] **Step 4: Add App::scheduler() accessor**

In `app.h` (from Plan 2), add:

```cpp
class App {
public:
    // ... existing ...

    /// Access the scheduler (for plugin setup and state management).
    Scheduler& scheduler() { return m_scheduler; }
    const Scheduler& scheduler() const { return m_scheduler; }
};
```

---

### Task 10: Update CMakeLists.txt

**Files:**
- Modify: `helios-core/CMakeLists.txt`

- [ ] **Step 1: Add new source files to the helios-core build**

```cmake
# In helios-core/CMakeLists.txt, add to the sources list:
target_sources(helios-core PRIVATE
    # ... existing files ...
    src/app/state.h
    src/app/state.cpp
    src/app/state_builder.h
    src/app/game_flow.h
    src/app/game_flow.cpp
    src/app/game_flow_plugin.h
    src/app/transition.h
)
```

- [ ] **Step 2: Add test source files**

```cmake
# In helios-core/CMakeLists.txt (test target section):
target_sources(helios-core-tests PRIVATE
    # ... existing test files ...
    tests/test_state.cpp
    tests/test_game_flow.cpp
    tests/test_transitions.cpp
)
```

- [ ] **Step 3: Verify the project configures and builds**

```bash
cd helios-core && cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

---

### Task 11: Tests -- state stack and modifier behavior

**Files:**
- Create: `helios-core/tests/test_state.cpp`
- Create: `helios-core/tests/test_game_flow.cpp`

- [ ] **Step 1: Write test_state.cpp -- entity tracking and auto-despawn**

```cpp
// helios-core/tests/test_state.cpp
#include <gtest/gtest.h>
#include "app/state.h"
#include "app/state_builder.h"
#include "ecs/world.h"

using namespace helios;

// ── Test fixtures ──────────────────────────────────────────────────────

enum class TestState { A, B, C };

struct PositionComponent {
    float x = 0.0f, y = 0.0f;
};

// A simple state that spawns two tracked entities.
class StateA : public State<TestState> {
public:
    StateA(World& world) {
        m_entity1 = spawn_tracked(world);
        world.add(m_entity1, PositionComponent{1.0f, 2.0f});

        m_entity2 = spawn_tracked(world);
        world.add(m_entity2, PositionComponent{3.0f, 4.0f});
    }

    Entity entity1() const { return m_entity1; }
    Entity entity2() const { return m_entity2; }

    static void describe(StateBuilder<StateA>& s) {
        s.opaque();
    }

private:
    Entity m_entity1;
    Entity m_entity2;
};

// ── Tests ──────────────────────────────────────────────────────────────

TEST(StateTest, SpawnTrackedCreatesEntities) {
    World world;
    StateA state(world);

    EXPECT_TRUE(world.is_alive(state.entity1()));
    EXPECT_TRUE(world.is_alive(state.entity2()));

    auto& pos = world.get<PositionComponent>(state.entity1());
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
}

TEST(StateTest, DespawnTrackedRemovesAllEntities) {
    World world;

    {
        StateA state(world);
        Entity e1 = state.entity1();
        Entity e2 = state.entity2();

        EXPECT_TRUE(world.is_alive(e1));
        EXPECT_TRUE(world.is_alive(e2));

        // Explicitly despawn tracked entities (normally called by GameFlow).
        state.despawn_tracked(world);

        EXPECT_FALSE(world.is_alive(e1));
        EXPECT_FALSE(world.is_alive(e2));
    }
}

TEST(StateTest, TrackedEntitiesListIsAccurate) {
    World world;
    StateA state(world);

    const auto& tracked = state.tracked_entities();
    EXPECT_EQ(tracked.size(), 2u);
    EXPECT_EQ(tracked[0], state.entity1());
    EXPECT_EQ(tracked[1], state.entity2());
}

TEST(StateTest, DespawnTrackedHandlesAlreadyDeadEntities) {
    World world;
    StateA state(world);

    // Manually despawn one entity before calling despawn_tracked.
    world.despawn(state.entity1());
    EXPECT_FALSE(world.is_alive(state.entity1()));

    // Should not crash -- skips dead entities.
    EXPECT_NO_THROW(state.despawn_tracked(world));
    EXPECT_FALSE(world.is_alive(state.entity2()));
}

TEST(StateTest, ModifierDefaultsToOpaque) {
    World world;
    StateA state(world);
    EXPECT_EQ(state.modifier(), StateModifier::Opaque);
}
```

- [ ] **Step 2: Write test_game_flow.cpp -- stack operations and modifiers**

```cpp
// helios-core/tests/test_game_flow.cpp
#include <gtest/gtest.h>
#include "app/game_flow.h"
#include "app/game_flow_plugin.h"
#include "app/state.h"
#include "app/state_builder.h"
#include "ecs/world.h"
#include "ecs/scheduler.h"

using namespace helios;

// ── Test state enum and states ─────────────────────────────────────────

enum class GS { Menu, Playing, Paused, Overlay };

static int g_menu_system_calls = 0;
static int g_playing_system_calls = 0;
static int g_paused_system_calls = 0;
static int g_overlay_system_calls = 0;

class MenuState : public State<GS> {
public:
    MenuState(World& world) {
        m_entity = spawn_tracked(world);
    }
    void menu_system() { ++g_menu_system_calls; }
    Entity entity() const { return m_entity; }
    static void describe(StateBuilder<MenuState>& s) {
        s.opaque();
        s.system(&MenuState::menu_system);
    }
private:
    Entity m_entity;
};

class PlayingState : public State<GS> {
public:
    PlayingState(World& world) {
        m_entity = spawn_tracked(world);
    }
    void playing_system() { ++g_playing_system_calls; }
    Entity entity() const { return m_entity; }
    static void describe(StateBuilder<PlayingState>& s) {
        s.opaque();
        s.system(&PlayingState::playing_system);
    }
private:
    Entity m_entity;
};

class PausedState : public State<GS> {
public:
    PausedState(World& world) {}
    void paused_system() { ++g_paused_system_calls; }
    static void describe(StateBuilder<PausedState>& s) {
        s.pause_below();
        s.system(&PausedState::paused_system);
    }
};

class OverlayState : public State<GS> {
public:
    OverlayState(World& world) {}
    void overlay_system() { ++g_overlay_system_calls; }
    static void describe(StateBuilder<OverlayState>& s) {
        s.transparent();
        s.system(&OverlayState::overlay_system);
    }
};

class GameFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_menu_system_calls = 0;
        g_playing_system_calls = 0;
        g_paused_system_calls = 0;
        g_overlay_system_calls = 0;

        flow.register_state<MenuState>(GS::Menu);
        flow.register_state<PlayingState>(GS::Playing);
        flow.register_state<PausedState>(GS::Paused);
        flow.register_state<OverlayState>(GS::Overlay);
        flow.bind_scheduler(scheduler);
    }

    World world;
    Scheduler scheduler;
    GameFlow<GS> flow;
};

// ── Push / Pop ─────────────────────────────────────────────────────────

TEST_F(GameFlowTest, PushIncreasesStackDepth) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Menu);
    EXPECT_TRUE(flow.is_in(GS::Menu));
}

TEST_F(GameFlowTest, PopDecreasesStackDepth) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);
    EXPECT_EQ(flow.depth(), 2u);

    flow.pop();
    flow.apply_pending(world, scheduler);
    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Menu);
}

TEST_F(GameFlowTest, PopOnEmptyStackIsNoOp) {
    flow.pop();
    EXPECT_NO_THROW(flow.apply_pending(world, scheduler));
    EXPECT_TRUE(flow.is_empty());
}

// ── go_to ──────────────────────────────────────────────────────────────

TEST_F(GameFlowTest, GoToClearsStackAndPushesNewState) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    flow.go_to<PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Playing);
    EXPECT_FALSE(flow.is_in(GS::Menu));
}

TEST_F(GameFlowTest, GoToMultipleStates) {
    flow.go_to<MenuState, PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 2u);
    EXPECT_EQ(flow.current(), GS::Playing);  // rightmost = top
    EXPECT_TRUE(flow.is_in(GS::Menu));
}

// ── switch_to ──────────────────────────────────────────────────────────

TEST_F(GameFlowTest, SwitchToReplacesTopState) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    flow.switch_to<PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), GS::Playing);
    EXPECT_FALSE(flow.is_in(GS::Menu));
}

// ── Entity auto-despawn on state exit ──────────────────────────────────

TEST_F(GameFlowTest, EntitiesDespawnedWhenStatePopped) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    // Get the entity the state created.
    // We need to inspect the stack to get the state's entity.
    // For testing, we check that entities exist then don't after pop.
    size_t entity_count_before = world.query<PositionComponent>().count();

    flow.pop();
    flow.apply_pending(world, scheduler);

    // After pop, the PlayingState's tracked entities should be despawned.
    // (PlayingState spawns one tracked entity.)
    EXPECT_TRUE(flow.is_empty());
}

TEST_F(GameFlowTest, EntitiesDespawnedWhenGoToClearsStack) {
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    // MenuState spawns one tracked entity.
    EXPECT_FALSE(flow.is_empty());

    flow.go_to<PlayingState>();
    flow.apply_pending(world, scheduler);

    // MenuState's tracked entity should be despawned.
    EXPECT_EQ(flow.depth(), 1u);
}

// ── Modifier behavior: opaque ──────────────────────────────────────────

TEST_F(GameFlowTest, OpaqueStateStopsSystemsBelow) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    // Push opaque Menu on top of Playing.
    flow.push<MenuState>();
    flow.apply_pending(world, scheduler);

    // Only MenuState's systems should be active.
    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 1u);  // only MenuState (opaque blocks below)
}

// ── Modifier behavior: transparent ─────────────────────────────────────

TEST_F(GameFlowTest, TransparentStateKeepsSystemsBelow) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    // Push transparent Overlay on top.
    flow.push<OverlayState>();
    flow.apply_pending(world, scheduler);

    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 2u);  // Both OverlayState and PlayingState active.
}

// ── Modifier behavior: pause_below ─────────────────────────────────────

TEST_F(GameFlowTest, PauseBelowStopsSystemsBelowButEntitiesVisible) {
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    flow.push<PausedState>();
    flow.apply_pending(world, scheduler);

    // PausedState has pause_below: only PausedState's systems should run.
    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 1u);  // only PausedState

    // But PlayingState's tracked entities should still be alive (visible).
    // They are NOT despawned (unlike opaque).
    EXPECT_EQ(flow.depth(), 2u);  // both states still on stack
}

// ── Multiple operations in one frame ───────────────────────────────────

TEST_F(GameFlowTest, MultipleOpsAppliedInOrder) {
    flow.push<MenuState>();
    flow.push<PlayingState>();
    flow.apply_pending(world, scheduler);

    EXPECT_EQ(flow.depth(), 2u);
    EXPECT_EQ(flow.current(), GS::Playing);

    flow.pop();
    flow.pop();
    flow.apply_pending(world, scheduler);

    EXPECT_TRUE(flow.is_empty());
}
```

- [ ] **Step 3: Verify tests compile and pass**

```bash
cd helios-core && cmake --build build --target helios-core-tests && ./build/helios-core-tests --gtest_filter="State*:GameFlow*"
```

---

### Task 12: Tests -- transition chains

**Files:**
- Create: `helios-core/tests/test_transitions.cpp`

- [ ] **Step 1: Write test_transitions.cpp**

```cpp
// helios-core/tests/test_transitions.cpp
#include <gtest/gtest.h>
#include "app/game_flow.h"
#include "app/game_flow_plugin.h"
#include "app/state.h"
#include "app/state_builder.h"
#include "app/transition.h"
#include "ecs/world.h"
#include "ecs/scheduler.h"

using namespace helios;

// ── Test state enum and states ─────────────────────────────────────────

enum class TS { Menu, Loading, Playing };

static bool g_loading_constructed = false;
static bool g_loading_destructed = false;
static bool g_playing_constructed = false;

class TMenuState : public State<TS> {
public:
    TMenuState(World& world) {}
    static void describe(StateBuilder<TMenuState>& s) { s.opaque(); }
};

class TLoadingState : public State<TS> {
public:
    TLoadingState(World& world) {
        g_loading_constructed = true;
        m_entity = spawn_tracked(world);
    }
    ~TLoadingState() override {
        g_loading_destructed = true;
    }

    // Simulates loading completion: calls pop() to proceed.
    void check_done(ResMut<GameFlow<TS>> flow) {
        flow->pop();
    }

    Entity entity() const { return m_entity; }

    static void describe(StateBuilder<TLoadingState>& s) {
        s.opaque();
        s.system(&TLoadingState::check_done);
    }
private:
    Entity m_entity;
};

class TPlayingState : public State<TS> {
public:
    TPlayingState(World& world) {
        g_playing_constructed = true;
    }
    static void describe(StateBuilder<TPlayingState>& s) { s.opaque(); }
};

class TransitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_loading_constructed = false;
        g_loading_destructed = false;
        g_playing_constructed = false;

        flow.register_state<TMenuState>(TS::Menu);
        flow.register_state<TLoadingState>(TS::Loading);
        flow.register_state<TPlayingState>(TS::Playing);
        flow.bind_scheduler(scheduler);
    }

    World world;
    Scheduler scheduler;
    GameFlow<TS> flow;
};

// ── Transition via intermediate state ──────────────────────────────────

TEST_F(TransitionTest, TransitionViaIntermediateState) {
    // Register transition: Menu -> Playing goes via Loading.
    flow.transition(TS::Menu, TS::Playing)
        .via<TLoadingState>();

    // Start in Menu.
    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);
    EXPECT_EQ(flow.current(), TS::Menu);

    // Request go_to Playing. The transition system should detect the
    // Menu->Playing chain and insert Loading.
    flow.go_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    // Stack should be: [Playing, Loading]  (Loading on top).
    EXPECT_TRUE(g_loading_constructed);
    EXPECT_TRUE(g_playing_constructed);
    EXPECT_EQ(flow.depth(), 2u);
    EXPECT_EQ(flow.current(), TS::Loading);
    EXPECT_TRUE(flow.is_in(TS::Playing));

    // Loading is opaque, so Playing's systems should not be active.
    auto active = flow.active_states();
    EXPECT_EQ(active.size(), 1u);
}

TEST_F(TransitionTest, IntermediateStatePopRevealsTarget) {
    flow.transition(TS::Menu, TS::Playing)
        .via<TLoadingState>();

    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);

    flow.go_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    // Stack: [Playing, Loading]
    // Simulate loading done: pop Loading.
    flow.pop();
    flow.apply_pending(world, scheduler);

    // Loading should be destructed, its tracked entity despawned.
    EXPECT_TRUE(g_loading_destructed);
    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), TS::Playing);
}

TEST_F(TransitionTest, DirectTransitionWithoutChain) {
    // No transition chain registered for Menu -> Playing via this path.
    // (We don't register a .via() here.)
    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);

    flow.switch_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    // Should go directly to Playing, no Loading.
    EXPECT_FALSE(g_loading_constructed);
    EXPECT_EQ(flow.depth(), 1u);
    EXPECT_EQ(flow.current(), TS::Playing);
}

TEST_F(TransitionTest, LoadingEntityDespawnedOnPop) {
    flow.transition(TS::Menu, TS::Playing)
        .via<TLoadingState>();

    flow.push<TMenuState>();
    flow.apply_pending(world, scheduler);

    flow.go_to<TPlayingState>();
    flow.apply_pending(world, scheduler);

    // Loading state's tracked entity should be alive.
    // (We can't easily access it without stack inspection, but we verify
    //  it's despawned after pop by checking world entity count.)

    flow.pop();  // pop Loading
    flow.apply_pending(world, scheduler);

    // Loading's tracked entities should be despawned.
    EXPECT_TRUE(g_loading_destructed);
    EXPECT_EQ(flow.depth(), 1u);
}
```

- [ ] **Step 2: Verify transition tests compile and pass**

```bash
cd helios-core && cmake --build build --target helios-core-tests && ./build/helios-core-tests --gtest_filter="Transition*"
```

- [ ] **Step 3: Run full test suite to ensure no regressions**

```bash
cd helios-core && cmake --build build --target helios-core-tests && ./build/helios-core-tests
```

---

## Summary of files created/modified

| File | Action | Purpose |
|------|--------|---------|
| `helios-core/src/app/state.h` | Create | `StateModifier` enum, `StateBase`, `State<E>` base class |
| `helios-core/src/app/state.cpp` | Create | `StateBase` method implementations |
| `helios-core/src/app/state_builder.h` | Create | `StateBuilder<S>`, `StateSystemDescriptor`, member-system invocation |
| `helios-core/src/app/game_flow.h` | Create | `GameFlow<E>` state stack, `StateEntry`, `StateRegistration`, pending ops |
| `helios-core/src/app/game_flow.cpp` | Create | Template instantiation placeholder |
| `helios-core/src/app/game_flow_plugin.h` | Create | `GameFlowPlugin<E>`, `process_state_transitions` system |
| `helios-core/src/app/transition.h` | Create | `TransitionDef<E>`, `TransitionBuilder<E>` |
| `helios-core/src/ecs/scheduler.h` | Modify | Add `owner_tag`, `remove_systems_by_tag()`, `add_system(Schedule, SystemDescriptor)` |
| `helios-core/src/ecs/scheduler.cpp` | Modify | Implement `remove_systems_by_tag()`, graph invalidation |
| `helios-core/src/app/app.h` | Modify | Add `scheduler()` accessor, `transition()` convenience |
| `helios-core/CMakeLists.txt` | Modify | Add new sources and test files |
| `helios-core/tests/test_state.cpp` | Create | Entity tracking and auto-despawn tests |
| `helios-core/tests/test_game_flow.cpp` | Create | Stack operations, modifier behavior tests |
| `helios-core/tests/test_transitions.cpp` | Create | Transition chain tests |

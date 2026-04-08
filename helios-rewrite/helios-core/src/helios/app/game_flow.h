// helios-core/src/helios/app/game_flow.h
#pragma once

#include "helios/app/state.h"
#include "helios/app/state_builder.h"
#include "helios/app/transition.h"
#include "helios/ecs/scheduler.h"
#include "helios/ecs/system_descriptor.h"

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

/// Marker component added to entities that should be hidden from rendering
/// because an opaque state is stacked above their owning state.
struct Hidden {};

/// Metadata for a registered state type. Created once when the user
/// registers a state via GameFlowPlugin or GameFlow::register_state<S>().
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

// -- Pending operations (queued, applied during transition system) --

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

    // -- Stack operations (queue a pending operation) --

    /// Clear the entire stack and push one or more new states.
    /// The states are pushed left-to-right: the rightmost ends up on top.
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

    // -- Queries --

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

    // -- Registration (called by GameFlowPlugin / App during setup) --

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

    // -- Transition registration --

    /// Define a transition chain from one state to another.
    TransitionBuilder<StateEnum> transition(StateEnum from, StateEnum to) {
        return TransitionBuilder<StateEnum>(from, to, m_transitions);
    }

    // -- Scheduler binding --

    /// Called by GameFlowPlugin::build() to give GameFlow a reference
    /// to the scheduler for dynamic system registration.
    void bind_scheduler(Scheduler& scheduler) {
        m_scheduler = &scheduler;
    }

    // -- Transition processing (called by the state management system) --

    /// Returns true if there are pending operations to process.
    bool has_pending() const {
        return !m_pending.empty();
    }

    /// Apply all pending operations using the bound scheduler.
    void apply_pending(World& world) {
        assert(m_scheduler && "GameFlow::apply_pending() called before bind_scheduler()");
        apply_pending(world, *m_scheduler);
    }

    /// Apply all pending operations. This is called once per frame by
    /// the state management system (see GameFlowPlugin).
    void apply_pending(World& world, Scheduler& scheduler);

    // -- Active systems query (used by Scheduler integration) --

    /// Computes which states on the stack are "active" (should have their
    /// systems running) based on stack modifiers.
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

    /// Look up whether a transition chain exists for (from, to).
    const TransitionDef<StateEnum>* find_transition(StateEnum from, StateEnum to) const {
        for (const auto& def : m_transitions) {
            if (def.from == from && def.to == to) {
                return &def;
            }
        }
        return nullptr;
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

    /// Hide all tracked entities of states below the given stack index.
    void hide_entities_below(size_t index, World& world);

    /// Unhide all tracked entities of states below the given stack index.
    void unhide_entities_below(size_t index, World& world);

    void mark_tracked_hidden(StateEntry<StateEnum>& entry, World& world, bool hide);

    std::vector<StateEntry<StateEnum>> m_stack;
    std::unordered_map<std::type_index, StateRegistration<StateEnum>> m_registrations;
    std::vector<detail::PendingOp<StateEnum>> m_pending;
    std::vector<TransitionDef<StateEnum>> m_transitions;
    Scheduler* m_scheduler = nullptr;

    /// Tracks which state entries currently have their systems registered
    /// in the scheduler, so we can diff on state changes.
    std::vector<std::type_index> m_active_system_types;

    // GameFlowPlugin needs access to m_transitions for bulk transfer.
    template<typename E> friend class GameFlowPlugin;
};

// ============================================================================
// Template method implementations
// ============================================================================

template<typename StateEnum>
void GameFlow<StateEnum>::push_state(
    const StateRegistration<StateEnum>& reg,
    World& world
) {
    StateEntry<StateEnum> entry;
    entry.registration = &m_registrations.at(reg.type);

    // Construct the state (RAII enter).
    auto instance = reg.factory(world);
    entry.instance = std::move(instance);

    m_stack.push_back(std::move(entry));
}

template<typename StateEnum>
void GameFlow<StateEnum>::pop_state(World& world) {
    assert(!m_stack.empty() && "GameFlow::pop_state() called on empty stack");

    // Despawn all entities tracked by this state.
    m_stack.back().instance->despawn_tracked(world);

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
        SystemDescriptor sd;
        sd.name = sys_desc.name;
        sd.accesses = sys_desc.accesses;

        // Capture raw pointer to the state instance. This is safe because
        // the state is alive on the stack for as long as its systems are
        // registered. We tag the SystemDescriptor with the state's type
        // so we can remove it later.
        StateBase* raw = entry.instance.get();
        sd.run = [raw, invoke = sys_desc.invoke](World& world) {
            invoke(raw, world);
        };
        sd.owner_tag = reg.type;

        scheduler.add_system(sys_desc.schedule, std::move(sd));
    }
}

template<typename StateEnum>
void GameFlow<StateEnum>::deactivate_state_systems(
    const StateEntry<StateEnum>& entry,
    Scheduler& scheduler
) {
    scheduler.remove_systems_by_tag(entry.registration->type);
}

template<typename StateEnum>
void GameFlow<StateEnum>::sync_scheduler(Scheduler& scheduler) {
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
            // If the entry was already popped, its systems were already removed
            // during the pop operation. Just skip.
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
void GameFlow<StateEnum>::hide_entities_below(size_t index, World& world) {
    for (size_t i = 0; i < index; ++i) {
        mark_tracked_hidden(m_stack[i], world, true);
    }
}

template<typename StateEnum>
void GameFlow<StateEnum>::unhide_entities_below(size_t index, World& world) {
    for (size_t i = 0; i < index; ++i) {
        mark_tracked_hidden(m_stack[i], world, false);
    }
}

template<typename StateEnum>
void GameFlow<StateEnum>::mark_tracked_hidden(
    StateEntry<StateEnum>& entry, World& world, bool hide
) {
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

template<typename StateEnum>
void GameFlow<StateEnum>::apply_pending(World& world, Scheduler& scheduler) {
    if (m_pending.empty()) return;

    // Process all pending operations in order.
    for (auto& op : m_pending) {
        std::visit([&](auto& concrete_op) {
            using T = std::decay_t<decltype(concrete_op)>;

            if constexpr (std::is_same_v<T, detail::OpGoTo<StateEnum>>) {
                // Capture the current top state id before clearing, for transition lookup.
                std::optional<StateEnum> from_id;
                if (!m_stack.empty()) {
                    from_id = m_stack.back().id();
                }

                // Clear entire stack.
                while (!m_stack.empty()) {
                    deactivate_state_systems(m_stack.back(), scheduler);
                    pop_state(world);
                }
                m_active_system_types.clear();

                // Check for transition chains (single target only).
                if (from_id.has_value() && concrete_op.state_types.size() == 1) {
                    StateEnum target_id = find_registration(concrete_op.state_types[0]).id;
                    const auto* chain = find_transition(*from_id, target_id);
                    if (chain) {
                        // Push target first (bottom), then intermediates on top.
                        const auto& target_reg = find_registration(concrete_op.state_types[0]);
                        push_state(target_reg, world);
                        for (const auto& via_type : chain->via_types) {
                            const auto& via_reg = find_registration(via_type);
                            push_state(via_reg, world);
                        }
                        return; // skip the normal push below
                    }
                }

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

    // Handle opaque modifier entity visibility.
    if (!m_stack.empty()) {
        const auto& top = m_stack.back();
        if (top.modifier() == StateModifier::Opaque) {
            hide_entities_below(m_stack.size() - 1, world);
        } else {
            unhide_entities_below(m_stack.size(), world);
        }
    }
}

} // namespace helios

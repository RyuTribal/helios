// helios-core/src/helios/app/state_builder.h
#pragma once

#include "helios/app/state.h"
#include "helios/ecs/access_descriptor.h"
#include "helios/ecs/commands.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/system_param_traits.h"

#include <functional>
#include <string>
#include <typeindex>
#include <vector>

namespace helios {

// Forward declarations
class World;

/// Descriptor for one system method belonging to a state.
struct StateSystemDescriptor {
    /// Type-erased function that, given a StateBase* (downcast to S*),
    /// a World&, and the system's last_run_tick, invokes the member function
    /// with its declared parameters extracted from the World.
    std::function<void(StateBase*, World&, uint32_t)> invoke;

    /// The schedule this system runs in (default: Update).
    Schedule schedule = Schedule::Update;

    /// Human-readable name for debugging (e.g., "Playing::player_movement").
    std::string name;

    /// Access descriptors extracted from the member function's parameters.
    /// Used by the Scheduler for dependency-graph construction.
    std::vector<AccessDescriptor> accesses;
};

// ============================================================================
// Member-function system invocation and access extraction
// ============================================================================

namespace detail {

/// Invoke a member function of S with parameters extracted from World.
/// Uses the existing SystemParam<T>::fetch(World&, uint32_t) infrastructure.
template<typename S, typename Ret, typename... Params>
void invoke_member_system_impl(S* obj, Ret(S::*fn)(Params...), World& world, uint32_t last_run_tick) {
    (obj->*fn)(SystemParam<std::decay_t<Params>>::fetch(world, last_run_tick)...);
}

/// Overload for member functions with no parameters.
template<typename S, typename Ret>
void invoke_member_system_impl(S* obj, Ret(S::*fn)(), World&, uint32_t) {
    (obj->*fn)();
}

/// Extract access descriptors from a member function's parameter types.
/// Uses the same SystemParam<T>::accesses() infrastructure as free-function systems.
template<typename... Params>
std::vector<AccessDescriptor> collect_member_accesses() {
    std::vector<AccessDescriptor> result;
    (([&] {
        auto a = SystemParam<std::decay_t<Params>>::accesses();
        result.insert(result.end(), a.begin(), a.end());
    }()), ...);
    return result;
}

/// Tag type for extracting access from a member function pointer.
template<typename S, typename Ret, typename... Params>
std::vector<AccessDescriptor> extract_member_accesses(Ret(S::*)(Params...)) {
    return collect_member_accesses<Params...>();
}

/// Overload for member functions with no parameters.
template<typename S, typename Ret>
std::vector<AccessDescriptor> extract_member_accesses(Ret(S::*)()) {
    return {};
}

} // namespace detail

/// Collects configuration for a concrete state S during S::describe().
/// S must derive from State<SomeEnum>.
template<typename S>
class StateBuilder {
public:
    StateBuilder() = default;

    // -- Modifiers --

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

    // -- System registration --

    /// Register a member function of S as a system for this state.
    ///
    /// Usage:
    ///   s.system(&Playing::player_movement);
    ///   s.system(&Playing::check_pause);
    ///
    /// The member function's parameters are resolved from the World just
    /// like free-function systems (Query<>, Res<>, ResMut<>, Commands&, etc.).
    template<typename Ret, typename... Params>
    StateBuilder& system(Ret(S::*fn)(Params...)) {
        StateSystemDescriptor desc;
        desc.name = std::string(typeid(S).name()) + "::<member>";
        desc.schedule = Schedule::Update;

        // Build the type-erased invoker.
        desc.invoke = [fn](StateBase* base, World& world, uint32_t last_run_tick) {
            S* self = static_cast<S*>(base);
            detail::invoke_member_system_impl(self, fn, world, last_run_tick);
        };

        // Extract access metadata from the member function's parameter types.
        desc.accesses = detail::extract_member_accesses(fn);

        m_systems.push_back(std::move(desc));
        return *this;
    }

    /// Register a member function on a specific schedule (not Update).
    ///
    /// Usage:
    ///   s.system(&Playing::player_movement, Schedule::FixedUpdate);
    template<typename Ret, typename... Params>
    StateBuilder& system(Ret(S::*fn)(Params...), Schedule sched) {
        system(fn);
        m_systems.back().schedule = sched;
        return *this;
    }

    // -- Accessors (read by GameFlow) --

    StateModifier get_modifier() const { return m_modifier; }
    const std::vector<StateSystemDescriptor>& get_systems() const { return m_systems; }

private:
    StateModifier m_modifier = StateModifier::Opaque;
    std::vector<StateSystemDescriptor> m_systems;
};

} // namespace helios

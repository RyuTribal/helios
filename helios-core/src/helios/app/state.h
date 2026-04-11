#pragma once

#include "helios/ecs/entity.h"
#include "helios/ecs/world.h"

#include <cassert>
#include <typeindex>
#include <vector>

namespace helios {

// Forward declarations
class World;
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

    /// Returns the list of entities tracked by this state.
    /// Used by GameFlow for opaque-state entity hiding.
    const std::vector<Entity>& tracked_entities() const {
        return m_tracked_entities;
    }

protected:
    /// Spawn an entity owned by this state. It will be automatically
    /// despawned when the state exits (destructor or explicit pop).
    Entity spawn_tracked(World& world);

private:
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

} // namespace helios

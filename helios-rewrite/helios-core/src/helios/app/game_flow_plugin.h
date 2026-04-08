// helios-core/src/helios/app/game_flow_plugin.h
#pragma once

#include "helios/app/game_flow.h"
#include "helios/app/state.h"
#include "helios/app/state_builder.h"
#include "helios/app/transition.h"
#include "helios/ecs/app.h"
#include "helios/ecs/scheduler.h"
#include "helios/ecs/system_params.h"
#include "helios/ecs/system_param_traits.h"

#include <functional>
#include <optional>
#include <typeindex>
#include <vector>

namespace helios {

/// System that processes pending state transitions.
/// Registered at Schedule::PreUpdate by GameFlowPlugin.
template<typename StateEnum>
void process_state_transitions(ResMut<GameFlow<StateEnum>> flow) {
    if (flow->has_pending()) {
        flow->apply_pending();
    }
}

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
        m_initial_push = [](GameFlow<StateEnum>& flow) {
            flow.template go_to<S>();
        };
        return *this;
    }

    /// Define a transition chain (forwarded to GameFlow).
    TransitionBuilder<StateEnum> transition(StateEnum from, StateEnum to) {
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

        // 3. Bind the scheduler and world so GameFlow can add/remove systems dynamically.
        flow.bind_scheduler(app.scheduler());
        flow.bind_world(app.world());

        // 4. Transfer transition definitions.
        flow.m_transitions = std::move(m_transitions);

        // 5. Insert the resource into the World.
        app.insert_resource<GameFlow<StateEnum>>(std::move(flow));

        // 6. Register the transition processing system.
        app.add_system(Schedule::PreUpdate,
                       process_state_transitions<StateEnum>,
                       "process_state_transitions");

        // 7. Queue the initial state push if set.
        if (m_initial_push) {
            auto& flow_ref = app.world().resource<GameFlow<StateEnum>>();
            // Re-bind scheduler and world on the moved-into resource.
            flow_ref.bind_scheduler(app.scheduler());
            flow_ref.bind_world(app.world());
            m_initial_push(flow_ref);
        }
    }

private:
    std::vector<std::function<void(GameFlow<StateEnum>&)>> m_registrations;
    std::function<void(GameFlow<StateEnum>&)> m_initial_push;
    std::vector<TransitionDef<StateEnum>> m_transitions;
};

} // namespace helios

#pragma once

#include <typeindex>
#include <vector>

namespace helios {

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

/// Describes a transition chain: From -> [Via...] -> To.
/// When GameFlow detects a go_to that matches (From, To),
/// it inserts the intermediate states.
template<typename StateEnum>
struct TransitionDef {
    StateEnum from;
    StateEnum to;
    /// Intermediate state types to push between From and To.
    /// The chain is: pop From, push To (bottom), push Via[0]...Via[N] (top).
    /// Each Via state pops itself when done. When the last Via pops,
    /// To becomes the active top.
    std::vector<std::type_index> via_types;
};

/// Builder returned by GameFlow::transition(from, to).
/// Allows chaining .via<LoadingScreen>().
template<typename StateEnum>
class TransitionBuilder {
public:
    TransitionBuilder(StateEnum from, StateEnum to,
                      std::vector<TransitionDef<StateEnum>>& registry)
        : m_from(from), m_to(to), m_registry(registry) {}

    /// Add an intermediate state to the transition chain.
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

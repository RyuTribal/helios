// helios-core/src/helios/ecs/system_set.h
#pragma once

#include "helios/ecs/system_descriptor.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace helios {

/// Wraps a system function with ordering constraints, allowing ergonomic
/// chaining before passing to App::add_system().
///
/// Usage:
///   app.add_system(Schedule::Update, sys(my_system).after(id_a).before(id_b));
///
/// When passed to add_system, the Scheduler detects the SystemSet wrapper
/// and applies the constraints to the resulting SystemDescriptor.
template <typename F>
class SystemSet {
public:
    explicit SystemSet(F fn) : m_fn(std::move(fn)) {}

    SystemSet& after(SystemId id) {
        m_after.push_back(id);
        return *this;
    }

    SystemSet& before(SystemId id) {
        m_before.push_back(id);
        return *this;
    }

    const F& function() const { return m_fn; }
    const std::vector<SystemId>& after_ids()  const { return m_after; }
    const std::vector<SystemId>& before_ids() const { return m_before; }

private:
    F                      m_fn;
    std::vector<SystemId>  m_after;
    std::vector<SystemId>  m_before;
};

/// Factory function for ergonomic usage: sys(my_function).after(id)
template <typename F>
SystemSet<std::decay_t<F>> sys(F&& fn) {
    return SystemSet<std::decay_t<F>>(std::forward<F>(fn));
}

// Type trait to detect SystemSet<F>
template <typename T>
struct is_system_set : std::false_type {};

template <typename F>
struct is_system_set<SystemSet<F>> : std::true_type {};

template <typename T>
inline constexpr bool is_system_set_v = is_system_set<T>::value;

} // namespace helios

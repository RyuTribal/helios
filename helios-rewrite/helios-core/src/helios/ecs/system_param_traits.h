// helios-core/src/helios/ecs/system_param_traits.h
#pragma once

#include "helios/ecs/access_descriptor.h"
#include "helios/ecs/query_filters.h"

#include <functional>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace helios {

// Forward declarations from Plan 1
class World;
class Commands;
template <typename... T> class Query;
template <typename T> class Res;
template <typename T> class ResMut;
template <typename T> class EventReader;
template <typename T> class EventWriter;

// ============================================================================
// SystemParam trait: each system parameter type specializes this.
// ============================================================================

template <typename T>
struct SystemParam; // primary template -- left undefined, specializations below

// ----------------------------------------------------------------------------
// Query<Ts...>
// ----------------------------------------------------------------------------
// Helper: determine access mode for a single query component type.
//   const T  -> Read
//   T        -> Write
//   With<T>  -> Read  (filter only, but still accesses the type)
//   Without<T> -> Read
//   Optional<T> -> same as T (mutable or const)

namespace detail {

// Strip const from a type and determine access mode
template <typename T>
struct QueryComponentAccess {
    using type = T;
    static constexpr AccessMode mode = AccessMode::Write;
};

template <typename T>
struct QueryComponentAccess<const T> {
    using type = T;
    static constexpr AccessMode mode = AccessMode::Read;
};

template <typename T>
struct QueryComponentAccess<With<T>> {
    using type = T;
    static constexpr AccessMode mode = AccessMode::Read;
};

template <typename T>
struct QueryComponentAccess<Without<T>> {
    using type = T;
    static constexpr AccessMode mode = AccessMode::Read;
};

template <typename T>
struct QueryComponentAccess<Optional<T>> {
    using type = T;
    static constexpr AccessMode mode = AccessMode::Write;
};

template <typename T>
struct QueryComponentAccess<Optional<const T>> {
    using type = T;
    static constexpr AccessMode mode = AccessMode::Read;
};

// Collect accesses from a pack of query component types
template <typename... Ts>
std::vector<AccessDescriptor> query_accesses() {
    std::vector<AccessDescriptor> result;
    (result.push_back(AccessDescriptor{
        .type = std::type_index(typeid(typename QueryComponentAccess<Ts>::type)),
        .mode = QueryComponentAccess<Ts>::mode,
    }), ...);
    return result;
}

// ============================================================================
// Function trait: decompose a function pointer into return + argument types
// ============================================================================

// Free function
template <typename F>
struct FunctionTraits;

template <typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

// Function reference
template <typename R, typename... Args>
struct FunctionTraits<R(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

// Functor / lambda: delegate to operator()
template <typename F>
struct FunctionTraits : FunctionTraits<decltype(&std::decay_t<F>::operator())> {};

// const member function (lambdas)
template <typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...) const> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

// mutable member function
template <typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

} // namespace detail

// ============================================================================
// SystemParam specializations
// ============================================================================

// --- Query<Ts...> ---
template <typename... Ts>
struct SystemParam<Query<Ts...>> {
    static std::vector<AccessDescriptor> accesses() {
        return detail::query_accesses<Ts...>();
    }

    static Query<Ts...> fetch(World& world) {
        return world.query<Ts...>();
    }
};

// --- Res<T> (read-only resource) ---
template <typename T>
struct SystemParam<Res<T>> {
    static std::vector<AccessDescriptor> accesses() {
        return { AccessDescriptor{
            .type = std::type_index(typeid(T)),
            .mode = AccessMode::Read,
        } };
    }

    static Res<T> fetch(World& world) {
        return Res<T>(&world.resource<T>());
    }
};

// --- ResMut<T> (read-write resource) ---
template <typename T>
struct SystemParam<ResMut<T>> {
    static std::vector<AccessDescriptor> accesses() {
        return { AccessDescriptor{
            .type = std::type_index(typeid(T)),
            .mode = AccessMode::Write,
        } };
    }

    static ResMut<T> fetch(World& world) {
        return ResMut<T>(&world.resource<T>());
    }
};

// --- Commands (deferred -- no access conflicts) ---
template <>
struct SystemParam<Commands> {
    static std::vector<AccessDescriptor> accesses() {
        return {}; // Commands are deferred, no data race
    }

    static Commands fetch(World& world) {
        return Commands(world.entities());
    }
};

// --- Commands& (reference variant -- same as Commands) ---
// When systems take Commands& the decay_t gives Commands, which is handled above.

// --- EventReader<T> (shared read on event channel T) ---
// Uses typeid(T) so the scheduler can detect conflicts with EventWriter<T>
// on the same event type T (read-write conflict detection requires a shared key).
template <typename T>
struct SystemParam<EventReader<T>> {
    static std::vector<AccessDescriptor> accesses() {
        return { AccessDescriptor{
            .type = std::type_index(typeid(T)),
            .mode = AccessMode::Read,
        } };
    }

    static EventReader<T> fetch(World& world) {
        return world.event_reader<T>();
    }
};

// --- EventWriter<T> (exclusive write on event channel T) ---
// Uses typeid(T) so that two EventWriter<T> systems (write-write) and any
// EventReader<T> + EventWriter<T> pair (read-write) are correctly detected
// as conflicting by the DAG scheduler.
template <typename T>
struct SystemParam<EventWriter<T>> {
    static std::vector<AccessDescriptor> accesses() {
        return { AccessDescriptor{
            .type = std::type_index(typeid(T)),
            .mode = AccessMode::Write,
        } };
    }

    static EventWriter<T> fetch(World& world) {
        return world.event_writer<T>();
    }
};

// ============================================================================
// SystemParamExtractor: given a callable F, produce accesses + a type-erased
// std::function<void(World&)> that fetches params and invokes F.
// ============================================================================

namespace detail {

// Collect accesses from a tuple of parameter types
template <typename Tuple, size_t... Is>
std::vector<AccessDescriptor> collect_accesses_impl(std::index_sequence<Is...>) {
    std::vector<AccessDescriptor> result;
    (([&] {
        auto a = SystemParam<std::tuple_element_t<Is, Tuple>>::accesses();
        result.insert(result.end(), a.begin(), a.end());
    }()), ...);
    return result;
}

template <typename Tuple>
std::vector<AccessDescriptor> collect_accesses() {
    return collect_accesses_impl<Tuple>(
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Invoke F by fetching each parameter from World
template <typename F, typename Tuple, size_t... Is>
void invoke_system_impl(F& fn, World& world, std::index_sequence<Is...>) {
    fn(SystemParam<std::tuple_element_t<Is, Tuple>>::fetch(world)...);
}

template <typename F, typename Tuple>
void invoke_system(F& fn, World& world) {
    invoke_system_impl<F, Tuple>(
        fn, world,
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

} // namespace detail

template <typename F>
struct SystemParamExtractor {
    using Traits = detail::FunctionTraits<F>;
    using ArgsTuple = typename Traits::args_tuple;

    static std::vector<AccessDescriptor> accesses() {
        return detail::collect_accesses<ArgsTuple>();
    }

    static std::function<void(World&)> wrap(F fn) {
        return [fn = std::move(fn)](World& world) mutable {
            detail::invoke_system<F, ArgsTuple>(fn, world);
        };
    }
};

} // namespace helios

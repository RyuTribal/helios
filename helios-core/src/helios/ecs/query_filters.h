#pragma once
#include <type_traits>
#include <tuple>

namespace helios {

// ---------------------------------------------------------------------------
// Filter tag types
// ---------------------------------------------------------------------------

/// Require component T to be present on the archetype, but do not fetch it.
template <typename T>
struct With {};

/// Require component T to be absent from the archetype; do not fetch it.
template <typename T>
struct Without {};

/// Optionally fetch component T.  Yields T* (nullptr when absent).
template <typename T>
struct Optional {};

/// Non-archetypal filter: only yield entities where component T was modified
/// since the owning system last ran.  This is a per-entity check in the
/// iterator, not an archetype-level filter.
template <typename T>
struct Changed {};

// ---------------------------------------------------------------------------
// Type traits: detect filter wrappers
// ---------------------------------------------------------------------------

template <typename T> struct is_with : std::false_type {};
template <typename T> struct is_with<With<T>> : std::true_type {};
template <typename T> inline constexpr bool is_with_v = is_with<T>::value;

template <typename T> struct is_without : std::false_type {};
template <typename T> struct is_without<Without<T>> : std::true_type {};
template <typename T> inline constexpr bool is_without_v = is_without<T>::value;

template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<Optional<T>> : std::true_type {};
template <typename T> inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T> struct is_changed : std::false_type {};
template <typename T> struct is_changed<Changed<T>> : std::true_type {};
template <typename T> inline constexpr bool is_changed_v = is_changed<T>::value;

/// True when the parameter is a filter (With/Without/Changed) that should
/// NOT appear in the result tuple.
template <typename T>
inline constexpr bool is_filter_v = is_with_v<T> || is_without_v<T> || is_changed_v<T>;

/// True when the parameter contributes a value to the result tuple
/// (plain component or Optional).
template <typename T>
inline constexpr bool is_fetch_v = !is_filter_v<T>;

// ---------------------------------------------------------------------------
// Inner-type extraction
// ---------------------------------------------------------------------------

/// Extract the inner component type from any filter/optional wrapper.
template <typename T> struct filter_inner          { using type = T; };
template <typename T> struct filter_inner<With<T>>     { using type = T; };
template <typename T> struct filter_inner<Without<T>>  { using type = T; };
template <typename T> struct filter_inner<Optional<T>> { using type = T; };
template <typename T> struct filter_inner<Changed<T>>  { using type = T; };
template <typename T> using filter_inner_t = typename filter_inner<T>::type;

/// Strip const and filter wrappers to get the raw component type.
template <typename T>
using raw_component_t = std::remove_const_t<filter_inner_t<T>>;

// ---------------------------------------------------------------------------
// Fetch-tuple construction
// ---------------------------------------------------------------------------

/// The reference/pointer type a single Param produces in the result tuple.
///   const T        -> const T&
///   T              -> T&
///   Optional<T>    -> T*
///   With / Without -> (excluded)
template <typename T, typename = void>
struct fetch_type;

template <typename T>
struct fetch_type<T, std::enable_if_t<!is_filter_v<T> && !is_optional_v<T>>> {
    // Plain component. Preserve const.
    using type = std::conditional_t<std::is_const_v<T>,
                                    const std::remove_const_t<T>&,
                                    T&>;
};

template <typename T>
struct fetch_type<Optional<T>, void> {
    using type = T*;
};

template <typename T>
using fetch_type_t = typename fetch_type<T>::type;

/// Build the result tuple for a set of Params, *excluding* With/Without.
/// We need lazy evaluation to avoid instantiating fetch_type_t on filter types.

// Helper: concatenate a single element tuple with Rest.
template <typename Head, typename Rest>
struct prepend_fetch {
    using type = decltype(std::tuple_cat(
        std::declval<std::tuple<fetch_type_t<Head>>>(),
        std::declval<Rest>()));
};

template <typename... Params>
struct fetch_tuple;

template <>
struct fetch_tuple<> {
    using type = std::tuple<>;
};

// Filter types (With/Without) – skip.
template <typename Head, typename... Tail>
    requires is_filter_v<Head>
struct fetch_tuple<Head, Tail...> {
    using type = typename fetch_tuple<Tail...>::type;
};

// Fetch types (plain, const, Optional) – prepend to rest.
template <typename Head, typename... Tail>
    requires (!is_filter_v<Head>)
struct fetch_tuple<Head, Tail...> {
    using type = typename prepend_fetch<Head, typename fetch_tuple<Tail...>::type>::type;
};

template <typename... Params>
using fetch_tuple_t = typename fetch_tuple<Params...>::type;

} // namespace helios

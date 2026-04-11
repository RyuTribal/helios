#pragma once

#include "helios/core/log_macros.h"
#include <cstdlib>
#include <cstdio>

namespace helios {

// Internal: called by assert macros on failure
// Defined in assert.cpp to avoid header bloat
[[noreturn]] void assert_fail(const char* condition, const char* file, int line,
                              const char* func, const char* msg = nullptr);

} // namespace helios

/// HELIOS_ASSERT(condition) — Aborts with full logging if condition is false.
/// Stripped in Release builds (NDEBUG defined).
/// Use for internal invariants that should never fail in correct code.
#ifdef NDEBUG
    #define HELIOS_ASSERT(condition, ...) ((void)0)
#else
    #define HELIOS_ASSERT(condition, ...)                                          \
        do {                                                                       \
            if (!(condition)) [[unlikely]] {                                        \
                ::helios::assert_fail(#condition, __FILE__, __LINE__,               \
                    __func__ __VA_OPT__(,) __VA_ARGS__);                           \
            }                                                                      \
        } while (false)
#endif

/// HELIOS_VERIFY(condition) — Like ASSERT but NOT stripped in Release.
/// Use at system boundaries (user input, file I/O, external API returns).
#define HELIOS_VERIFY(condition, ...)                                              \
    do {                                                                           \
        if (!(condition)) [[unlikely]] {                                            \
            ::helios::assert_fail(#condition, __FILE__, __LINE__,                   \
                __func__ __VA_OPT__(,) __VA_ARGS__);                               \
        }                                                                          \
    } while (false)

/// HELIOS_UNREACHABLE() — Marks code paths that should never execute.
#define HELIOS_UNREACHABLE(...)                                                    \
    ::helios::assert_fail("UNREACHABLE", __FILE__, __LINE__,                       \
        __func__ __VA_OPT__(,) __VA_ARGS__)

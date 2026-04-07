// helios-rewrite/helios-core/src/helios/core/log_macros.h
#pragma once

#include "helios/core/log_level.h"
#include "helios/core/log_channel.h"
#include "helios/core/log_entry.h"
#include "helios/core/log_system.h"

/// ============================================================================
/// HELIOS_LOG -- The primary logging macro.
///
/// Usage:
///   HELIOS_LOG(Renderer, Info, "Loaded texture: {}", name)
///       .field("width", 1024)
///       .field("format", "BC7");
///
/// The channel name is a bare token (not a string). It must match a channel
/// defined with HELIOS_DEFINE_LOG_CHANNEL(Renderer).
///
/// The level is a bare token: Trace, Debug, Info, Warn, Error, Fatal.
///
/// Returns a LogEntry (or NullLogEntry) that supports .field() chaining.
/// The log message is emitted when the LogEntry is destroyed (end of statement).
/// ============================================================================

/// Internal: dispatch to the right logger, check channel + system filters.
#define HELIOS_LOG_IMPL(Channel, Level, ...)                                    \
    [&]() -> decltype(auto) {                                                   \
        auto& _hlc = HELIOS_LOG_CHANNEL(Channel);                              \
        constexpr auto _hll = ::helios::LogLevel::Level;                        \
        if (_hlc.should_log(_hll)) {                                            \
            auto* _hls = ::helios::LogSystem::instance();                       \
            if (_hls) {                                                         \
                return _hls->log(_hlc, _hll, __VA_ARGS__);                      \
            }                                                                   \
        }                                                                       \
        /* Channel disabled or no LogSystem -- return a real LogEntry that */   \
        /* won't emit (null logger). This keeps the return type consistent. */  \
        return ::helios::LogEntry(nullptr, _hll, "");                           \
    }()

// ---- Per-level macros with compile-time stripping ----

// Trace (level 0) -- stripped in Release/Dist
#if HELIOS_LOG_ACTIVE_LEVEL <= 0
    #define HELIOS_LOG_TRACE(Channel, ...) HELIOS_LOG_IMPL(Channel, Trace, __VA_ARGS__)
#else
    #define HELIOS_LOG_TRACE(Channel, ...) ::helios::NullLogEntry{}
#endif

// Debug (level 1) -- stripped in Release/Dist
#if HELIOS_LOG_ACTIVE_LEVEL <= 1
    #define HELIOS_LOG_DEBUG(Channel, ...) HELIOS_LOG_IMPL(Channel, Debug, __VA_ARGS__)
#else
    #define HELIOS_LOG_DEBUG(Channel, ...) ::helios::NullLogEntry{}
#endif

// Info (level 2) -- stripped in Dist
#if HELIOS_LOG_ACTIVE_LEVEL <= 2
    #define HELIOS_LOG_INFO(Channel, ...) HELIOS_LOG_IMPL(Channel, Info, __VA_ARGS__)
#else
    #define HELIOS_LOG_INFO(Channel, ...) ::helios::NullLogEntry{}
#endif

// Warn (level 3)
#if HELIOS_LOG_ACTIVE_LEVEL <= 3
    #define HELIOS_LOG_WARN(Channel, ...) HELIOS_LOG_IMPL(Channel, Warn, __VA_ARGS__)
#else
    #define HELIOS_LOG_WARN(Channel, ...) ::helios::NullLogEntry{}
#endif

// Error (level 4)
#if HELIOS_LOG_ACTIVE_LEVEL <= 4
    #define HELIOS_LOG_ERROR(Channel, ...) HELIOS_LOG_IMPL(Channel, Error, __VA_ARGS__)
#else
    #define HELIOS_LOG_ERROR(Channel, ...) ::helios::NullLogEntry{}
#endif

// Fatal (level 5) -- NEVER stripped. Always available.
#define HELIOS_LOG_FATAL(Channel, ...)                                          \
    [&]() -> ::helios::NullLogEntry {                                           \
        auto& _hlc = HELIOS_LOG_CHANNEL(Channel);                              \
        auto* _hls = ::helios::LogSystem::instance();                           \
        std::string _msg = fmt::format(__VA_ARGS__);                            \
        if (_hls) {                                                             \
            /* Log through normal path first */                                 \
            auto _entry = _hls->log(_hlc, ::helios::LogLevel::Fatal, "{}", _msg); \
            /* Force emit by destroying entry */                                \
        }                                                                       \
        /* handle_fatal will flush, dump crash context, and abort */            \
        if (_hls) {                                                             \
            _hls->handle_fatal(_hlc.name, _msg);                               \
        } else {                                                                \
            std::cerr << "[FATAL] [" << _hlc.name << "] " << _msg << std::endl; \
            std::abort();                                                       \
        }                                                                       \
    }()

/// The unified HELIOS_LOG macro. Dispatches to the per-level macro.
///
/// Usage: HELIOS_LOG(Renderer, Info, "Loaded texture: {}", name)
///
/// For explicit level macros (preferred when you want compile-time stripping
/// guarantees), use HELIOS_LOG_TRACE, HELIOS_LOG_DEBUG, etc.
///
/// This unified macro always compiles the format arguments but performs a
/// runtime channel+level check. For hot paths, prefer the per-level macros
/// to get compile-time stripping of Trace/Debug in Release builds.
#define HELIOS_LOG(Channel, Level, ...) HELIOS_LOG_IMPL(Channel, Level, __VA_ARGS__)

// ---- Built-in channels ----
// These are always available. Subsystems define their own.

/// Core engine channel -- for engine internals.
HELIOS_DEFINE_LOG_CHANNEL(Core);

/// Application channel -- for game/app code.
HELIOS_DEFINE_LOG_CHANNEL(App);

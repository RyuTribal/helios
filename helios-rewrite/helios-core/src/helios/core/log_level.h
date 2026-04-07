// helios-rewrite/helios-core/src/helios/core/log_level.h
#pragma once

#include <cstdint>
#include <string_view>

namespace helios {

/// Verbosity levels, ordered from most verbose to least.
/// Matches spdlog::level::level_enum ordering for easy mapping.
enum class LogLevel : uint8_t {
    Trace = 0,  // Extremely verbose, per-frame data
    Debug = 1,  // Debug diagnostics, enabled only in Debug builds
    Info  = 2,  // Normal operational messages
    Warn  = 3,  // Something unexpected but recoverable
    Error = 4,  // Something failed, subsystem may be degraded
    Fatal = 5,  // Unrecoverable, will crash after logging
    Off   = 6,  // Sentinel: disables all logging for a channel
};

/// Convert LogLevel to human-readable string.
constexpr std::string_view log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "Trace";
        case LogLevel::Debug: return "Debug";
        case LogLevel::Info:  return "Info";
        case LogLevel::Warn:  return "Warn";
        case LogLevel::Error: return "Error";
        case LogLevel::Fatal: return "Fatal";
        case LogLevel::Off:   return "Off";
    }
    return "Unknown";
}

/// Parse a string to LogLevel. Returns LogLevel::Trace on unknown input.
constexpr LogLevel log_level_from_string(std::string_view str) {
    if (str == "Trace" || str == "trace") return LogLevel::Trace;
    if (str == "Debug" || str == "debug") return LogLevel::Debug;
    if (str == "Info"  || str == "info")  return LogLevel::Info;
    if (str == "Warn"  || str == "warn")  return LogLevel::Warn;
    if (str == "Error" || str == "error") return LogLevel::Error;
    if (str == "Fatal" || str == "fatal") return LogLevel::Fatal;
    if (str == "Off"   || str == "off")   return LogLevel::Off;
    return LogLevel::Trace;
}

/// Compile-time active level. Messages below this are compiled out entirely.
/// Set via CMake: -DHELIOS_LOG_ACTIVE_LEVEL=0 (Trace) through 6 (Off).
/// Defaults:
///   Debug build:   0 (Trace)  -- everything enabled
///   Release build: 2 (Info)   -- Trace and Debug compiled out
///   Dist build:    3 (Warn)   -- Trace, Debug, Info compiled out
#ifndef HELIOS_LOG_ACTIVE_LEVEL
    #ifdef NDEBUG
        #define HELIOS_LOG_ACTIVE_LEVEL 2  // Info
    #else
        #define HELIOS_LOG_ACTIVE_LEVEL 0  // Trace
    #endif
#endif

} // namespace helios

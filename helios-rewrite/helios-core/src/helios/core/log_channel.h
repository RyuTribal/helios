// helios-rewrite/helios-core/src/helios/core/log_channel.h
#pragma once

#include "helios/core/log_level.h"

#include <atomic>
#include <string_view>

namespace helios {

/// A log channel represents a subsystem's logging identity.
/// Each channel has:
///   - A compile-time name (e.g., "Renderer", "Physics", "Audio")
///   - A runtime-adjustable minimum verbosity level
///   - A runtime-adjustable enabled flag
///
/// Channels are defined at file scope via HELIOS_DEFINE_LOG_CHANNEL.
/// They are lightweight -- just a name + two atomics.
struct LogChannel {
    const char* name;
    std::atomic<LogLevel> min_level{LogLevel::Trace};
    std::atomic<bool>     enabled{true};

    /// Construct with a name and optional default minimum level.
    constexpr LogChannel(const char* channel_name, LogLevel default_level = LogLevel::Trace)
        : name(channel_name)
        , min_level(default_level)
        , enabled(true)
    {}

    // Non-copyable, non-movable (lives at file scope as a global)
    LogChannel(const LogChannel&) = delete;
    LogChannel& operator=(const LogChannel&) = delete;
    LogChannel(LogChannel&&) = delete;
    LogChannel& operator=(LogChannel&&) = delete;

    /// Check if a message at the given level should be emitted.
    [[nodiscard]] bool should_log(LogLevel level) const {
        return enabled.load(std::memory_order_relaxed)
            && static_cast<uint8_t>(level) >= static_cast<uint8_t>(min_level.load(std::memory_order_relaxed));
    }

    /// Set the runtime minimum level for this channel.
    void set_level(LogLevel level) {
        min_level.store(level, std::memory_order_relaxed);
    }

    /// Enable or disable this channel entirely.
    void set_enabled(bool value) {
        enabled.store(value, std::memory_order_relaxed);
    }
};

} // namespace helios

/// Define a log channel. Place in exactly ONE .cpp file (or in a header as inline).
/// Usage:
///   HELIOS_DEFINE_LOG_CHANNEL(Renderer)
///   HELIOS_DEFINE_LOG_CHANNEL_WITH_LEVEL(Audio, LogLevel::Info)
///
/// This creates a global `LogChannel` instance named `LogChannel_<Name>`.
/// The HELIOS_LOG macro references this by token pasting.
#define HELIOS_DEFINE_LOG_CHANNEL(Name) \
    inline ::helios::LogChannel LogChannel_##Name{#Name}

#define HELIOS_DEFINE_LOG_CHANNEL_WITH_LEVEL(Name, DefaultLevel) \
    inline ::helios::LogChannel LogChannel_##Name{#Name, DefaultLevel}

/// Declare an extern log channel (use in headers, define in one .cpp).
#define HELIOS_DECLARE_LOG_CHANNEL(Name) \
    extern ::helios::LogChannel LogChannel_##Name

/// Access a log channel by name token.
#define HELIOS_LOG_CHANNEL(Name) LogChannel_##Name

// helios-rewrite/helios-core/src/helios/core/log_channel.h
#pragma once

#include "helios/core/log_level.h"
#include "helios/core/log_sink.h"
#include "helios/core/log_channel_registry.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace helios {

/// A log channel represents a subsystem's logging identity.
/// Each channel has:
///   - A compile-time name (e.g., "Renderer", "Physics", "Audio")
///   - A runtime-adjustable minimum verbosity level
///   - A runtime-adjustable enabled flag
///
/// **Convenience layer (macros):** Channels defined at file scope via
/// HELIOS_DEFINE_LOG_CHANNEL are lightweight -- just a name + two atomics.
/// They rely on LogSystem to route messages to sinks.
///
/// **Power layer (standalone):** Create a channel with your own sinks and
/// call channel.log() directly. No LogSystem required.
///
///   auto sink = std::make_shared<ConsoleSink>();
///   LogChannel ch("MyModule", LogLevel::Debug, {sink});
///   ch.log(LogLevel::Info, "Hello from {}", "my module");
///
struct LogChannel {
    const char* name;
    std::atomic<LogLevel> min_level{LogLevel::Trace};
    std::atomic<bool>     enabled{true};

    /// Construct with a name and optional default minimum level.
    /// Used by HELIOS_DEFINE_LOG_CHANNEL macros.
    /// Registers itself with LogChannelRegistry; aborts on duplicate names.
    LogChannel(const char* channel_name, LogLevel default_level = LogLevel::Trace)
        : name(channel_name)
        , min_level(default_level)
        , enabled(true)
    {
        LogChannelRegistry::register_channel(*this);
    }

    /// Construct a standalone channel with user-provided sinks.
    /// This channel can be used independently of LogSystem.
    /// Registers itself with LogChannelRegistry; aborts on duplicate names.
    LogChannel(const char* channel_name, LogLevel default_level,
               std::initializer_list<std::shared_ptr<LogSink>> sinks)
        : name(channel_name)
        , min_level(default_level)
        , enabled(true)
        , m_sinks(sinks)
    {
        LogChannelRegistry::register_channel(*this);
    }

    /// Construct a standalone channel with a vector of sinks.
    /// Registers itself with LogChannelRegistry; aborts on duplicate names.
    LogChannel(const char* channel_name, LogLevel default_level,
               std::vector<std::shared_ptr<LogSink>> sinks)
        : name(channel_name)
        , min_level(default_level)
        , enabled(true)
        , m_sinks(std::move(sinks))
    {
        LogChannelRegistry::register_channel(*this);
    }

    // Non-copyable, non-movable (lives at file scope as a global)
    LogChannel(const LogChannel&) = delete;
    LogChannel& operator=(const LogChannel&) = delete;
    LogChannel(LogChannel&&) = delete;
    LogChannel& operator=(LogChannel&&) = delete;

    /// Destructor -- deregisters from the channel registry.
    /// Inline globals (program lifetime) are never destroyed in practice;
    /// standalone channels deregister when they go out of scope.
    ~LogChannel() {
        LogChannelRegistry::deregister_channel(*this);
    }

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

    // ---- Standalone logging API (power layer) ----

    /// Log a message through this channel's sinks directly.
    /// Bypasses LogSystem entirely. Only works if the channel was constructed
    /// with sinks; otherwise this is a no-op.
    template<typename... Args>
    void log(LogLevel level, fmt::format_string<Args...> fmt_str, Args&&... args) {
        if (!should_log(level) || m_sinks.empty()) return;
        std::string formatted = fmt::format(fmt_str, std::forward<Args>(args)...);
        for (auto& sink : m_sinks) {
            sink->write(level, name, formatted);
        }
    }

    /// Flush all sinks attached to this channel.
    void flush() {
        for (auto& sink : m_sinks) {
            sink->flush();
        }
    }

    /// Add a sink to this channel at runtime.
    void add_sink(std::shared_ptr<LogSink> sink) {
        m_sinks.push_back(std::move(sink));
    }

    /// Check if this channel has standalone sinks attached.
    [[nodiscard]] bool has_sinks() const { return !m_sinks.empty(); }

    /// Get the sinks (read-only).
    [[nodiscard]] const std::vector<std::shared_ptr<LogSink>>& sinks() const { return m_sinks; }

private:
    /// Sinks for standalone use. Empty for macro-defined channels
    /// (those route through LogSystem instead).
    std::vector<std::shared_ptr<LogSink>> m_sinks;
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

// helios-rewrite/helios-core/src/helios/core/log_system.h
#pragma once

#include "helios/core/log_level.h"
#include "helios/core/log_channel.h"
#include "helios/core/log_entry.h"

#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <fstream>

namespace helios {

/// Configuration for creating a LogSystem.
struct LogConfig {
    /// Directory where log files are written. Created if it doesn't exist.
    std::string log_directory = "logs";

    /// Name of the primary log file (inside log_directory).
    std::string log_file_name = "helios.log";

    /// Maximum size of a single log file before rotation (bytes). Default: 10 MB.
    std::size_t max_file_size = 10 * 1024 * 1024;

    /// Maximum number of rotated log files to keep. Default: 5.
    std::size_t max_rotated_files = 5;

    /// Capacity of the in-memory ring buffer (number of messages). Default: 1024.
    std::size_t ring_buffer_capacity = 1024;

    /// Enable colored console output. Default: true.
    bool console_colored = true;

    /// Enable file sink. Default: true.
    bool enable_file_sink = true;

    /// Enable console sink. Default: true.
    bool enable_console_sink = true;

    /// Enable ring buffer sink (for in-game console / crash dumps). Default: true.
    bool enable_ring_buffer_sink = true;

    /// Use async logging (spdlog thread pool). Default: false.
    /// When true, log calls are non-blocking and messages are processed
    /// by a background thread. Recommended for high-throughput scenarios.
    bool async_mode = false;

    /// Async queue size (only used when async_mode=true). Default: 8192.
    std::size_t async_queue_size = 8192;

    /// Number of async worker threads (only used when async_mode=true). Default: 1.
    std::size_t async_thread_count = 1;

    /// Default log level for all channels. Default: Trace (everything).
    LogLevel default_level = LogLevel::Trace;

    /// Log pattern for file output.
    /// Tokens: %Y=year, %m=month, %d=day, %H=hour, %M=min, %S=sec, %e=ms,
    ///         %t=thread_id, %n=logger_name(channel), %l=level, %v=message
    std::string file_pattern = "[%Y-%m-%d %H:%M:%S.%e] [tid %t] [%n] [%l] %v";

    /// Log pattern for console output.
    std::string console_pattern = "[%H:%M:%S.%e] [%t] [%n] [%^%l%$] %v";
};

/// The central logging system. RAII -- constructor sets up sinks, destructor
/// flushes and tears down. NOT a singleton: you own the instance.
///
/// A global pointer (LogSystem::s_instance) is set by the constructor so that
/// HELIOS_LOG macros can find the system without threading it through every call.
/// If multiple LogSystem instances are created, the most recently constructed one
/// is the active global (this is unusual; normally there's exactly one).
///
/// **Usage as standalone (before World/App exist):**
///   LogSystem log_system(LogConfig{});
///   HELIOS_LOG(Core, Info, "Engine starting");
///
/// **Usage as World Resource (once ECS is up):**
///   app.insert_resource<LogSystem>(LogConfig{});
///   // HELIOS_LOG macros still work via s_instance.
///
class LogSystem {
public:
    /// Construct and initialize all sinks. Sets the global instance pointer.
    explicit LogSystem(const LogConfig& config = LogConfig{});

    /// Flush all loggers, clear the global instance pointer, drop spdlog state.
    ~LogSystem();

    // Non-copyable
    LogSystem(const LogSystem&) = delete;
    LogSystem& operator=(const LogSystem&) = delete;

    // Movable (transfers ownership of global pointer)
    LogSystem(LogSystem&& other) noexcept;
    LogSystem& operator=(LogSystem&& other) noexcept;

    /// Get the global LogSystem instance. May return nullptr if none is active.
    [[nodiscard]] static LogSystem* instance() { return s_instance; }

    /// Get or create a spdlog logger for the given channel name.
    /// Channels are lazily created on first use. All channels share the same sinks.
    /// Thread-safe.
    [[nodiscard]] spdlog::logger* get_or_create_channel_logger(std::string_view channel_name);

    /// Create a LogEntry for a given channel and level. The caller is responsible
    /// for the compile-time and channel-level filtering BEFORE calling this.
    /// This is the "hot path" that HELIOS_LOG macros call after filtering.
    template<typename... Args>
    [[nodiscard]] LogEntry log(LogChannel& channel, LogLevel level,
                               fmt::format_string<Args...> fmt_str, Args&&... args) {
        spdlog::logger* logger = get_or_create_channel_logger(channel.name);
        std::string formatted = fmt::format(fmt_str, std::forward<Args>(args)...);
        return LogEntry(logger, level, std::move(formatted));
    }

    // ---- Runtime filtering API ----

    /// Set the minimum log level for ALL channels (global default).
    void set_global_level(LogLevel level);

    /// Set the minimum log level for a specific channel (by name).
    void set_channel_level(std::string_view channel_name, LogLevel level);

    /// Enable or disable a specific channel (by name).
    void set_channel_enabled(std::string_view channel_name, bool enabled);

    /// Enable or disable a specific channel (by reference).
    static void set_channel_enabled(LogChannel& channel, bool enabled);

    /// Set the minimum level on a channel reference.
    static void set_channel_level(LogChannel& channel, LogLevel level);

    // ---- Ring buffer access (for in-game console) ----

    /// Get the last N formatted log messages from the ring buffer.
    /// Returns up to `count` messages, or all if count==0.
    [[nodiscard]] std::vector<std::string> get_recent_messages(std::size_t count = 0) const;

    // ---- Crash context ----

    /// Dump the ring buffer contents to a crash log file.
    /// Called automatically on Fatal, but can also be called manually.
    void dump_crash_context(std::string_view crash_file_path = "") const;

    /// Handle a fatal log event: dump crash context, flush, abort.
    [[noreturn]] void handle_fatal(std::string_view channel_name, std::string_view message) const;

    // ---- Accessors ----

    [[nodiscard]] const LogConfig& config() const { return m_config; }
    [[nodiscard]] bool is_async() const { return m_config.async_mode; }

    /// Flush all sinks immediately.
    void flush();

private:
    /// Create shared sinks based on config.
    void create_sinks();

    /// Create a new spdlog logger for the given name, using shared sinks.
    std::shared_ptr<spdlog::logger> create_logger(const std::string& name);

    /// Map helios::LogLevel to spdlog::level::level_enum.
    static spdlog::level::level_enum to_spdlog_level(LogLevel level);

    LogConfig m_config;

    // Shared sinks -- all channel loggers use these
    std::vector<spdlog::sink_ptr>                    m_sinks;
    std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> m_ring_buffer_sink;

    // Channel loggers, keyed by channel name
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_loggers;
    mutable std::mutex m_loggers_mutex;

    // Async thread pool (only created when async_mode=true)
    std::shared_ptr<spdlog::details::thread_pool> m_thread_pool;

    // Global instance pointer for macro access
    static LogSystem* s_instance;

    // Track whether this instance owns the global pointer
    bool m_is_global_owner = false;
};

} // namespace helios

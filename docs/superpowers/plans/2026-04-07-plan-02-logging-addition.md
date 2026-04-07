# Logging System -- Task 0 Addition to Plan 2 (Scheduler, App, Plugins)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a comprehensive, RAII-based, spdlog-backed logging system with log channels/categories, runtime filtering, multiple sinks (console, rotating file, ring buffer for in-game console), structured fields, compile-time stripping, crash context dump, and async support. This is Task 0 of Plan 2 because every subsequent subsystem depends on logging.

**Depends on:** Plan 1 (CMake + ECS Core) -- assumes the `helios-rewrite/` directory structure, `helios-core` CMake target, and test infrastructure exist.

**Does NOT depend on:** World, App, Scheduler, or any other Plan 2 types. The logging system must work standalone (before World/App exist) AND as an optional World Resource once the ECS is running.

**Spec:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` -- `core/logging.h / logging.cpp (spdlog wrapper, RAII)`

**Tech Stack:** C++20, spdlog 1.x (FetchContent), fmt (bundled with spdlog), Google Test

**Key directory:** `helios-rewrite/helios-core/src/core/` and `helios-rewrite/helios-core/include/helios/core/`

---

## Insertion Point in Plan 2

This task is **Task 0** and must be inserted BEFORE the current Task 1 (Schedule Enum and AccessDescriptor). The existing Plan 2 tasks (Task 1 through Task 22) remain unchanged in content but are conceptually preceded by this task. No renumbering of existing tasks is required -- this is Task 0.

**Rationale:** Logging is the most fundamental infrastructure. The scheduler, app, plugin system, thread pool, and every other subsystem will use `HELIOS_LOG(...)` for diagnostics, warnings, and errors. It must exist first.

---

## Research Summary: Best Practices from Major Engines

### Unreal Engine (UE_LOG)
- **Category macros:** `DECLARE_LOG_CATEGORY_EXTERN(LogRenderer, Log, All)` + `DEFINE_LOG_CATEGORY(LogRenderer)` -- each subsystem declares its own category at file scope. Categories are compile-time tokens, not runtime strings.
- **Verbosity enum:** `Fatal`, `Error`, `Warning`, `Display`, `Log`, `Verbose`, `VeryVerbose`. Two thresholds: compile-time (strip below this) and runtime (filter below this).
- **Macro API:** `UE_LOG(LogRenderer, Warning, TEXT("Texture %s missing"), *Name)` -- category is a bare token, not a string.
- **Output devices:** Console, file, IDE output, crash reporter. Multiple output devices can be registered.
- **Structured data:** UE5 added structured logging with key-value fields for telemetry pipelines.
- **Crash context:** `FGenericCrashContext` captures recent log lines and writes them to crash minidumps.

### Godot
- Simpler approach: `print_line()`, `WARN_PRINT()`, `ERR_PRINT()`, `ERR_FAIL_COND_MSG()`.
- No channel system -- all goes to one output. Filtering is minimal.
- Logger classes: `StdLogger`, `FileLogger`, `CompositeLogger`. Hot-swappable at runtime.
- Takeaway: Simplicity is good for the API surface, but we want more filtering power.

### spdlog Best Practices
- **Multiple loggers, shared sinks:** Create one logger per subsystem (= our "channel"), but share sink objects across loggers. This gives per-channel filtering while all output goes to the same file/console.
- **Async logger:** `spdlog::async_logger` backed by a thread pool with MPMC queue. Use `async_overflow_policy::overrun_oldest` for games (never block the game thread).
- **Ring buffer sink:** `spdlog::sinks::ringbuffer_sink_mt` keeps last N messages in memory -- perfect for in-game console and crash dumps.
- **Rotating file sink:** `spdlog::sinks::rotating_file_sink_mt` with max size + max files.
- **Pattern:** `[%Y-%m-%d %H:%M:%S.%e] [%t] [%n] [%l] %v` -- timestamp, thread ID, logger name, level, message.
- **Compile-time level:** `#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE` before including spdlog -- but we implement our own macro layer for channel support.
- **fmt integration:** spdlog uses fmt for formatting. We expose `{}` syntax to callers.

### Structured Logging Patterns
- Fluent API: `log.info("loaded texture").field("width", w).field("path", p)` returns a builder that appends `| width=1024 path=/tex/foo.png` to the message on destruction.
- Alternative: Embed structured fields into the fmt string. We choose the fluent builder approach because it separates human-readable message from machine-parseable metadata.

### Design Decisions for Helios
1. **RAII, not singleton:** `LogSystem` is a normal object. A global `LogSystem*` pointer is set by the constructor (like a "service locator" pattern) so macros can find it, but the object itself is owned by `main()` or `App`.
2. **Channel = spdlog logger:** Each `HELIOS_DEFINE_LOG_CHANNEL(X)` creates a function that lazily obtains/creates a named spdlog logger sharing the global sinks. Channel filtering is done via spdlog's per-logger level + our own enabled/disabled flag.
3. **Structured fields via RAII builder:** `HELIOS_LOG(...)` returns a `LogEntry` whose destructor actually emits the log line, allowing `.field()` chaining.
4. **Compile-time stripping:** `HELIOS_LOG_TRACE` and `HELIOS_LOG_DEBUG` expand to nothing in non-debug builds via `#if` guards on `HELIOS_LOG_ACTIVE_LEVEL`.
5. **Crash context:** On `Fatal`, the ring buffer contents are dumped to `logs/crash_context.log` before `std::abort()`.

---

## Task 0: Logging System

### Files to create:
- `helios-rewrite/helios-core/include/helios/core/log_level.h`
- `helios-rewrite/helios-core/include/helios/core/log_channel.h`
- `helios-rewrite/helios-core/include/helios/core/log_entry.h`
- `helios-rewrite/helios-core/include/helios/core/log_system.h`
- `helios-rewrite/helios-core/include/helios/core/log_macros.h`
- `helios-rewrite/helios-core/include/helios/core/logging.h` (umbrella header)
- `helios-rewrite/helios-core/src/core/log_system.cpp`
- `helios-rewrite/tests/core/test_logging.cpp`

### Files to modify:
- `helios-rewrite/helios-core/CMakeLists.txt` (add source, add spdlog dependency)
- `helios-rewrite/CMakeLists.txt` (add spdlog FetchContent)
- `helios-rewrite/tests/CMakeLists.txt` (add test source)

---

### Step 0.1: Add spdlog to root CMakeLists.txt

- [ ] **Modify `helios-rewrite/CMakeLists.txt`** -- add spdlog via FetchContent, right after the glm block:

Add the following after the `FetchContent_MakeAvailable(glm)` line:

```cmake
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)   # use spdlog's bundled fmt
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)
```

---

### Step 0.2: Create log_level.h

- [ ] **Create file: `helios-rewrite/helios-core/include/helios/core/log_level.h`**

```cpp
// helios-rewrite/helios-core/include/helios/core/log_level.h
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
```

---

### Step 0.3: Create log_channel.h

- [ ] **Create file: `helios-rewrite/helios-core/include/helios/core/log_channel.h`**

```cpp
// helios-rewrite/helios-core/include/helios/core/log_channel.h
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
```

---

### Step 0.4: Create log_entry.h

- [ ] **Create file: `helios-rewrite/helios-core/include/helios/core/log_entry.h`**

```cpp
// helios-rewrite/helios-core/include/helios/core/log_entry.h
#pragma once

#include "helios/core/log_level.h"

#include <spdlog/spdlog.h>
#include <spdlog/logger.h>

#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace helios {

/// RAII log entry builder. Returned by HELIOS_LOG macros.
///
/// Allows fluent field attachment:
///   HELIOS_LOG(Renderer, Info, "Loaded texture: {}", name)
///       .field("width", 1024)
///       .field("format", "BC7");
///
/// The actual log message is emitted in the destructor. This ensures that all
/// .field() calls are accumulated before the message is sent to spdlog.
///
/// If moved from (e.g., stored in a variable), only the final owner emits.
class LogEntry {
public:
    /// Construct a log entry. Typically called by HELIOS_LOG internals.
    LogEntry(spdlog::logger* logger, LogLevel level, std::string message)
        : m_logger(logger)
        , m_level(level)
        , m_message(std::move(message))
        , m_owns(true)
    {}

    /// Move constructor -- transfers ownership of emission.
    LogEntry(LogEntry&& other) noexcept
        : m_logger(other.m_logger)
        , m_level(other.m_level)
        , m_message(std::move(other.m_message))
        , m_fields(std::move(other.m_fields))
        , m_owns(other.m_owns)
    {
        other.m_owns = false;
    }

    /// Move assignment -- transfers ownership of emission.
    LogEntry& operator=(LogEntry&& other) noexcept {
        if (this != &other) {
            // Emit our current message if we own it
            if (m_owns) {
                emit();
            }
            m_logger = other.m_logger;
            m_level = other.m_level;
            m_message = std::move(other.m_message);
            m_fields = std::move(other.m_fields);
            m_owns = other.m_owns;
            other.m_owns = false;
        }
        return *this;
    }

    // Non-copyable
    LogEntry(const LogEntry&) = delete;
    LogEntry& operator=(const LogEntry&) = delete;

    /// Destructor emits the log message with all accumulated fields.
    ~LogEntry() {
        if (m_owns) {
            emit();
        }
    }

    /// Attach a string field.
    LogEntry& field(std::string_view key, std::string_view value) & {
        m_fields.emplace_back(std::string(key), std::string(value));
        return *this;
    }

    /// Attach a numeric field (integer).
    LogEntry& field(std::string_view key, int64_t value) & {
        m_fields.emplace_back(std::string(key), std::to_string(value));
        return *this;
    }

    /// Attach a numeric field (unsigned integer).
    LogEntry& field(std::string_view key, uint64_t value) & {
        m_fields.emplace_back(std::string(key), std::to_string(value));
        return *this;
    }

    /// Attach a numeric field (double).
    LogEntry& field(std::string_view key, double value) & {
        m_fields.emplace_back(std::string(key), std::to_string(value));
        return *this;
    }

    /// Attach a boolean field.
    LogEntry& field(std::string_view key, bool value) & {
        m_fields.emplace_back(std::string(key), value ? "true" : "false");
        return *this;
    }

    /// Attach a string field (rvalue chain -- for temporaries).
    LogEntry&& field(std::string_view key, std::string_view value) && {
        m_fields.emplace_back(std::string(key), std::string(value));
        return std::move(*this);
    }

    /// Attach a numeric field (rvalue chain).
    LogEntry&& field(std::string_view key, int64_t value) && {
        m_fields.emplace_back(std::string(key), std::to_string(value));
        return std::move(*this);
    }

    /// Attach a numeric field (rvalue chain, unsigned).
    LogEntry&& field(std::string_view key, uint64_t value) && {
        m_fields.emplace_back(std::string(key), std::to_string(value));
        return std::move(*this);
    }

    /// Attach a numeric field (rvalue chain, double).
    LogEntry&& field(std::string_view key, double value) && {
        m_fields.emplace_back(std::string(key), std::to_string(value));
        return std::move(*this);
    }

    /// Attach a boolean field (rvalue chain).
    LogEntry&& field(std::string_view key, bool value) && {
        m_fields.emplace_back(std::string(key), value ? "true" : "false");
        return std::move(*this);
    }

private:
    /// Actually send the message to spdlog.
    void emit() {
        if (!m_logger) return;

        // Build final message: "User message | key1=val1 key2=val2"
        std::string final_msg = m_message;
        if (!m_fields.empty()) {
            final_msg += " |";
            for (const auto& [key, value] : m_fields) {
                final_msg += ' ';
                final_msg += key;
                final_msg += '=';
                final_msg += value;
            }
        }

        // Map helios::LogLevel to spdlog::level
        switch (m_level) {
            case LogLevel::Trace: m_logger->trace("{}", final_msg);    break;
            case LogLevel::Debug: m_logger->debug("{}", final_msg);    break;
            case LogLevel::Info:  m_logger->info("{}", final_msg);     break;
            case LogLevel::Warn:  m_logger->warn("{}", final_msg);     break;
            case LogLevel::Error: m_logger->error("{}", final_msg);    break;
            case LogLevel::Fatal: m_logger->critical("{}", final_msg); break;
            default: break;
        }
    }

    spdlog::logger* m_logger = nullptr;
    LogLevel        m_level  = LogLevel::Info;
    std::string     m_message;
    std::vector<std::pair<std::string, std::string>> m_fields;
    bool            m_owns   = false;
};

/// A no-op log entry that discards everything. Used when a channel is
/// disabled or the log level is filtered out, so .field() calls compile
/// but do nothing.
class NullLogEntry {
public:
    NullLogEntry& field(std::string_view, std::string_view) & { return *this; }
    NullLogEntry& field(std::string_view, int64_t) &          { return *this; }
    NullLogEntry& field(std::string_view, uint64_t) &         { return *this; }
    NullLogEntry& field(std::string_view, double) &           { return *this; }
    NullLogEntry& field(std::string_view, bool) &             { return *this; }

    NullLogEntry&& field(std::string_view, std::string_view) && { return std::move(*this); }
    NullLogEntry&& field(std::string_view, int64_t) &&          { return std::move(*this); }
    NullLogEntry&& field(std::string_view, uint64_t) &&         { return std::move(*this); }
    NullLogEntry&& field(std::string_view, double) &&           { return std::move(*this); }
    NullLogEntry&& field(std::string_view, bool) &&             { return std::move(*this); }
};

} // namespace helios
```

---

### Step 0.5: Create log_system.h

- [ ] **Create file: `helios-rewrite/helios-core/include/helios/core/log_system.h`**

```cpp
// helios-rewrite/helios-core/include/helios/core/log_system.h
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
```

---

### Step 0.6: Create log_system.cpp

- [ ] **Create file: `helios-rewrite/helios-core/src/core/log_system.cpp`**

```cpp
// helios-rewrite/helios-core/src/core/log_system.cpp
#include "helios/core/log_system.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/pattern_formatter.h>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace helios {

// Static global instance pointer
LogSystem* LogSystem::s_instance = nullptr;

// ---- Construction / Destruction ----

LogSystem::LogSystem(const LogConfig& config)
    : m_config(config)
{
    // Create log directory if needed
    if (m_config.enable_file_sink) {
        std::filesystem::path log_dir(m_config.log_directory);
        if (!std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }
    }

    // Create shared sinks
    create_sinks();

    // Set up async thread pool if requested
    if (m_config.async_mode) {
        m_thread_pool = std::make_shared<spdlog::details::thread_pool>(
            m_config.async_queue_size,
            m_config.async_thread_count
        );
    }

    // Set global instance
    s_instance = this;
    m_is_global_owner = true;
}

LogSystem::~LogSystem() {
    // Flush all loggers
    flush();

    // Clear all our loggers from spdlog registry
    {
        std::lock_guard lock(m_loggers_mutex);
        for (auto& [name, logger] : m_loggers) {
            spdlog::drop(name);
        }
        m_loggers.clear();
    }

    // Reset thread pool
    m_thread_pool.reset();

    // Clear global instance if we own it
    if (m_is_global_owner && s_instance == this) {
        s_instance = nullptr;
    }
}

LogSystem::LogSystem(LogSystem&& other) noexcept
    : m_config(std::move(other.m_config))
    , m_sinks(std::move(other.m_sinks))
    , m_ring_buffer_sink(std::move(other.m_ring_buffer_sink))
    , m_loggers(std::move(other.m_loggers))
    , m_thread_pool(std::move(other.m_thread_pool))
    , m_is_global_owner(other.m_is_global_owner)
{
    other.m_is_global_owner = false;
    if (m_is_global_owner) {
        s_instance = this;
    }
}

LogSystem& LogSystem::operator=(LogSystem&& other) noexcept {
    if (this != &other) {
        // Clean up current state
        flush();
        {
            std::lock_guard lock(m_loggers_mutex);
            for (auto& [name, logger] : m_loggers) {
                spdlog::drop(name);
            }
            m_loggers.clear();
        }
        if (m_is_global_owner && s_instance == this) {
            s_instance = nullptr;
        }

        // Move from other
        m_config = std::move(other.m_config);
        m_sinks = std::move(other.m_sinks);
        m_ring_buffer_sink = std::move(other.m_ring_buffer_sink);
        m_loggers = std::move(other.m_loggers);
        m_thread_pool = std::move(other.m_thread_pool);
        m_is_global_owner = other.m_is_global_owner;
        other.m_is_global_owner = false;

        if (m_is_global_owner) {
            s_instance = this;
        }
    }
    return *this;
}

// ---- Sink creation ----

void LogSystem::create_sinks() {
    m_sinks.clear();

    // Console sink (colored)
    if (m_config.enable_console_sink) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(m_config.console_pattern);
        console_sink->set_level(to_spdlog_level(m_config.default_level));
        m_sinks.push_back(console_sink);
    }

    // Rotating file sink
    if (m_config.enable_file_sink) {
        std::string file_path = (std::filesystem::path(m_config.log_directory)
                                 / m_config.log_file_name).string();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file_path,
            m_config.max_file_size,
            m_config.max_rotated_files
        );
        file_sink->set_pattern(m_config.file_pattern);
        file_sink->set_level(to_spdlog_level(m_config.default_level));
        m_sinks.push_back(file_sink);
    }

    // Ring buffer sink (for in-game console and crash dumps)
    if (m_config.enable_ring_buffer_sink) {
        m_ring_buffer_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(
            m_config.ring_buffer_capacity
        );
        m_ring_buffer_sink->set_level(to_spdlog_level(m_config.default_level));
        m_sinks.push_back(m_ring_buffer_sink);
    }
}

// ---- Logger management ----

std::shared_ptr<spdlog::logger> LogSystem::create_logger(const std::string& name) {
    std::shared_ptr<spdlog::logger> logger;

    if (m_config.async_mode && m_thread_pool) {
        logger = std::make_shared<spdlog::async_logger>(
            name,
            m_sinks.begin(),
            m_sinks.end(),
            m_thread_pool,
            spdlog::async_overflow_policy::overrun_oldest
        );
    } else {
        logger = std::make_shared<spdlog::logger>(
            name,
            m_sinks.begin(),
            m_sinks.end()
        );
    }

    logger->set_level(to_spdlog_level(m_config.default_level));
    logger->flush_on(spdlog::level::err);

    // Register with spdlog (for global operations like flush_all)
    spdlog::register_logger(logger);

    return logger;
}

spdlog::logger* LogSystem::get_or_create_channel_logger(std::string_view channel_name) {
    std::string name_str(channel_name);

    // Fast path: check without lock (logger already exists)
    {
        std::lock_guard lock(m_loggers_mutex);
        auto it = m_loggers.find(name_str);
        if (it != m_loggers.end()) {
            return it->second.get();
        }

        // Slow path: create new logger
        auto logger = create_logger(name_str);
        auto* raw_ptr = logger.get();
        m_loggers.emplace(std::move(name_str), std::move(logger));
        return raw_ptr;
    }
}

// ---- Runtime filtering ----

void LogSystem::set_global_level(LogLevel level) {
    m_config.default_level = level;
    auto spd_level = to_spdlog_level(level);

    // Update all existing loggers
    std::lock_guard lock(m_loggers_mutex);
    for (auto& [name, logger] : m_loggers) {
        logger->set_level(spd_level);
    }

    // Update sinks
    for (auto& sink : m_sinks) {
        sink->set_level(spd_level);
    }
}

void LogSystem::set_channel_level(std::string_view channel_name, LogLevel level) {
    std::string name_str(channel_name);
    std::lock_guard lock(m_loggers_mutex);
    auto it = m_loggers.find(name_str);
    if (it != m_loggers.end()) {
        it->second->set_level(to_spdlog_level(level));
    }
}

void LogSystem::set_channel_enabled(std::string_view channel_name, bool enabled) {
    std::string name_str(channel_name);
    std::lock_guard lock(m_loggers_mutex);
    auto it = m_loggers.find(name_str);
    if (it != m_loggers.end()) {
        it->second->set_level(enabled ? to_spdlog_level(m_config.default_level)
                                      : spdlog::level::off);
    }
}

void LogSystem::set_channel_enabled(LogChannel& channel, bool enabled) {
    channel.set_enabled(enabled);
}

void LogSystem::set_channel_level(LogChannel& channel, LogLevel level) {
    channel.set_level(level);
}

// ---- Ring buffer access ----

std::vector<std::string> LogSystem::get_recent_messages(std::size_t count) const {
    if (!m_ring_buffer_sink) return {};
    return m_ring_buffer_sink->last_formatted(count);
}

// ---- Crash context ----

void LogSystem::dump_crash_context(std::string_view crash_file_path) const {
    std::string path;
    if (crash_file_path.empty()) {
        path = (std::filesystem::path(m_config.log_directory) / "crash_context.log").string();
    } else {
        path = std::string(crash_file_path);
    }

    auto messages = get_recent_messages(0); // Get all buffered messages

    // Use raw file I/O -- we might be crashing, avoid allocations where possible
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file << "=== HELIOS CRASH CONTEXT ===\n";
        file << "Last " << messages.size() << " log messages before crash:\n";
        file << "============================\n\n";
        for (const auto& msg : messages) {
            file << msg << '\n';
        }
        file << "\n=== END CRASH CONTEXT ===\n";
        file.flush();
    }
}

void LogSystem::handle_fatal(std::string_view channel_name, std::string_view message) const {
    // Log the fatal message to stderr directly as a fallback
    std::cerr << "[FATAL] [" << channel_name << "] " << message << std::endl;

    // Dump crash context
    dump_crash_context();

    // Flush all loggers
    spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
        logger->flush();
    });

    // Abort
    std::abort();
}

// ---- Flush ----

void LogSystem::flush() {
    std::lock_guard lock(m_loggers_mutex);
    for (auto& [name, logger] : m_loggers) {
        logger->flush();
    }
}

// ---- Level mapping ----

spdlog::level::level_enum LogSystem::to_spdlog_level(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return spdlog::level::trace;
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info:  return spdlog::level::info;
        case LogLevel::Warn:  return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        case LogLevel::Fatal: return spdlog::level::critical;
        case LogLevel::Off:   return spdlog::level::off;
    }
    return spdlog::level::info;
}

} // namespace helios
```

---

### Step 0.7: Create log_macros.h

- [ ] **Create file: `helios-rewrite/helios-core/include/helios/core/log_macros.h`**

```cpp
// helios-rewrite/helios-core/include/helios/core/log_macros.h
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

/// Internal: compile-time level gate. If the level is below the active level,
/// the entire macro expands to a NullLogEntry (zero cost, optimized away).
#define HELIOS_LOG_LEVEL_GATE(LevelValue, Channel, Level, ...)                  \
    []() -> decltype(auto) {                                                    \
        if constexpr (LevelValue >= HELIOS_LOG_ACTIVE_LEVEL) {                  \
            /* Trick: we need to capture the enclosing scope for format args */  \
            /* but constexpr if doesn't help with that in a lambda. */           \
            /* So we return a tag type and use a ternary outside. */             \
            return std::true_type{};                                            \
        } else {                                                                \
            return std::false_type{};                                           \
        }                                                                       \
    }()                                                                         \
    /* This part is never reached if the above returns false_type, but we */    \
    /* need the compiler to see both branches. The actual gating is below. */

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
```

---

### Step 0.8: Create logging.h (umbrella header)

- [ ] **Create file: `helios-rewrite/helios-core/include/helios/core/logging.h`**

```cpp
// helios-rewrite/helios-core/include/helios/core/logging.h
//
// Umbrella header for the Helios logging system.
// Include this single header to get everything.
//
#pragma once

#include "helios/core/log_level.h"
#include "helios/core/log_channel.h"
#include "helios/core/log_entry.h"
#include "helios/core/log_system.h"
#include "helios/core/log_macros.h"
```

---

### Step 0.9: Update helios-core CMakeLists.txt

- [ ] **Modify `helios-rewrite/helios-core/CMakeLists.txt`** to add the logging source and spdlog linkage.

The file should become:

```cmake
add_library(helios-core STATIC)

# Sources will be added as tasks progress
target_sources(helios-core
    PRIVATE
        src/ecs/entity.cpp
        src/ecs/entity_allocator.cpp
        src/ecs/archetype.cpp
        src/ecs/archetype_storage.cpp
        src/ecs/resource_storage.cpp
        src/ecs/event_storage.cpp
        src/ecs/world.cpp
        src/ecs/commands.cpp
        src/core/log_system.cpp
)

target_include_directories(helios-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(helios-core
    PUBLIC
        glm::glm
        spdlog::spdlog
)

target_compile_features(helios-core PUBLIC cxx_std_20)

# Compile-time log level configuration.
# Debug:   HELIOS_LOG_ACTIVE_LEVEL=0 (Trace -- everything)
# Release: HELIOS_LOG_ACTIVE_LEVEL=2 (Info  -- strip Trace+Debug)
# Dist:    HELIOS_LOG_ACTIVE_LEVEL=3 (Warn  -- strip Trace+Debug+Info)
target_compile_definitions(helios-core
    PUBLIC
        $<$<CONFIG:Debug>:HELIOS_LOG_ACTIVE_LEVEL=0>
        $<$<CONFIG:Release>:HELIOS_LOG_ACTIVE_LEVEL=2>
        $<$<CONFIG:RelWithDebInfo>:HELIOS_LOG_ACTIVE_LEVEL=1>
        $<$<CONFIG:MinSizeRel>:HELIOS_LOG_ACTIVE_LEVEL=3>
)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-core PRIVATE /W4)
else()
    target_compile_options(helios-core PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

---

### Step 0.10: Update tests CMakeLists.txt

- [ ] **Modify `helios-rewrite/tests/CMakeLists.txt`** to add the logging test:

```cmake
add_executable(helios-tests
    ecs/test_entity.cpp
    ecs/test_entity_allocator.cpp
    ecs/test_archetype.cpp
    ecs/test_archetype_storage.cpp
    ecs/test_resources.cpp
    ecs/test_events.cpp
    ecs/test_query.cpp
    ecs/test_world.cpp
    ecs/test_commands.cpp
    ecs/test_components.cpp
    core/test_logging.cpp
)

target_link_libraries(helios-tests
    PRIVATE
        helios-core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(helios-tests)
```

---

### Step 0.11: Create unit tests

- [ ] **Create file: `helios-rewrite/tests/core/test_logging.cpp`**

```cpp
// helios-rewrite/tests/core/test_logging.cpp
//
// Comprehensive tests for the Helios logging system.
//
#include <helios/core/logging.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ---- Test channels ----
HELIOS_DEFINE_LOG_CHANNEL(TestChannel);
HELIOS_DEFINE_LOG_CHANNEL(Renderer);
HELIOS_DEFINE_LOG_CHANNEL(Physics);
HELIOS_DEFINE_LOG_CHANNEL(Audio);
HELIOS_DEFINE_LOG_CHANNEL_WITH_LEVEL(VerboseChannel, helios::LogLevel::Debug);

// ---- Helper: read file contents ----
static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

// ---- Test fixture ----
class LoggingTest : public ::testing::Test {
protected:
    static constexpr const char* kTestLogDir = "/tmp/helios_log_test";

    void SetUp() override {
        // Clean up any previous test logs
        if (fs::exists(kTestLogDir)) {
            fs::remove_all(kTestLogDir);
        }
    }

    void TearDown() override {
        // Clean up test logs
        if (fs::exists(kTestLogDir)) {
            fs::remove_all(kTestLogDir);
        }
    }

    helios::LogConfig make_test_config() {
        helios::LogConfig config;
        config.log_directory = kTestLogDir;
        config.log_file_name = "test.log";
        config.enable_console_sink = false; // Don't pollute test output
        config.enable_file_sink = true;
        config.enable_ring_buffer_sink = true;
        config.ring_buffer_capacity = 64;
        config.max_file_size = 1024 * 1024; // 1 MB
        config.max_rotated_files = 2;
        config.default_level = helios::LogLevel::Trace;
        return config;
    }
};

// ---- Tests ----

TEST_F(LoggingTest, ConstructionSetsGlobalInstance) {
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    {
        helios::LogSystem log_system(make_test_config());
        ASSERT_NE(helios::LogSystem::instance(), nullptr);
        EXPECT_EQ(helios::LogSystem::instance(), &log_system);
    }

    // After destruction, global instance should be null
    EXPECT_EQ(helios::LogSystem::instance(), nullptr);
}

TEST_F(LoggingTest, MoveConstructionTransfersOwnership) {
    helios::LogSystem log_system(make_test_config());
    auto* original_ptr = &log_system;
    ASSERT_EQ(helios::LogSystem::instance(), original_ptr);

    helios::LogSystem moved(std::move(log_system));
    EXPECT_EQ(helios::LogSystem::instance(), &moved);
}

TEST_F(LoggingTest, MoveAssignmentTransfersOwnership) {
    helios::LogSystem log_system(make_test_config());

    auto config2 = make_test_config();
    config2.log_file_name = "test2.log";
    helios::LogSystem log_system2(config2);

    // log_system2 is now the global
    EXPECT_EQ(helios::LogSystem::instance(), &log_system2);

    log_system2 = std::move(log_system);
    EXPECT_EQ(helios::LogSystem::instance(), &log_system2);
}

TEST_F(LoggingTest, BasicLoggingToFile) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "Hello from test: {}", 42);
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);
    EXPECT_NE(contents.find("Hello from test: 42"), std::string::npos);
    EXPECT_NE(contents.find("TestChannel"), std::string::npos);
}

TEST_F(LoggingTest, AllLogLevels) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Trace, "trace msg");
    HELIOS_LOG(TestChannel, Debug, "debug msg");
    HELIOS_LOG(TestChannel, Info,  "info msg");
    HELIOS_LOG(TestChannel, Warn,  "warn msg");
    HELIOS_LOG(TestChannel, Error, "error msg");
    // Note: we don't test Fatal here because it calls std::abort()

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("trace msg"), std::string::npos);
    EXPECT_NE(contents.find("debug msg"), std::string::npos);
    EXPECT_NE(contents.find("info msg"),  std::string::npos);
    EXPECT_NE(contents.find("warn msg"),  std::string::npos);
    EXPECT_NE(contents.find("error msg"), std::string::npos);
}

TEST_F(LoggingTest, PerLevelMacros) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG_TRACE(TestChannel, "trace level");
    HELIOS_LOG_DEBUG(TestChannel, "debug level");
    HELIOS_LOG_INFO(TestChannel,  "info level");
    HELIOS_LOG_WARN(TestChannel,  "warn level");
    HELIOS_LOG_ERROR(TestChannel, "error level");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // In Debug builds (which tests use), all levels should be present
    // In Release, Trace and Debug would be stripped at compile time
    EXPECT_NE(contents.find("info level"),  std::string::npos);
    EXPECT_NE(contents.find("warn level"),  std::string::npos);
    EXPECT_NE(contents.find("error level"), std::string::npos);
}

TEST_F(LoggingTest, StructuredFieldsInOutput) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "Loaded texture: {}", "grass.png")
        .field("width", int64_t{1024})
        .field("height", int64_t{1024})
        .field("format", "BC7");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("Loaded texture: grass.png"), std::string::npos);
    EXPECT_NE(contents.find("width=1024"), std::string::npos);
    EXPECT_NE(contents.find("height=1024"), std::string::npos);
    EXPECT_NE(contents.find("format=BC7"), std::string::npos);
    // Fields should be separated by pipe
    EXPECT_NE(contents.find("|"), std::string::npos);
}

TEST_F(LoggingTest, StructuredFieldsBoolAndDouble) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "Config loaded")
        .field("vsync", true)
        .field("fullscreen", false)
        .field("gamma", 2.2);

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("vsync=true"), std::string::npos);
    EXPECT_NE(contents.find("fullscreen=false"), std::string::npos);
    EXPECT_NE(contents.find("gamma="), std::string::npos);
}

TEST_F(LoggingTest, MultipleChannels) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(Renderer, Info, "Renderer initialized");
    HELIOS_LOG(Physics,  Info, "Physics world created");
    HELIOS_LOG(Audio,    Info, "Audio device opened");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("Renderer"), std::string::npos);
    EXPECT_NE(contents.find("Physics"),  std::string::npos);
    EXPECT_NE(contents.find("Audio"),    std::string::npos);
}

TEST_F(LoggingTest, ChannelRuntimeFiltering) {
    helios::LogSystem log_system(make_test_config());

    // Disable Physics channel
    LogChannel_Physics.set_enabled(false);

    HELIOS_LOG(Renderer, Info, "renderer visible");
    HELIOS_LOG(Physics,  Info, "physics hidden");
    HELIOS_LOG(Audio,    Info, "audio visible");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("renderer visible"), std::string::npos);
    EXPECT_EQ(contents.find("physics hidden"), std::string::npos); // Should NOT appear
    EXPECT_NE(contents.find("audio visible"), std::string::npos);

    // Re-enable for other tests
    LogChannel_Physics.set_enabled(true);
}

TEST_F(LoggingTest, ChannelMinLevelFiltering) {
    helios::LogSystem log_system(make_test_config());

    // Set Renderer channel to only show Warn and above
    LogChannel_Renderer.set_level(helios::LogLevel::Warn);

    HELIOS_LOG(Renderer, Info,  "renderer info hidden");
    HELIOS_LOG(Renderer, Warn,  "renderer warn visible");
    HELIOS_LOG(Renderer, Error, "renderer error visible");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("renderer info hidden"), std::string::npos);
    EXPECT_NE(contents.find("renderer warn visible"), std::string::npos);
    EXPECT_NE(contents.find("renderer error visible"), std::string::npos);

    // Reset for other tests
    LogChannel_Renderer.set_level(helios::LogLevel::Trace);
}

TEST_F(LoggingTest, GlobalLevelFiltering) {
    helios::LogSystem log_system(make_test_config());

    log_system.set_global_level(helios::LogLevel::Error);

    HELIOS_LOG(TestChannel, Info,  "info hidden");
    HELIOS_LOG(TestChannel, Warn,  "warn hidden");
    HELIOS_LOG(TestChannel, Error, "error visible");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("info hidden"), std::string::npos);
    EXPECT_EQ(contents.find("warn hidden"), std::string::npos);
    EXPECT_NE(contents.find("error visible"), std::string::npos);
}

TEST_F(LoggingTest, RingBufferCapturesMessages) {
    helios::LogSystem log_system(make_test_config());

    for (int i = 0; i < 10; ++i) {
        HELIOS_LOG(TestChannel, Info, "ring buffer msg {}", i);
    }
    log_system.flush();

    auto messages = log_system.get_recent_messages(0);
    EXPECT_EQ(messages.size(), 10u);

    // Check that messages are in order
    for (int i = 0; i < 10; ++i) {
        std::string expected = "ring buffer msg " + std::to_string(i);
        bool found = false;
        for (const auto& msg : messages) {
            if (msg.find(expected) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Missing message: " << expected;
    }
}

TEST_F(LoggingTest, RingBufferRespectCapacity) {
    auto config = make_test_config();
    config.ring_buffer_capacity = 5;
    helios::LogSystem log_system(config);

    for (int i = 0; i < 20; ++i) {
        HELIOS_LOG(TestChannel, Info, "overflow msg {}", i);
    }
    log_system.flush();

    auto messages = log_system.get_recent_messages(0);
    // Ring buffer should only keep the last 5
    EXPECT_LE(messages.size(), 5u);

    // The most recent messages should be present
    bool found_last = false;
    for (const auto& msg : messages) {
        if (msg.find("overflow msg 19") != std::string::npos) {
            found_last = true;
            break;
        }
    }
    EXPECT_TRUE(found_last);
}

TEST_F(LoggingTest, RingBufferPartialRetrieval) {
    helios::LogSystem log_system(make_test_config());

    for (int i = 0; i < 10; ++i) {
        HELIOS_LOG(TestChannel, Info, "partial msg {}", i);
    }
    log_system.flush();

    auto messages = log_system.get_recent_messages(3);
    EXPECT_EQ(messages.size(), 3u);
}

TEST_F(LoggingTest, CrashContextDump) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info,  "before crash 1");
    HELIOS_LOG(TestChannel, Warn,  "before crash 2");
    HELIOS_LOG(TestChannel, Error, "before crash 3");
    log_system.flush();

    std::string crash_path = std::string(kTestLogDir) + "/test_crash_context.log";
    log_system.dump_crash_context(crash_path);

    std::string contents = read_file(crash_path);
    EXPECT_NE(contents.find("HELIOS CRASH CONTEXT"), std::string::npos);
    EXPECT_NE(contents.find("before crash 1"), std::string::npos);
    EXPECT_NE(contents.find("before crash 2"), std::string::npos);
    EXPECT_NE(contents.find("before crash 3"), std::string::npos);
}

TEST_F(LoggingTest, FormatStringWithMultipleArgs) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "pos=({}, {}, {}), vel={:.2f}", 1, 2, 3, 4.567);
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("pos=(1, 2, 3), vel=4.57"), std::string::npos);
}

TEST_F(LoggingTest, ThreadSafety) {
    helios::LogSystem log_system(make_test_config());

    constexpr int kThreadCount = 8;
    constexpr int kMessagesPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                HELIOS_LOG(TestChannel, Info, "thread {} msg {}", t, i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    log_system.flush();

    // Verify all messages made it to the ring buffer or file
    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // We should have kThreadCount * kMessagesPerThread lines
    // Just verify some representative messages
    EXPECT_NE(contents.find("thread 0 msg 0"), std::string::npos);
    EXPECT_NE(contents.find("thread 7 msg 99"), std::string::npos);
}

TEST_F(LoggingTest, AsyncMode) {
    auto config = make_test_config();
    config.async_mode = true;
    config.async_queue_size = 4096;
    config.async_thread_count = 1;
    helios::LogSystem log_system(config);

    EXPECT_TRUE(log_system.is_async());

    for (int i = 0; i < 50; ++i) {
        HELIOS_LOG(TestChannel, Info, "async msg {}", i);
    }

    // Give async thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log_system.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("async msg 0"), std::string::npos);
    EXPECT_NE(contents.find("async msg 49"), std::string::npos);
}

TEST_F(LoggingTest, NoSystemInstanceReturnsNullEntry) {
    // No LogSystem is active
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    // This should not crash -- returns a LogEntry with null logger
    HELIOS_LOG(TestChannel, Info, "no system active");
    // If we got here without crashing, the test passes
}

TEST_F(LoggingTest, BuiltInChannels) {
    helios::LogSystem log_system(make_test_config());

    // Core and App channels are always defined
    HELIOS_LOG(Core, Info, "core channel works");
    HELIOS_LOG(App,  Info, "app channel works");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("core channel works"), std::string::npos);
    EXPECT_NE(contents.find("app channel works"),  std::string::npos);
}

TEST_F(LoggingTest, ChannelWithDefaultLevel) {
    helios::LogSystem log_system(make_test_config());

    // VerboseChannel was defined with LogLevel::Debug as default
    HELIOS_LOG(VerboseChannel, Trace, "trace filtered by channel default");
    HELIOS_LOG(VerboseChannel, Debug, "debug passes channel default");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // Trace should be filtered by channel's default level of Debug
    EXPECT_EQ(contents.find("trace filtered by channel default"), std::string::npos);
    EXPECT_NE(contents.find("debug passes channel default"), std::string::npos);
}

TEST_F(LoggingTest, LogLevelStringConversion) {
    using helios::LogLevel;
    using helios::log_level_to_string;
    using helios::log_level_from_string;

    EXPECT_EQ(log_level_to_string(LogLevel::Trace), "Trace");
    EXPECT_EQ(log_level_to_string(LogLevel::Debug), "Debug");
    EXPECT_EQ(log_level_to_string(LogLevel::Info),  "Info");
    EXPECT_EQ(log_level_to_string(LogLevel::Warn),  "Warn");
    EXPECT_EQ(log_level_to_string(LogLevel::Error), "Error");
    EXPECT_EQ(log_level_to_string(LogLevel::Fatal), "Fatal");
    EXPECT_EQ(log_level_to_string(LogLevel::Off),   "Off");

    EXPECT_EQ(log_level_from_string("Trace"), LogLevel::Trace);
    EXPECT_EQ(log_level_from_string("debug"), LogLevel::Debug);
    EXPECT_EQ(log_level_from_string("Info"),  LogLevel::Info);
    EXPECT_EQ(log_level_from_string("warn"),  LogLevel::Warn);
    EXPECT_EQ(log_level_from_string("Error"), LogLevel::Error);
    EXPECT_EQ(log_level_from_string("fatal"), LogLevel::Fatal);
    EXPECT_EQ(log_level_from_string("Off"),   LogLevel::Off);
    EXPECT_EQ(log_level_from_string("garbage"), LogLevel::Trace); // default
}

TEST_F(LoggingTest, FlushWorks) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "pre-flush message");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);
    EXPECT_NE(contents.find("pre-flush message"), std::string::npos);
}

TEST_F(LoggingTest, TimestampAndThreadIdInOutput) {
    auto config = make_test_config();
    // Use a pattern that includes timestamp and thread ID
    config.file_pattern = "[%Y-%m-%d %H:%M:%S.%e] [tid %t] [%n] [%l] %v";
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "timestamp test");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // Should contain a timestamp pattern like [2026-04-07 ...]
    EXPECT_NE(contents.find("[202"), std::string::npos); // Year prefix
    EXPECT_NE(contents.find("[tid"), std::string::npos); // Thread ID
}

TEST_F(LoggingTest, DisabledRingBufferReturnsEmpty) {
    auto config = make_test_config();
    config.enable_ring_buffer_sink = false;
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "no ring buffer");
    log_system.flush();

    auto messages = log_system.get_recent_messages(0);
    EXPECT_TRUE(messages.empty());
}

TEST_F(LoggingTest, DisabledFileSinkNoFile) {
    auto config = make_test_config();
    config.enable_file_sink = false;
    config.enable_console_sink = false;
    // Only ring buffer
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "ring only");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    EXPECT_FALSE(fs::exists(log_path));

    // But ring buffer should have it
    auto messages = log_system.get_recent_messages(0);
    EXPECT_GE(messages.size(), 1u);
}

TEST_F(LoggingTest, SetChannelLevelViaSystem) {
    helios::LogSystem log_system(make_test_config());

    // Use the system API to set channel level
    helios::LogSystem::set_channel_level(LogChannel_TestChannel, helios::LogLevel::Error);

    HELIOS_LOG(TestChannel, Info, "filtered by system api");
    HELIOS_LOG(TestChannel, Error, "passes system api filter");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("filtered by system api"), std::string::npos);
    EXPECT_NE(contents.find("passes system api filter"), std::string::npos);

    // Reset
    helios::LogSystem::set_channel_level(LogChannel_TestChannel, helios::LogLevel::Trace);
}

TEST_F(LoggingTest, SetChannelEnabledViaSystem) {
    helios::LogSystem log_system(make_test_config());

    helios::LogSystem::set_channel_enabled(LogChannel_TestChannel, false);
    HELIOS_LOG(TestChannel, Error, "channel disabled");

    helios::LogSystem::set_channel_enabled(LogChannel_TestChannel, true);
    HELIOS_LOG(TestChannel, Info, "channel re-enabled");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("channel disabled"), std::string::npos);
    EXPECT_NE(contents.find("channel re-enabled"), std::string::npos);
}

TEST_F(LoggingTest, LogDirectoryCreated) {
    std::string nested_dir = std::string(kTestLogDir) + "/nested/deep";
    ASSERT_FALSE(fs::exists(nested_dir));

    auto config = make_test_config();
    config.log_directory = nested_dir;
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "nested dir test");
    log_system.flush();

    EXPECT_TRUE(fs::exists(nested_dir));
}

TEST_F(LoggingTest, NullLogEntryFieldsCompile) {
    // NullLogEntry should accept .field() calls and do nothing
    helios::NullLogEntry null_entry;
    null_entry.field("key", "value");
    null_entry.field("num", int64_t{42});
    null_entry.field("flag", true);
    null_entry.field("pi", 3.14);
    null_entry.field("big", uint64_t{999});

    // Rvalue chain
    helios::NullLogEntry{}
        .field("a", "b")
        .field("c", int64_t{1})
        .field("d", true);

    // If we got here, NullLogEntry API is correct
}
```

---

### Step 0.12: Create directory structure

- [ ] **Create required directories:**

```bash
mkdir -p helios-rewrite/helios-core/include/helios/core
mkdir -p helios-rewrite/helios-core/src/core
mkdir -p helios-rewrite/tests/core
```

---

### Step 0.13: Verify build

- [ ] **Build and run tests:**

```bash
cd helios-rewrite
cmake -B build -DHELIOS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ctest --output-on-failure
```

Expected: All existing ECS tests still pass. All new logging tests pass.

---

### Step 0.14: Commit

- [ ] **Commit:**

```bash
git add helios-rewrite/helios-core/include/helios/core/log_level.h \
        helios-rewrite/helios-core/include/helios/core/log_channel.h \
        helios-rewrite/helios-core/include/helios/core/log_entry.h \
        helios-rewrite/helios-core/include/helios/core/log_system.h \
        helios-rewrite/helios-core/include/helios/core/log_macros.h \
        helios-rewrite/helios-core/include/helios/core/logging.h \
        helios-rewrite/helios-core/src/core/log_system.cpp \
        helios-rewrite/tests/core/test_logging.cpp \
        helios-rewrite/helios-core/CMakeLists.txt \
        helios-rewrite/CMakeLists.txt \
        helios-rewrite/tests/CMakeLists.txt
git commit -m "feat(core): add comprehensive RAII logging system with channels, structured fields, and crash context"
```

---

## Summary of Files

| File | Purpose |
|------|---------|
| `helios-core/include/helios/core/log_level.h` | `LogLevel` enum, string conversion, compile-time active level define |
| `helios-core/include/helios/core/log_channel.h` | `LogChannel` struct, `HELIOS_DEFINE_LOG_CHANNEL` macro |
| `helios-core/include/helios/core/log_entry.h` | `LogEntry` RAII builder with `.field()` chaining, `NullLogEntry` |
| `helios-core/include/helios/core/log_system.h` | `LogSystem` class, `LogConfig`, sink management, crash context |
| `helios-core/include/helios/core/log_macros.h` | `HELIOS_LOG`, `HELIOS_LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL` macros, built-in channels |
| `helios-core/include/helios/core/logging.h` | Umbrella header |
| `helios-core/src/core/log_system.cpp` | `LogSystem` implementation |
| `tests/core/test_logging.cpp` | 28 unit tests covering all features |

## Design Summary

| Requirement | Solution |
|-------------|----------|
| RAII | Constructor creates sinks, destructor flushes and drops loggers. No Init()/Shutdown(). |
| spdlog-backed | All logging goes through spdlog loggers and sinks. FetchContent v1.15.0. |
| Not a singleton | `LogSystem` is a regular object. Global pointer set by constructor for macro convenience. |
| Channel system | `HELIOS_DEFINE_LOG_CHANNEL(Name)` creates `LogChannel_Name` global. Each channel maps to an spdlog logger sharing global sinks. |
| Verbosity levels | `LogLevel::Trace/Debug/Info/Warn/Error/Fatal/Off` mapped to spdlog levels. |
| Runtime filtering | `LogChannel::set_level()`, `LogChannel::set_enabled()`, `LogSystem::set_global_level()`. |
| Multiple sinks | Console (colored), rotating file (10MB x 5), ring buffer (1024 entries). All configurable. |
| Structured fields | `LogEntry::field(key, value)` chaining. Appended as `| key=value` on emit. `NullLogEntry` for stripped paths. |
| Async option | `LogConfig::async_mode = true` creates spdlog async_logger with thread pool. `overrun_oldest` policy. |
| Thread-safe | spdlog's `_mt` sinks + our mutex on logger map. |
| Compile-time stripping | `HELIOS_LOG_ACTIVE_LEVEL` define. `HELIOS_LOG_TRACE`/`HELIOS_LOG_DEBUG` expand to `NullLogEntry{}` when stripped. Set per CMake config. |
| Crash context | `handle_fatal()` dumps ring buffer to `logs/crash_context.log`, flushes, aborts. |
| Formatted output | fmt syntax via spdlog's bundled fmt: `"pos=({}, {})", x, y`. |
| Timestamp + thread ID | Default patterns include `%H:%M:%S.%e` (time) and `%t` (thread ID). |

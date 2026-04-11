#pragma once

#include "helios/core/log_level.h"

#include <spdlog/spdlog.h>
#include <spdlog/logger.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace helios {

// Forward-declare LogChannel to avoid circular includes.
struct LogChannel;

/// RAII log entry builder. Returned by HELIOS_LOG macros and LogChannel::log().
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
///
/// **Two emission paths:**
/// - spdlog path: constructed with a spdlog::logger* (used by LogSystem macros)
/// - standalone path: constructed with a LogChannel* (used for standalone channels)
class LogEntry {
public:
    /// Construct a log entry targeting a spdlog logger (convenience/macro path).
    LogEntry(spdlog::logger* logger, LogLevel level, std::string message)
        : m_logger(logger)
        , m_channel(nullptr)
        , m_level(level)
        , m_message(std::move(message))
        , m_owns(true)
    {}

    /// Construct a log entry targeting a standalone LogChannel (power layer path).
    LogEntry(LogChannel* channel, LogLevel level, std::string message)
        : m_logger(nullptr)
        , m_channel(channel)
        , m_level(level)
        , m_message(std::move(message))
        , m_owns(true)
    {}

    /// Move constructor -- transfers ownership of emission.
    LogEntry(LogEntry&& other) noexcept
        : m_logger(other.m_logger)
        , m_channel(other.m_channel)
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
            m_channel = other.m_channel;
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

    /// Attach a string field (const char* overload to prevent bool conversion).
    LogEntry& field(std::string_view key, const char* value) & {
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

    /// Attach a string field (rvalue chain, const char* overload).
    LogEntry&& field(std::string_view key, const char* value) && {
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
    /// Build the final message string with fields appended.
    [[nodiscard]] std::string build_final_message() const {
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
        return final_msg;
    }

    /// Actually send the message via the appropriate path.
    void emit();

    spdlog::logger* m_logger  = nullptr;
    LogChannel*     m_channel = nullptr;
    LogLevel        m_level   = LogLevel::Info;
    std::string     m_message;
    std::vector<std::pair<std::string, std::string>> m_fields;
    bool            m_owns    = false;
};

/// A no-op log entry that discards everything. Used when a channel is
/// disabled or the log level is filtered out, so .field() calls compile
/// but do nothing.
class NullLogEntry {
public:
    NullLogEntry& field(std::string_view, std::string_view) & { return *this; }
    NullLogEntry& field(std::string_view, const char*) &      { return *this; }
    NullLogEntry& field(std::string_view, int64_t) &          { return *this; }
    NullLogEntry& field(std::string_view, uint64_t) &         { return *this; }
    NullLogEntry& field(std::string_view, double) &           { return *this; }
    NullLogEntry& field(std::string_view, bool) &             { return *this; }

    NullLogEntry&& field(std::string_view, std::string_view) && { return std::move(*this); }
    NullLogEntry&& field(std::string_view, const char*) &&      { return std::move(*this); }
    NullLogEntry&& field(std::string_view, int64_t) &&          { return std::move(*this); }
    NullLogEntry&& field(std::string_view, uint64_t) &&         { return std::move(*this); }
    NullLogEntry&& field(std::string_view, double) &&           { return std::move(*this); }
    NullLogEntry&& field(std::string_view, bool) &&             { return std::move(*this); }
};

} // namespace helios

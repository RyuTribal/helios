// helios-rewrite/helios-core/src/helios/core/log_entry.h
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

// helios-rewrite/helios-core/src/helios/core/ring_buffer_sink.h
//
// In-memory ring buffer sink, usable independently of LogSystem.
//
#pragma once

#include "helios/core/log_sink.h"
#include "helios/core/log_level.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/ringbuffer_sink.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace helios {

/// A standalone in-memory ring buffer sink. Captures the last N messages
/// for querying (e.g., in-game console, crash dumps).
///
/// Usage:
///   auto sink = std::make_shared<RingBufferSink>(128);
///   sink->write(LogLevel::Info, "MyModule", "Hello");
///   auto msgs = sink->get_messages();
///
class RingBufferSink : public LogSink {
public:
    /// Construct with a given capacity (number of messages to keep).
    explicit RingBufferSink(std::size_t capacity = 1024)
        : m_spdlog_sink(std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(capacity))
    {
        m_spdlog_sink->set_level(spdlog::level::trace);
    }

    void write(LogLevel level, const std::string& channel_name,
               const std::string& message) override {
        auto spd_level = to_spdlog_level(level);
        spdlog::details::log_msg msg(channel_name, spd_level, message);
        m_spdlog_sink->log(msg);
    }

    void flush() override {
        m_spdlog_sink->flush();
    }

    /// Get the last N formatted messages. Pass 0 to get all buffered messages.
    [[nodiscard]] std::vector<std::string> get_messages(std::size_t count = 0) const {
        return m_spdlog_sink->last_formatted(count);
    }

    /// Access the underlying spdlog sink (for LogSystem integration).
    [[nodiscard]] std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> spdlog_sink() const {
        return m_spdlog_sink;
    }

private:
    static spdlog::level::level_enum to_spdlog_level(LogLevel level) {
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

    std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> m_spdlog_sink;
};

} // namespace helios

// helios-rewrite/helios-core/src/helios/core/console_sink.h
//
// Colored console sink, usable independently of LogSystem.
//
#pragma once

#include "helios/core/log_sink.h"
#include "helios/core/log_level.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <string>

namespace helios {

/// A standalone sink that writes colored output to the console (stdout).
/// Wraps spdlog's stdout_color_sink internally.
///
/// Usage:
///   auto sink = std::make_shared<ConsoleSink>();
///   sink->write(LogLevel::Info, "MyModule", "Hello world");
///
class ConsoleSink : public LogSink {
public:
    /// Construct with an optional output pattern.
    explicit ConsoleSink(std::string pattern = "[%H:%M:%S.%e] [%n] [%^%l%$] %v")
        : m_pattern(std::move(pattern))
        , m_spdlog_sink(std::make_shared<spdlog::sinks::stdout_color_sink_mt>())
    {
        m_spdlog_sink->set_pattern(m_pattern);
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

    /// Access the underlying spdlog sink (for LogSystem integration).
    [[nodiscard]] spdlog::sink_ptr spdlog_sink() const { return m_spdlog_sink; }

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

    std::string m_pattern;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> m_spdlog_sink;
};

} // namespace helios

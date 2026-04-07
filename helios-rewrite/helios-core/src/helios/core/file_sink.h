// helios-rewrite/helios-core/src/helios/core/file_sink.h
//
// Rotating file sink, usable independently of LogSystem.
//
#pragma once

#include "helios/core/log_sink.h"
#include "helios/core/log_level.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace helios {

/// A standalone sink that writes to rotating log files.
/// Wraps spdlog's rotating_file_sink internally.
///
/// Usage:
///   auto sink = std::make_shared<FileSink>("logs/my.log");
///   sink->write(LogLevel::Info, "MyModule", "Hello file");
///
class FileSink : public LogSink {
public:
    /// Construct a rotating file sink.
    /// @param file_path      Path to the log file.
    /// @param max_file_size  Max size in bytes before rotation. Default: 10 MB.
    /// @param max_files      Number of rotated files to keep. Default: 5.
    /// @param pattern        Output format pattern.
    explicit FileSink(const std::string& file_path,
                      std::size_t max_file_size = 10 * 1024 * 1024,
                      std::size_t max_files = 5,
                      std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [tid %t] [%n] [%l] %v")
        : m_pattern(std::move(pattern))
    {
        // Create parent directory if needed
        auto parent = std::filesystem::path(file_path).parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }

        m_spdlog_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file_path, max_file_size, max_files);
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
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> m_spdlog_sink;
};

} // namespace helios

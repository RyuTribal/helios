#pragma once

#include "helios/core/log_level.h"

#include <string>

namespace helios {

/// Abstract sink interface. Users can implement this to create custom sinks.
/// All concrete sinks (ConsoleSink, FileSink, RingBufferSink) implement this.
///
/// Usage (custom sink):
///   class MySink : public LogSink {
///   public:
///       void write(LogLevel level, const std::string& channel, const std::string& msg) override {
///           // your logic here
///       }
///       void flush() override { /* ... */ }
///   };
///
class LogSink {
public:
    virtual ~LogSink() = default;

    /// Write a log message. Called by LogChannel for each message that passes
    /// level filtering. Implementations must be thread-safe.
    virtual void write(LogLevel level, const std::string& channel_name,
                       const std::string& message) = 0;

    /// Flush any buffered output.
    virtual void flush() = 0;
};

} // namespace helios

#include "helios/core/log_entry.h"
#include "helios/core/log_channel.h"

namespace helios {

void LogEntry::emit() {
    std::string final_msg = build_final_message();

    // Path 1: spdlog logger (used by LogSystem / macros)
    if (m_logger) {
        switch (m_level) {
            case LogLevel::Trace: m_logger->trace("{}", final_msg);    break;
            case LogLevel::Debug: m_logger->debug("{}", final_msg);    break;
            case LogLevel::Info:  m_logger->info("{}", final_msg);     break;
            case LogLevel::Warn:  m_logger->warn("{}", final_msg);     break;
            case LogLevel::Error: m_logger->error("{}", final_msg);    break;
            case LogLevel::Fatal: m_logger->critical("{}", final_msg); break;
            default: break;
        }
        return;
    }

    // Path 2: standalone LogChannel with sinks (power layer)
    if (m_channel) {
        for (auto& sink : m_channel->sinks()) {
            sink->write(m_level, m_channel->name, final_msg);
        }
        return;
    }

    // Neither logger nor channel -- silently discard (null entry).
}

} // namespace helios

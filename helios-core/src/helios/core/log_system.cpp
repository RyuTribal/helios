#include "helios/core/log_system.h"
#include "helios/core/crash_handler.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/pattern_formatter.h>

#include "helios/core/assert.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace helios {

LogSystem* LogSystem::s_instance = nullptr;

LogSystem::LogSystem(const LogConfig& config)
    : m_config(config)
{
    if (m_config.enable_file_sink) {
        std::filesystem::path log_dir(m_config.log_directory);
        if (!std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }
    }

    create_sinks();

    if (m_config.async_mode) {
        m_thread_pool = std::make_shared<spdlog::details::thread_pool>(
            m_config.async_queue_size,
            m_config.async_thread_count
        );
    }

    s_instance = this;
    m_is_global_owner = true;

    install_crash_handlers();
}

LogSystem::~LogSystem() {
    remove_crash_handlers();
    flush();

    {
        std::lock_guard lock(m_loggers_mutex);
        for (auto& [name, logger] : m_loggers) {
            spdlog::drop(name);
        }
        m_loggers.clear();
    }

    m_thread_pool.reset();

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

void LogSystem::create_sinks() {
    m_sinks.clear();

    if (m_config.enable_console_sink) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(m_config.console_pattern);
        console_sink->set_level(to_spdlog_level(m_config.default_level));
        m_sinks.push_back(console_sink);
    }

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

    if (m_config.enable_ring_buffer_sink) {
        m_ring_buffer_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(
            m_config.ring_buffer_capacity
        );
        m_ring_buffer_sink->set_level(to_spdlog_level(m_config.default_level));
        m_sinks.push_back(m_ring_buffer_sink);
    }
}

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

    spdlog::register_logger(logger);

    return logger;
}

spdlog::logger* LogSystem::get_or_create_channel_logger(std::string_view channel_name) {
    std::string name_str(channel_name);

    {
        std::lock_guard lock(m_loggers_mutex);
        auto it = m_loggers.find(name_str);
        if (it != m_loggers.end()) {
            return it->second.get();
        }

        auto logger = create_logger(name_str);
        auto* raw_ptr = logger.get();
        m_loggers.emplace(std::move(name_str), std::move(logger));
        return raw_ptr;
    }
}

void LogSystem::set_global_level(LogLevel level) {
    m_config.default_level = level;
    auto spd_level = to_spdlog_level(level);

    std::lock_guard lock(m_loggers_mutex);
    for (auto& [name, logger] : m_loggers) {
        logger->set_level(spd_level);
    }

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

std::vector<std::string> LogSystem::get_recent_messages(std::size_t count) const {
    if (!m_ring_buffer_sink) return {};
    return m_ring_buffer_sink->last_formatted(count);
}

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
    std::cerr << "[FATAL] [" << channel_name << "] " << message << std::endl;
    dump_crash_context();

    spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
        logger->flush();
    });

    std::abort();
}

void LogSystem::flush() {
    std::lock_guard lock(m_loggers_mutex);
    for (auto& [name, logger] : m_loggers) {
        logger->flush();
    }
}

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

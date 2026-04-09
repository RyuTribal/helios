// helios-script/src/file_watcher.cpp
#include "file_watcher.h"

#include <algorithm>

namespace helios {

FileWatcher::FileWatcher(
    const std::filesystem::path& watch_dir,
    std::vector<std::string> extensions,
    Callback callback,
    std::chrono::milliseconds poll_interval)
    : m_watch_dir(watch_dir)
    , m_extensions(std::move(extensions))
    , m_callback(std::move(callback))
    , m_poll_interval(poll_interval)
{
    // Build initial snapshot of file times
    if (std::filesystem::exists(m_watch_dir)) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(m_watch_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            bool match = std::any_of(m_extensions.begin(), m_extensions.end(),
                                     [&](const auto& e) { return e == ext; });
            if (match) {
                m_file_times[entry.path().string()] = entry.last_write_time();
            }
        }
    }

    // Start watcher thread
    m_thread = std::jthread([this](std::stop_token st) { watch_loop(st); });
}

void FileWatcher::watch_loop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(m_poll_interval);
        if (stop_token.stop_requested()) break;

        if (!std::filesystem::exists(m_watch_dir)) continue;

        for (auto& entry : std::filesystem::recursive_directory_iterator(m_watch_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            bool match = std::any_of(m_extensions.begin(), m_extensions.end(),
                                     [&](const auto& e) { return e == ext; });
            if (!match) continue;

            auto path_str = entry.path().string();
            auto current_time = entry.last_write_time();

            auto it = m_file_times.find(path_str);
            if (it == m_file_times.end()) {
                // New file
                m_file_times[path_str] = current_time;
                m_callback(entry.path());
            } else if (it->second != current_time) {
                // Modified file
                it->second = current_time;
                m_callback(entry.path());
            }
        }
    }
}

} // namespace helios

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace helios {

/// Watches a directory for file modifications.
/// Uses std::jthread (auto-joins on destruction) and polls
/// std::filesystem::last_write_time at a configurable interval.
///
/// RAII: starts watching in constructor, stops in destructor.
class FileWatcher {
public:
    using Callback = std::function<void(const std::filesystem::path& changed_file)>;

    /// Start watching the given directory for files matching the extensions.
    /// callback is invoked from the watcher thread when a change is detected.
    ///
    /// @param watch_dir     Directory to watch (recursively).
    /// @param extensions    File extensions to monitor (e.g., {".cs", ".dll"}).
    /// @param callback      Called with the path of each changed file.
    /// @param poll_interval How often to check for changes (default 500ms).
    FileWatcher(const std::filesystem::path& watch_dir,
                std::vector<std::string> extensions,
                Callback callback,
                std::chrono::milliseconds poll_interval = std::chrono::milliseconds(500));

    ~FileWatcher() = default;  // std::jthread auto-joins

    // Non-copyable, movable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) noexcept = default;
    FileWatcher& operator=(FileWatcher&&) noexcept = default;

private:
    void watch_loop(std::stop_token stop_token);

    std::filesystem::path m_watch_dir;
    std::vector<std::string> m_extensions;
    Callback m_callback;
    std::chrono::milliseconds m_poll_interval;
    std::jthread m_thread;

    // Tracks last_write_time for each watched file
    std::unordered_map<std::string, std::filesystem::file_time_type> m_file_times;
};

} // namespace helios

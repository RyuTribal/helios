#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace helios {

/// Simple thread pool. Workers pull tasks from a shared FIFO queue.
/// Construction starts the threads; destruction joins them.
class ThreadPool {
public:
    /// Create a pool with `num_threads` workers.
    /// If 0, uses std::thread::hardware_concurrency() - 1 (at least 1).
    explicit ThreadPool(uint32_t num_threads = 0);

    /// Signals all workers to stop and joins them.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Enqueue a task. Returns a future that completes when the task finishes.
    std::future<void> submit(std::function<void()> task);

    /// Block until all currently submitted tasks have finished.
    void wait_idle();

    /// Number of worker threads.
    uint32_t thread_count() const { return static_cast<uint32_t>(m_workers.size()); }

private:
    void worker_loop();

    std::vector<std::thread>              m_workers;
    std::deque<std::packaged_task<void()>> m_tasks;
    std::mutex                            m_mutex;
    std::condition_variable               m_cv;
    std::atomic<bool>                     m_stop{false};

    // For wait_idle: track in-flight tasks
    std::atomic<uint32_t>                 m_in_flight{0};
    std::mutex                            m_idle_mutex;
    std::condition_variable               m_idle_cv;
};

} // namespace helios

#include "helios/ecs/thread_pool.h"

#include <algorithm>

namespace helios {

ThreadPool::ThreadPool(uint32_t num_threads) {
    if (num_threads == 0) {
        uint32_t hw = std::thread::hardware_concurrency();
        num_threads = (hw > 1) ? (hw - 1) : 1;
    }

    m_workers.reserve(num_threads);
    for (uint32_t i = 0; i < num_threads; ++i) {
        m_workers.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(m_mutex);
        m_stop.store(true, std::memory_order_release);
    }
    m_cv.notify_all();

    for (auto& w : m_workers) {
        if (w.joinable()) w.join();
    }
}

std::future<void> ThreadPool::submit(std::function<void()> task) {
    std::packaged_task<void()> pt(std::move(task));
    auto future = pt.get_future();

    m_in_flight.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard lock(m_mutex);
        m_tasks.push_back(std::move(pt));
    }
    m_cv.notify_one();

    return future;
}

void ThreadPool::wait_idle() {
    std::unique_lock lock(m_idle_mutex);
    m_idle_cv.wait(lock, [this] {
        return m_in_flight.load(std::memory_order_acquire) == 0;
    });
}

void ThreadPool::worker_loop() {
    while (true) {
        std::packaged_task<void()> task;

        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] {
                return m_stop.load(std::memory_order_acquire) || !m_tasks.empty();
            });

            if (m_stop.load(std::memory_order_acquire) && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop_front();
        }

        task();

        uint32_t prev = m_in_flight.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1) {
            // We were the last in-flight task -- wake anyone waiting in wait_idle
            m_idle_cv.notify_all();
        }
    }
}

} // namespace helios

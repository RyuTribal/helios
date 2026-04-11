#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>
#include "helios/ecs/thread_pool.h"

using namespace helios;

TEST(ThreadPool, BasicSubmit) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    auto f1 = pool.submit([&] { counter.fetch_add(1); });
    auto f2 = pool.submit([&] { counter.fetch_add(1); });

    f1.get();
    f2.get();

    EXPECT_EQ(counter.load(), 2);
}

TEST(ThreadPool, ManyTasks) {
    ThreadPool pool(4);
    constexpr int N = 1000;
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    futures.reserve(N);

    for (int i = 0; i < N; ++i) {
        futures.push_back(pool.submit([&] { counter.fetch_add(1); }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), N);
}

TEST(ThreadPool, WaitIdle) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool.submit([&] { counter.fetch_add(1); });
    }

    pool.wait_idle();
    EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPool, TasksActuallyRunInParallel) {
    ThreadPool pool(4);
    std::atomic<int> concurrent{0};
    std::atomic<int> max_concurrent{0};

    constexpr int N = 20;
    std::vector<std::future<void>> futures;
    futures.reserve(N);

    for (int i = 0; i < N; ++i) {
        futures.push_back(pool.submit([&] {
            int c = concurrent.fetch_add(1) + 1;

            // Track maximum observed concurrency
            int prev_max = max_concurrent.load();
            while (c > prev_max &&
                   !max_concurrent.compare_exchange_weak(prev_max, c)) {}

            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            concurrent.fetch_sub(1);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    // With 4 threads and 20 tasks each sleeping 10ms, we should see
    // concurrency > 1 at some point.
    EXPECT_GT(max_concurrent.load(), 1);
}

TEST(ThreadPool, DestructorJoins) {
    std::atomic<int> counter{0};

    {
        ThreadPool pool(2);
        for (int i = 0; i < 50; ++i) {
            pool.submit([&] { counter.fetch_add(1); });
        }
        // pool goes out of scope -- destructor must join all workers
    }

    EXPECT_EQ(counter.load(), 50);
}

TEST(ThreadPool, SingleThread) {
    ThreadPool pool(1);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool.submit([&] { counter.fetch_add(1); });
    }

    pool.wait_idle();
    EXPECT_EQ(counter.load(), 100);
}

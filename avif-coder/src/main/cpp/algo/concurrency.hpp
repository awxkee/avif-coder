#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <type_traits>

namespace concurrency {

    template<typename Function>
    struct function_traits;

    template<typename R, typename... Args>
    struct function_traits<R(Args...)> {
        using result_type = R;
    };

    template<typename Function, typename... Args>
    void parallel_for(const int numThreads, const int numIterations, Function &&func, Args &&... args) {
        static_assert(std::is_invocable_v<Function, int, Args...>, "func must take an int parameter for iteration id");

        std::vector<std::thread> threads;

        int segmentHeight = numIterations / numThreads;

        auto parallelWorker = [&](int start, int end) {
            for (int y = start; y < end; ++y) {
                {
                    std::invoke(func, y, std::forward<Args>(args)...);
                }
            }
        };

        if (numThreads > 1) {
            // Launch N-1 worker threads
            for (int i = 1; i < numThreads; ++i) {
                int start = i * segmentHeight;
                int end = (i + 1) * segmentHeight;
                if (i == numThreads - 1) {
                    end = numIterations;
                }
                threads.emplace_back(parallelWorker, start, end);
            }
        }

        int start = 0;
        int end = segmentHeight;
        if (numThreads == 1) {
            end = numIterations;
        }
        parallelWorker(start, end);

        // Join all threads
        for (auto &thread: threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    template<typename Function, typename... Args>
    void parallel_for_with_thread_id(const int numThreads, const int numIterations, Function &&func, Args &&... args) {
        static_assert(std::is_invocable_v<Function, int, int, Args...>, "func must take an int parameter for threadId, and iteration Id");

        std::vector<std::thread> threads;

        int segmentHeight = numIterations / numThreads;

        auto parallel_worker = [&](int threadId, int start, int end) {
            for (int y = start; y < end; ++y) {
                {
                    std::invoke(func, threadId, y, std::forward<Args>(args)...);
                }
            }
        };

        if (numThreads > 1) {
            // Launch N-1 worker threads
            for (int i = 1; i < numThreads; ++i) {
                int start = i * segmentHeight;
                int end = (i + 1) * segmentHeight;
                if (i == numThreads - 1) {
                    end = numIterations;
                }
                threads.emplace_back(parallel_worker, i, start, end);
            }
        }

        int start = 0;
        int end = segmentHeight;
        if (numThreads == 1) {
            end = numIterations;
        }
        parallel_worker(0, start, end);

        // Join all threads
        for (auto &thread: threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
}

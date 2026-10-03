//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the boundary tests of the concurrent structures share: threads
// released at one moment onto one boundary (the last element, an empty
// structure, a full ring), so that their operations meet there rather
// than one after another as the threads start
#pragma once

#include <atomic>
#include <thread>
#include <vector>

namespace together {
    // f(i) on n threads, i = 0..n-1, each spinning until all are started;
    // returns once every thread has returned
    template<class F>
    void run(int n, F f) {
        std::atomic<int> started = {0};
        std::vector<std::thread> threads;
        for (int i = 0; i < n; ++i) {
            threads.emplace_back([&, i] {
                started.fetch_add(1, std::memory_order_acq_rel);
                while (started.load(std::memory_order_acquire) < n) {
                    std::this_thread::yield();
                }
                f(i);
            });
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    // The rounds a boundary is met in: enough for the threads to meet at
    // it in some of them, few enough for a sanitizer's run
    constexpr int Rounds = 300;
}

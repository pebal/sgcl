//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Thread: the thread of the standard library under the interface's names.
// Its closure is unmanaged memory, so a Ptr it needs is captured by
// reference (to a frame that outlives it) or held through a RootPtr.
#pragma once

#include <chrono>
#include <thread>
#include <utility>

namespace Sgcl {
    class Thread {
    public:
        using InnerType = std::thread;
        using Id = std::thread::id;

        Thread() noexcept = default;

        template<class F, class... A>
        explicit Thread(F&& f, A&&... a)
        : _t(std::forward<F>(f), std::forward<A>(a)...) {
        }

        Thread(Thread&&) noexcept = default;
        Thread& operator=(Thread&&) noexcept = default;
        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;

        // A thread still joinable when destroyed ends the program, as the
        // standard's does
        ~Thread() = default;

        // Waits until the thread ends
        void Join() {
            _t.join();
        }

        // Lets the thread run on its own: it is no longer joinable
        void Detach() {
            _t.detach();
        }

        bool IsJoinable() const noexcept {
            return _t.joinable();
        }

        Id GetId() const noexcept {
            return _t.get_id();
        }

        void Swap(Thread& other) noexcept {
            _t.swap(other._t);
        }

        static unsigned HardwareConcurrency() noexcept {
            return std::thread::hardware_concurrency();
        }

        InnerType& Inner() noexcept {
            return _t;
        }

        const InnerType& Inner() const noexcept {
            return _t;
        }

    private:
        InnerType _t;
    };

    inline void swap(Thread& a, Thread& b) noexcept {
        a.Swap(b);
    }

    // The calling thread: std::this_thread under the interface's names
    struct ThisThread {
        static Thread::Id GetId() noexcept {
            return std::this_thread::get_id();
        }

        static void Yield() noexcept {
            std::this_thread::yield();
        }

        template<class Rep, class Period>
        static void SleepFor(const std::chrono::duration<Rep, Period>& d) {
            std::this_thread::sleep_for(d);
        }

        template<class Clock, class Duration>
        static void SleepUntil(const std::chrono::time_point<Clock, Duration>& t) {
            std::this_thread::sleep_until(t);
        }
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...


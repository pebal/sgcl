//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <chrono>

namespace sgcl::detail {
    class Timer {
    public:
        Timer() noexcept {
            reset();
        }

        void reset() noexcept {
            _clock = now();
        }

        double duration() noexcept {
            return duration(now());
        }

        float duration(const std::chrono::steady_clock::time_point& clock) noexcept {
            return std::chrono::duration<float, std::milli>(clock - _clock).count();
        }

        static std::chrono::steady_clock::time_point now() noexcept {
            return std::chrono::steady_clock::now();
        }

    private:
        std::chrono::steady_clock::time_point _clock;
    };
}

//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include <chrono>

namespace sgcl {
    namespace Priv {
        struct Timer {
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
}

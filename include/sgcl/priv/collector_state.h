//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include <atomic>

namespace sgcl {
    namespace Priv {
        struct Collector_state {
            inline static std::atomic<bool> aborted = {false};
            inline static std::atomic<bool> terminated = {true};
        };
    }
}

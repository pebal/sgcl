//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/collector.h"

namespace sgcl {
    struct collector {
        inline static void terminate() noexcept {
            Priv::Collector::terminate();
        }
    };
}

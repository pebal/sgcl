//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/collector.h"

namespace sgcl {
    struct collector {
        inline static int64_t live_object_count() {
            return Priv::Collector_instance().live_object_count();
        }

        inline static void force_collect(bool wait = false) noexcept {
            Priv::Collector_instance().force_collect(wait);
        }

        inline static void terminate() noexcept {
            Priv::Collector::terminate();
        }
    };
}

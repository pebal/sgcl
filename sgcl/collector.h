//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/collector.h"

namespace sgcl {
    class collector {
    public:
        using pause_guard = detail::Collector::PauseGuard;

        inline static size_t get_live_object_count() {
            return std::get<1>(get_live_objects()).size();
        }

        inline static std::tuple<pause_guard, std::vector<void*>> get_live_objects() {
            return detail::collector_instance().get_live_objects();
        }

        inline static bool force_collect(bool wait = false) noexcept {
            return detail::collector_instance().force_collect(wait);
        }

        inline static void terminate() noexcept {
            detail::Collector::terminate();
        }
    };
}

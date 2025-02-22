//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/collector.h"

namespace sgcl {
    class Collector {
    public:
        using PauseGuard = detail::Collector::PauseGuard;

        inline static size_t living_objects_number() {
            return detail::collector_instance().last_living_objects_number();
        }

        inline static std::tuple<PauseGuard, std::vector<void*>> living_objects() {
            return detail::collector_instance().living_objects();
        }

        inline static bool force_collect(bool wait = false) noexcept {
            return detail::collector_instance().force_collect(wait);
        }

        inline static void terminate() noexcept {
            detail::Collector::terminate();
        }
    };
}

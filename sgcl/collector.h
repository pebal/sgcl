//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/collector.h"
#include "unique_ptr.h"

namespace sgcl {
    struct collector {
        inline static int64_t live_objects_number() {
            return Priv::Collector_instance().live_objects_number();
        }

        inline static unique_ptr<tracked_ptr<void>[]> live_objects() {
            unique_ptr<tracked_ptr<void>[]> array;
            Priv::Collector_instance().live_objects((Priv::Unique_ptr<Priv::Tracked_ptr[]>&)array);
            return array;
        }

        inline static void force_collect(bool wait = false) noexcept {
            Priv::Collector_instance().force_collect(wait);
        }

        inline static void terminate() noexcept {
            Priv::Collector::terminate();
        }
    };
}

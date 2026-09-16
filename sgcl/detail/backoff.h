//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "os.h"

namespace sgcl::detail {
    // The wait between two attempts at a contended word (the backoff of
    // Herlihy and Shavit): one pause after the first failed
    // compare-exchange, twice as many after each next, up to
    // config::BackoffMax. Many threads at one word otherwise spend more
    // on their retries than on their operations; the backoff turns the
    // storm into near-serial exchanges. A local of the operation, so that
    // every operation starts at one pause.
    struct Backoff {
        unsigned pauses = 1;

        void operator()() noexcept {
            for (unsigned i = 0; i < pauses; ++i) {
                os::spin_pause();
            }
            if (pauses < config::BackoffMax) {
                pauses *= 2;
            }
        }
    };
}

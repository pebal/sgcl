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
    // every operation starts at one pause. Max caps the pauses:
    // config::BackoffMax for a word that is the whole structure (the
    // stack's head), less where a long pause leaves work undone that
    // others wait for (the channel's ring).
    template<unsigned Max = config::BackoffMax>
    struct Backoff {
        unsigned pauses = 1;

        void operator()() noexcept {
            for (unsigned i = 0; i < pauses; ++i) {
                os::spin_pause();
            }
            if (pauses < Max) {
                pauses *= 2;
            }
        }
    };
}

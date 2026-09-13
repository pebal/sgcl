//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "page.h"
#include "thread.h"

namespace sgcl::detail {
    // A root for a node a container touches after unlinking it: a word of
    // this frame that the conservative scan sees (its address escapes, so
    // it is in memory), the state the write barrier would set (the window
    // after the cycle's demotion), on a registered thread (the frame is
    // scanned only then). What a tracked_ptr local costs beyond this is
    // not needed here: the release ordering of its store (nothing is
    // published), the card (a frame is never carded).
    class Anchor {
    public:
        explicit Anchor(const void* p) noexcept
        : _p(const_cast<void*>(p)) {
            os::escape(&_p);
            ensure_thread_registered();
            if (p) {   // an empty range anchors nothing
                Page::set_state<State::Reachable>(p);
            }
        }

        Anchor(const Anchor&) = delete;
        Anchor& operator=(const Anchor&) = delete;

        // The next node of a walk: the same word, the state set anew.
        void reset(const void* p) noexcept {
            _p = const_cast<void*>(p);
            if (p) {
                Page::set_state<State::Reachable>(p);
            }
        }

        ~Anchor() noexcept {
            *(void* volatile*)&_p = nullptr;   // a dead frame keeps nothing
        }

    private:
        void* _p;
    };
}

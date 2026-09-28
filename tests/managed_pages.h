//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "tests/types.h"

#include <cstddef>
#include <typeinfo>

// What an operation costs in managed memory, for the tests that hold a call
// to allocating only its answer there (the audit of 2026-09-26: scratch of
// one call belongs in plain memory).
namespace heap_count {
    // The pages of managed memory `f` takes from the heap over `n` calls,
    // with the collector parked: nothing is swept meanwhile, so what one
    // call drops is not taken again by the next, and whatever a call leaves
    // behind shows as pages. One round of `n` calls goes first uncounted,
    // to use up the free slots earlier sweeps left in pages already taken.
    // A page is 64 KB, so `n` has to be large enough for the difference a
    // test looks for to come to several of them.
    template<class F>
    size_t pages_of(size_t n, F&& f) {
        using sgcl::detail::MemoryCounters;
        collector::clear_stack();
        collector::force_collect(true);
        collector::force_collect(true);
        size_t before = 0;
        size_t after = 0;
        {
            collector::stepper s(true);
            s.advance_to(collector::stepper::phase::start);
            off_frame([&] {
                for (size_t i = 0; i < n; ++i) {
                    f();
                }
            });
            before = MemoryCounters::alloc_since_cycle();
            off_frame([&] {
                for (size_t i = 0; i < n; ++i) {
                    f();
                }
            });
            after = MemoryCounters::alloc_since_cycle();
            s.finish_cycle();
            s.finish_cycle();
        }
        collector::clear_stack();
        collector::force_collect(true);
        return after - before;
    }

    // The live managed buffers whose elements are T: those of the library's
    // vectors of T, of a buffer made for T's. Live means reachable when the
    // count is taken, so the objects that hold them have to be held.
    template<class T>
    size_t live_buffers_of() {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (s.buffers && s.type && (*s.type == typeid(T) || *s.type == typeid(T[]))) {
                n += s.live_objects;
            }
        }
        return n;
    }

    // Everything unreachable swept, and the stack below the caller cleared
    // of words that could keep it, before a count
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

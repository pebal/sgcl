//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Whether a secret is anywhere in managed memory: the pages the heap has
// given out (live objects, the slots of objects not swept yet, the free
// slots sweeps left with their old bytes), read in place for the secret's
// bytes. For the tests of the rule that a secret (a private key, a
// password, a traffic secret) never lies in managed memory. Read only: the
// heap's own table of pages (detail::Heap: base, size, page_of_checked),
// nothing in the engine changed. The collector is parked for the whole of
// it (collector::stepper, as heap_count::pages_of), so that nothing a test
// does is swept and given out again, and no page is let go while it is
// read.
#pragma once

#include "tests/types.h"
#include "sgcl/core/detail/heap.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace managed_scan {
    using bytes_t = std::vector<uint8_t>;

    // How many of the patterns (each searched whole; a pattern across two
    // pages of one large object is found, the pages in use run together)
    // lie somewhere in the managed pages in use
    inline size_t patterns_found(const std::vector<bytes_t>& patterns, std::vector<size_t>* which = nullptr) {
        using sgcl::detail::Heap;
        const uintptr_t base = Heap::base();
        const size_t size = Heap::size();
        std::vector<bool> found(patterns.size(), false);
        auto search = [&](const uint8_t* first, const uint8_t* last) {
            for (size_t i = 0; i < patterns.size(); ++i) {
                const auto& p = patterns[i];
                if (!found[i] && !p.empty() && std::search(first, last, p.begin(), p.end()) != last) {
                    found[i] = true;
                }
            }
        };
        size_t run = 0;   // the start of the pages in use before this one, as an offset
        bool in_run = false;
        for (size_t off = 0; off < size; off += Heap::PageSize) {
            const bool used = Heap::page_of_checked(reinterpret_cast<const void*>(base + off)) != nullptr;
            if (used && !in_run) {
                run = off;
                in_run = true;
            } else if (!used && in_run) {
                search(reinterpret_cast<const uint8_t*>(base + run), reinterpret_cast<const uint8_t*>(base + off));
                in_run = false;
            }
        }
        if (in_run) {
            search(reinterpret_cast<const uint8_t*>(base + run), reinterpret_cast<const uint8_t*>(base + size));
        }
        if (which) {
            which->clear();
            for (size_t i = 0; i < found.size(); ++i) {
                if (found[i]) {
                    which->push_back(i);
                }
            }
        }
        return size_t(std::count(found.begin(), found.end(), true));
    }

    // `f` run with the collector parked, then the managed pages searched
    // for the patterns `f` leaves in `patterns` (it may add those it learns
    // as it runs: a traffic secret made in a handshake)
    template<class F>
    size_t found_after(std::vector<bytes_t>& patterns, F&& f, std::vector<size_t>* which = nullptr) {
        collector::clear_stack();
        collector::force_collect(true);
        collector::force_collect(true);
        size_t n = 0;
        {
            collector::stepper s(true);
            s.advance_to(collector::stepper::phase::start);
            off_frame([&] { f(); });
            n = patterns_found(patterns, which);
            s.finish_cycle();
            s.finish_cycle();
        }
        collector::clear_stack();
        collector::force_collect(true);
        return n;
    }
}

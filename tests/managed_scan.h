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
#include "sgcl/core/detail/os.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace managed_scan {
    using bytes_t = std::vector<uint8_t>;

    // The patterns not found yet looked for in [from, to); returns how
    // many are still not found. A function of its own: a lambda's body
    // would not take patterns_found's attribute
    SGCL_NO_SANITIZE inline size_t search_range(const uint8_t* from, const uint8_t* to, const size_t* first_of, const size_t* order_of,
                                                const uint8_t* const* data_of, const size_t* length_of, uint8_t* found_of, size_t left) {
        for (const uint8_t* at = from; at != to && left != 0; ++at) {
            const size_t b = *at;
            for (size_t k = first_of[b]; k != first_of[b + 1]; ++k) {
                const size_t i = order_of[k];
                const size_t n = length_of[i];
                if (found_of[i] || n > size_t(to - at)) {
                    continue;
                }
                const uint8_t* const q = data_of[i];
                size_t j = 1;
                while (j != n && at[j] == q[j]) {
                    ++j;
                }
                if (j == n) {
                    found_of[i] = 1;
                    --left;
                }
            }
        }
        return left;
    }

    // How many of the patterns (each searched whole; a pattern across two
    // pages of one large object is found, the pages in use run together)
    // lie somewhere in the managed pages in use. One pass over the pages:
    // at each byte the patterns starting with it (a table by the first
    // byte) compared by a plain loop. The read is racy by design, as the
    // collector's stack scan is: parking the collector does not stop the
    // other runtime threads, and the timer thread stores to its root cells
    // on these pages. Such a store may give the scan a stale byte, which is
    // irrelevant for the secrets: they are written before the scan starts
    // (the exchange's threads joined). Hence SGCL_NO_SANITIZE, and no
    // std::search, memcmp or memchr (instrumented or intercepted)
    SGCL_NO_SANITIZE inline size_t patterns_found(const std::vector<bytes_t>& patterns, std::vector<size_t>* which = nullptr) {
        using sgcl::detail::Heap;
        const uintptr_t base = Heap::base();
        const size_t size = Heap::size();
        const size_t count = patterns.size();
        // first[b] .. first[b + 1]: the indices in `order` of the non-empty
        // patterns that start with the byte b
        std::vector<size_t> first(257, 0);
        for (const auto& p : patterns) {
            if (!p.empty()) {
                ++first[size_t(p[0]) + 1];
            }
        }
        for (size_t b = 0; b < 256; ++b) {
            first[b + 1] += first[b];
        }
        std::vector<size_t> order(first[256]);
        std::vector<size_t> next(first.begin(), first.end() - 1);
        for (size_t i = 0; i < count; ++i) {
            if (!patterns[i].empty()) {
                order[next[patterns[i][0]]++] = i;
            }
        }
        std::vector<const uint8_t*> data(count);
        std::vector<size_t> length(count);
        std::vector<uint8_t> found(count, 0);
        for (size_t i = 0; i < count; ++i) {
            data[i] = patterns[i].data();
            length[i] = patterns[i].size();
        }
        const size_t* const first_of = first.data();
        const size_t* const order_of = order.data();
        const uint8_t* const* const data_of = data.data();
        const size_t* const length_of = length.data();
        uint8_t* const found_of = found.data();
        size_t left = order.size();   // the patterns not found yet
        auto search = [&](const uint8_t* from, const uint8_t* to) {
            left = search_range(from, to, first_of, order_of, data_of, length_of, found_of, left);
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
        size_t n = 0;
        if (which) {
            which->clear();
        }
        for (size_t i = 0; i < count; ++i) {
            if (found[i]) {
                ++n;
                if (which) {
                    which->push_back(i);
                }
            }
        }
        return n;
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

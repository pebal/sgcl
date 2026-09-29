//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The placement of the scheduler's objects moved on purpose, for A/B runs
// that must not measure where the heap happened to put them: with
// SGCL_BENCH_PAD_PAGES=k (0 to 3) the process takes k managed pages
// before anything else runs (one object of a type of its own each, so a
// page each, rooted for good), and the workers' queues and everything made
// after them land k pages further. The workers' rings on one page or the
// next moved chan task cap0 from 206 to 275 ns per item with no line of the
// scheduler changed (docs/sgcl/async/benchmarks.md, "placement"): an A/B
// of the scheduler or the channels runs each side at every placement and
// compares the medians. Only the benchmarks include this; the library
// never pads.
#pragma once

#include "sgcl/sgcl.h"

#include <cstdlib>

namespace bench {
    template<int N>
    struct PlacementPad {
        unsigned char bytes[64] = {};
    };

    template<int N>
    inline void placement_pad() {
        static auto* held = new sgcl::root_ptr<PlacementPad<N>>(sgcl::make_tracked<PlacementPad<N>>());
        (void)held;
    }

    inline int placement_pages() {
        const char* v = std::getenv("SGCL_BENCH_PAD_PAGES");
        int k = v ? std::atoi(v) : 0;
        if (k > 0) {
            placement_pad<0>();
        }
        if (k > 1) {
            placement_pad<1>();
        }
        if (k > 2) {
            placement_pad<2>();
        }
        return k;
    }

    // before main: the pages taken before the scheduler's first start
    inline const int placement_padded = placement_pages();
}

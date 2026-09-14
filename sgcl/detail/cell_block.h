//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "pointer.h"

namespace sgcl::detail {
    // The cells of the gc::tracked_ptrs that live in unmanaged memory
    // (gc/tracked_ptr.h), a line of the L1 cache of them to a block
    // (config.h: L1LineSize; 16 on a 128-byte line, 8 on a 64-byte one): a managed
    // object of nothing but pointers, made by the thread's cell allocator
    // (gc/tracked_ptr.h: CellAllocator) once per block and never freed by
    // a mutator. A free slot holds its own address, which no
    // pointer in use can hold (a word of a managed object addressing
    // itself), so a free slot is told from a null one without a word of
    // data in the block. The allocator hands out a slot zeroed; the
    // pointer stores into it through the barrier, as into any tracked
    // word, and its destructor puts the address back, on whatever thread
    // it runs. The block is in the unique state (a root, traced every
    // cycle) while the allocator may still hand out a slot of it; once it
    // may not (the last slot handed out, or the thread gone) the
    // allocator sets the block's state to UniqueReleased (types.h), and
    // from then on the collector frees the block in the cycle that finds
    // every slot free (collector.h: _release_cell_blocks). The collector
    // reads the words of a block without the type's pointer map: it knows
    // them all to be pointers (collector.h: _mark_cell_block).
    struct CellBlock {
        static constexpr unsigned Slots = config::L1LineSize / sizeof(void*);

        CellBlock() noexcept {
            for (auto& s : slots) {
                s.store_no_update(&s);
            }
        }

        static bool is_free(const Pointer& s) noexcept {
            return s.load() == &s;
        }

        bool all_free() const noexcept {
            for (auto& s : slots) {
                if (!is_free(s)) {
                    return false;
                }
            }
            return true;
        }

        Pointer slots[Slots];
    };

    static_assert(sizeof(CellBlock) == config::L1LineSize);
}

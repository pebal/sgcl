//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../make_tracked.h"
#include "cell_block.h"
#include "page.h"
#include "pointer.h"

namespace sgcl::detail {
    // The thread's source of cells for the root_ptrs (root_ptr.h), the
    // roots in unmanaged memory (cell_block.h): a block of a cache line
    // of slots, handed out in order, each zeroed as it goes. The block is
    // a root by its unique state while any slot of it may still be handed
    // out; the last slot handed out, or the thread ending, sets the block
    // UniqueReleased (types.h), with release after the last zeroing, and
    // the collector frees it in the cycle that finds every slot free
    // again. The root_ptr's destructor frees its slot on whatever thread
    // it runs, so a slot handed out here may be freed by another thread,
    // and a block is held by nothing but its slots. take() is called on a
    // registered thread (root_ptr registers it first).
    struct CellAllocator {
        CellBlock* block = nullptr;
        unsigned index = 0;

        ~CellAllocator() noexcept {
            release();
        }

        // The next cell of the block, zeroed, a block made when there is
        // none; the block let go of after its last cell
        Pointer* take() noexcept {
            if (!block) [[unlikely]] {
                block = make_tracked<CellBlock>().release();
                index = 0;
            }
            auto slot = (Pointer*)&block->slots[index];   // the word, a tracked word from here on
            slot->store(nullptr);
            if (++index == CellBlock::Slots) [[unlikely]] {
                release();
            }
            return slot;
        }

        // A cell given back, from any thread: free again, its own address
        // (cell_block.h)
        static void free(Pointer* cell) noexcept {
            assert(!CellBlock::is_free(cell));
            cell->store_no_update(cell);
        }

        // The block let go of: its state, after the slots handed out so far
        void release() noexcept {
            if (block) {
                Page::set_state<State::UniqueReleased>(block);
                block = nullptr;
            }
        }
    };

    inline thread_local CellAllocator cell_allocator;
}

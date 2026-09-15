//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "os.h"

#include <atomic>

namespace sgcl::detail {
    // The cell behind a weak_ptr: a managed object of its own type holding
    // the target as a word the collector does not trace (type_info.h:
    // MayContainTracked<WeakCell> is false, so the type's pointer map is
    // empty). The cells of a program lie on the pages of this one type,
    // which is how the collector finds them (collector.h:
    // _clear_weak_cells): a live cell whose target a cycle found
    // unreachable has the word cleared before the sweep. A weak_ptr is a
    // tracked_ptr to a cell; copies share it, and the cell is collected
    // with the last of them. Constructed inside the allocator's init,
    // before its slot is published (maker.h: make_tracked_before_publish),
    // so the collector never sees a cell half-written or the word of the
    // slot's last occupant. The word, while it holds an address, names
    // a live object and nothing else: the clearing precedes the sweep
    // that frees the object's slot, and the slot cannot be handed out
    // again before that. The weak containers (detail/weak_table.h)
    // hash and compare their keys by the word, read without a lock.
    // A cell watched by an expiry_queue (expiry_queue.h) carries flags: the
    // collector, finding the target of a Watched cell unreachable, does not
    // clear it but marks the target reachable and sets Expired, and keeps
    // the target alive from then on, cycle after cycle, until the queue
    // drains the entry and sets Drained; from then on the cell is an
    // ordinary one and the target dies with the next cycle that finds it
    // unreachable, unless the queue's function kept it.
    struct WeakCell {
        enum Flags : unsigned {
            Watched = 1,   // an expiry_queue holds an entry for this cell
            Expired = 2,   // the collector found the target unreachable and kept it for the queue
            Drained = 4    // the queue has run its function: no longer kept
        };

        explicit WeakCell(void* p, unsigned f = 0) noexcept
        : target(p)
        , flags(f) {
        }

        // The word and the flags as the collector's weak phase reads them
        // (collector.h: _for_each_weak_cell). A cell is written by its
        // constructor before its slot is published, ordered before these
        // reads by the release store of the state and the acquire fence
        // the collector has passed since; the sanitizers do not follow
        // that fence and would report the constructor's stores against
        // the reads, so the reads are hidden from them (os.h: load_word).
        SGCL_NO_SANITIZE void* collector_target() const noexcept {
            auto p = (void*)os::load_word(&target);
            std::atomic_thread_fence(std::memory_order_acquire);
            return p;
        }

        SGCL_NO_SANITIZE unsigned collector_flags() const noexcept {
            auto f = *(const volatile unsigned*)&flags;
            std::atomic_thread_fence(std::memory_order_acquire);
            return f;
        }

        std::atomic<void*> target;
        std::atomic<unsigned> flags;
    };
}

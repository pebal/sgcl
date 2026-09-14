//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "os.h"
#include "pointer.h"

#include <cstdint>
#include <type_traits>
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace sgcl::detail {
    // The cells of the gc::tracked_ptrs that live in unmanaged memory
    // (gc/tracked_ptr.h), a line of the L1 cache of them to a block
    // (config.h: L1LineSize; 16 on a 128-byte line, 8 on a 64-byte one): a
    // managed object of nothing but pointers, made by the thread's cell
    // allocator (gc/tracked_ptr.h: CellAllocator) once per block and never
    // freed by a mutator. A free slot holds its own address, which no
    // pointer in use can hold (a word of a managed object addressing
    // itself), so a free slot is told from a null one without a word of
    // data in the block. The block has no constructor: a page issued to
    // this type is filled with the addresses of its words once, on the
    // mutator that takes it (fill_page, from the pool allocator), and
    // from then on the words keep the state on their own: the allocator
    // hands out a slot zeroed, the pointer stores into it through the
    // barrier, as into any tracked word, and its destructor puts the
    // address back, on whatever thread it runs; a block the collector
    // frees has every word free, so the slot comes back to the allocator
    // as a block of free cells without a store. The block is in the
    // unique state (a root, traced every cycle) while the allocator may
    // still hand out a slot of it; once it may not (the last slot handed
    // out, or the thread gone) the allocator sets the block's state to
    // UniqueReleased (types.h), and from then on the collector frees the
    // block in the cycle that finds every slot free (collector.h:
    // _release_cell_blocks). The marking traces a block by the type's
    // pointer map like any object: the map is full and stays so, every
    // word a heap address, a free cell's its own, which marks the block
    // itself, a root already (a branch in the trace for the blocks alone
    // cost every object of every type 15% of the marking).
    // The words are plain, so that the type is trivial and make_tracked's
    // default-initialization leaves them as they are; the collector reads
    // them as the atomic words they are used as (a slot in use is a
    // detail::Pointer, the same word).
    struct CellBlock {
        static constexpr unsigned Slots = config::L1LineSize / sizeof(void*);

        static uintptr_t word(const uintptr_t& s) noexcept {
            return os::load_word(&s);   // written by other threads (the pointers, the allocator): a volatile read, as of any word the collector reads
        }

        static bool is_free(const void* slot) noexcept {
            return word(*(const uintptr_t*)slot) == (uintptr_t)slot;
        }

        bool all_free() const noexcept {
            for (auto& s : slots) {
                if (word(s) != (uintptr_t)&s) {
                    return false;
                }
            }
            return true;
        }

        // Every word of a page of blocks its own address: on arm64 four
        // words a step as two NEON vectors advanced by a constant (the
        // compiler's own vectorization of the plain loop rebuilds the
        // vectors from scalar registers every step); elsewhere the loop,
        // which the compiler vectorizes as it can
        static void fill_page(void* data, size_t bytes) noexcept {
            auto words = (uintptr_t*)data;
            auto count = bytes / sizeof(uintptr_t);
#if defined(__aarch64__) && defined(__ARM_NEON)
            const uint64x2_t two = vdupq_n_u64(2 * sizeof(uintptr_t));
            const uint64x2_t four = vdupq_n_u64(4 * sizeof(uintptr_t));
            uint64x2_t v = {(uint64_t)words, (uint64_t)(words + 1)};
            for (size_t i = 0; i < count; i += 4) {
                vst1q_u64((uint64_t*)(words + i), v);
                vst1q_u64((uint64_t*)(words + i + 2), vaddq_u64(v, two));
                v = vaddq_u64(v, four);
            }
#else
            for (size_t i = 0; i < count; ++i) {
                words[i] = (uintptr_t)(words + i);
            }
#endif
        }

        uintptr_t slots[Slots];
    };

    static_assert(sizeof(CellBlock) == config::L1LineSize);
    static_assert(std::is_trivially_default_constructible_v<CellBlock> && std::is_trivially_destructible_v<CellBlock>);
}

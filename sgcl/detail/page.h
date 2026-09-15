//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "heap.h"
#include "metadata.h"

#include <cassert>
#include <cstring>

namespace sgcl::detail {
    struct alignas(config::CacheLineSize) Page {
        template<class T>
        using Info = TypeInfo<T>;

        using Flag = uint64_t;
        static constexpr unsigned FlagBitCount = sizeof(Flag) * 8;

        // The collector's bits, one word of each per 64 slots. The free
        // bitmap of the allocator (free_bits) is an array of its own behind
        // them: the owning mutator writes it while it allocates, and on
        // these lines only the collector writes.
        struct Flags {
            Flag registered = {0};
            Flag reachable = {0};
            Flag marked = {0};
        };

        // The header lives outside its page: `data` is the first byte of the
        // page (or of the page range of a large object) in the heap.
        template<class T>
        Page(T* data) noexcept
        : metadata(&Info<T>::private_metadata())
        , data((uintptr_t)data)
        // A page with a single object (large objects included) maps every
        // interior pointer to index 0: multiplier 0 does that for free.
        , multiplier(metadata->object_count == 1 ? 0 : (1ull << 32 | 0x10000) / metadata->object_size)
        , object_size(metadata->object_size)
        , object_count(metadata->object_count)
        , is_array(metadata->is_array)
        , flags_ptr((Flags*)((uintptr_t)states() + ((sizeof(std::atomic<State>) * metadata->object_count + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1)))) {
            assert(metadata != nullptr);
            assert(data != nullptr);
            std::memset(this->states(), State::Reserved, object_count);
            std::memset(this->flags(), 0, sizeof(Flags) * this->flags_count());
            set_all_free();
        }

        // Every slot free: a fresh page, or the rebuild in the collector.
        void set_all_free() noexcept {
            auto free = this->free_bits();
            auto count = flags_count();
            auto objects = object_count;
            for (unsigned i = 0; i < count; ++i) {
                auto valid = (i == count - 1 && objects % FlagBitCount) ? (Flag(1) << (objects % FlagBitCount)) - 1 : ~Flag(0);
                free[i] = valid;
            }
            auto summary = this->summary();
            auto words = summary_count();
            for (unsigned w = 0; w < words; ++w) {
                auto valid = (w == words - 1 && count % 64) ? (uint64_t(1) << (count % 64)) - 1 : ~uint64_t(0);
                summary[w] = valid;
            }
        }

        // Slot allocation bitmap, a word per Flags word: bit set = slot free.
        // Written by the owning mutator only (owned == true); the collector
        // rebuilds it from the states before handing the page to the
        // per-type buffer.
        Flag* free_bits() const noexcept {
            return (Flag*)(flags() + flags_count());
        }

        // One bit per Flags word: set when that word has a free slot.
        uint64_t* summary() const noexcept {
            return (uint64_t*)(free_bits() + flags_count());
        }

        unsigned summary_count() const noexcept {   // words of the summary
            return (flags_count() + 63) / 64;
        }

        ~Page() noexcept {
            if constexpr(!std::is_trivially_destructible_v<std::atomic<State>>) {
                auto states = this->states();
                std::destroy(states, states + metadata->object_count);
            }
        }

        // What follows the header (page_info.h: HeaderSize): the states, one
        // byte per slot, then the Flags, the free bitmap and its summary
        std::atomic<State>* states() const noexcept {
            return (std::atomic<State>*)(this + 1);
        }

        Flags* flags() const noexcept {
            return flags_ptr;
        }

        unsigned flags_count() const noexcept {
            return (object_count + FlagBitCount - 1) / FlagBitCount;
        }

        // Start of a cycle. A full cycle forgets every mark; a young cycle
        // keeps them (an object marked once stays marked until the next full
        // cycle).
        void clear_flags(bool full) noexcept {
            auto flags = this->flags();
            auto count = flags_count();
            // The reachable bits are clear after every cycle: the marking
            // takes every bit it is given (collector.h: _mark_page). A full
            // cycle clears the marks; a young cycle reads nothing here.
            if (full) {
                for (unsigned i = 0; i < count; ++i) {
                    flags[i].reachable = 0;
                    flags[i].marked = 0;
                }
            } else {
#if !defined(NDEBUG)
                for (unsigned i = 0; i < count; ++i) {
                    assert(flags[i].reachable == 0 && "a reachable bit left behind by the marking");
                }
#endif
            }
        }

        // Card marking for the young cycles. A pointer stored into an object
        // stamps the card of the object's page with the current epoch (a
        // cycle number the collector advances at the start of every cycle;
        // heap.h: stamp_card). A young cycle retraces the objects of every
        // page stamped with the previous epoch or the current one, so that a
        // pointer from an already marked object to a young one is followed
        // although the marked object is not traced again. The collector
        // never clears a stamp, it only advances the epoch: no store from a
        // mutator can be lost to a clear. Pointers held on the stacks need
        // no card: the stacks are scanned every cycle.
        static void mark_card(const void* location) noexcept {
            if constexpr(!config::Generational) {
                return;
            }
            // Fast path for the stack: a location within ChunkSize of this
            // frame is not an object, because the heap's range keeps a
            // guard chunk at both ends (heap.h). No memory is read; without
            // it a copy onto the stack pays the card's reads (1.34 to 1.69
            // ns measured). A location farther up a stack stamps a card
            // nothing reads (heap.h: card_of covers every address).
            char probe;
            auto distance = (intptr_t)((const char*)location - &probe);
            if ((uintptr_t)(distance + (intptr_t)config::ChunkSize) < 2 * config::ChunkSize) {
                return;
            }
            Heap::mark_card(location);
        }

        // The page (or its range) holds a pointer stored during the epoch
        // before `e` or later.
        bool dirty_since(uint32_t e) const noexcept {
            return Heap::dirty_since_last((const void*)data, page_count, e);
        }

        // The Flags word of slot i, and its bit in the word
        static constexpr unsigned flag_index_of(unsigned i) noexcept {
            return i / FlagBitCount;
        }

        static constexpr Flag flag_mask_of(unsigned i) noexcept {
            return Flag(1) << (i % FlagBitCount);
        }

        // The slot of an address in the page (an interior one too): a
        // multiply by the reciprocal of the object size, no division
        unsigned index_of(const void* p) noexcept {
            assert(p != nullptr);
            return ((uintptr_t)p - data) * multiplier >> 32;
        }

        void* pointer_of(unsigned index) noexcept {
            return (void*)(data + index * object_size);
        }

        // The header of the page an address of the heap is in (heap.h)
        static Page* page_of(const void* p) noexcept {
            assert(p != nullptr);
            return Heap::page_of(p);
        }

        // Headers live in a per-type slab; never `delete` one.
        static void release(Page* page) noexcept {
            auto slab = page->metadata->header_slab;
            page->~Page();
            slab->free(page);
        }

        size_t data_size() const noexcept {   // the page, or the range of a large object
            return page_count * config::PageSize;
        }

        // The type's metadata, and the object's first byte, of any address
        // into a managed object
        static Metadata& metadata_of(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            return *page->metadata;
        }

        static void* base_address_of(const void* p) noexcept {
            assert(p != nullptr);
            auto page = page_of(p);
            auto index = page->index_of(p);
            return page->pointer_of(index);
        }

        // The state of the object at p: UniqueLock or BadAlloc from the
        // makers (the page's object_created raised for the registration),
        // Reachable from the write barrier, Destroyed from a unique_ptr's
        // deleter and from a container's erase (slot.h).
        template<State S>
        static void set_state(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto index = page->index_of(p);
            auto &state = page->states()[index];
            if constexpr(S == State::UniqueLock
                      || S == State::BadAlloc) {
                state.store(S, std::memory_order_relaxed);
                page->object_created.store(true, std::memory_order_release);
            } else if constexpr(S == State::Reachable) {
                // Write barrier: Reachable with the parity of the current
                // epoch. Both stores are conditional: the states line is
                // shared by 64 objects and the flag by a whole page, so an
                // unconditional store from every thread copying a pointer to
                // the same page keeps those lines bouncing between cores. A
                // skipped flag store only delays the collector's look at an
                // already reachable object by a cycle (the collector clears
                // the flag with an acq_rel exchange), never its safety.
                // Two hot paths: a copy of a pointer to an object whose
                // state is current (the first test), and to one still
                // Fresh, made since the last cycle (the second test, taken
                // for every young object). The store is once per object
                // per cycle. Nothing else in here: an expression the
                // compiler hoists ahead of the tests costs every copy
                // (measured: a select with four operations, 1.4 -> 2.1 ns),
                // which is why the transition out of UniqueLock is not
                // here but in set_state_released, the barrier of the paths
                // that release an object from its unique_ptr; a unique
                // object never reaches this one.
                auto wanted = reachable_state();
                auto old = state.load(std::memory_order_relaxed);
                assert(!is_unique_state(old) && "an object leaves its unique_ptr through store_released, never through this barrier");
                if (old != wanted) [[unlikely]] {
                    if (old != State(wanted | State::Fresh)) {
                        state.store(wanted, std::memory_order_relaxed);
                    }
                }
                if (!page->state_updated.load(std::memory_order_relaxed)) {
                    page->state_updated.store(true, std::memory_order_release);
                }
            } else {
                state.store(S, std::memory_order_release);
            }
        }

        // The write barrier for the first store of an object released
        // from its unique_ptr (detail/pointer.h: store_released): the
        // state from UniqueLock, with the parity the allocator read
        // (unique_state), to Reachable with the current parity and Fresh,
        // the state of an object made after the flip that the cycle leaves
        // alone (collector.h: _register_page), unless the allocation's
        // parity is not the current one: the object was allocated before
        // the flip, or with an epoch the thread had not seen yet, and the
        // stores of its constructor may have read the old epoch, so it gets
        // the current state without Fresh: registered by the cycle like any
        // object made before the flip, reachable by state should the
        // thread's stack have been scanned already. Once per object, on a
        // path of its own, so the select costs the ordinary barrier nothing.
        static void set_state_released(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto index = page->index_of(p);
            auto& state = page->states()[index];
            auto wanted = reachable_state();
            auto old = state.load(std::memory_order_relaxed);
            assert(is_unique_state(old));
            state.store((old ^ wanted) & State::Parity ? wanted : State(wanted | State::Fresh), std::memory_order_relaxed);
            if (!page->state_updated.load(std::memory_order_relaxed)) {
                page->state_updated.store(true, std::memory_order_release);
            }
        }

        // The state of the object at p, as it is now
        static State state_of(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            return page->states()[page->index_of(p)].load(std::memory_order_acquire);
        }

        // The object dies in the sweep in progress: registered and not
        // marked by the cycle. Meaningful on a sweeping thread only
        // (thread.h: sweeping), where the flags are frozen until every run
        // of the sweep is done; during marking the bit is transient.
        static bool dying(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto index = page->index_of(p);
            auto& flag = page->flags()[flag_index_of(index)];
            auto mask = flag_mask_of(index);
            return (flag.registered & mask) && !(flag.marked & mask);
        }

        // `p` points into a created object of the managed heap (a base
        // subobject, a member, the object itself), not into a buffer: the
        // target a tracked_ptr may be made from a raw pointer (tracked_ptr.h).
        static bool is_object(const void* p) noexcept {
            auto page = Heap::page_of_checked(p);
            if (!page || page->is_array) {
                return false;
            }
            auto index = page->index_of(p);
            return index < page->object_count && !(page->states()[index].load(std::memory_order_acquire) & State::FreeMask);
        }

        // Whether a unique_ptr owns the object at p (the debug assertions:
        // no tracked_ptr or weak_ptr may address it)
        static bool is_unique(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto index = page->index_of(p);
            auto &state = page->states()[index];
            return is_unique_state(state.load(std::memory_order_acquire));
        }

        // The header is two 64-byte lines. The first holds what the mutators
        // read on every barrier (data, multiplier) and the flags they write
        // (state_updated, card, object_created, owned, on_empty_list); the
        // second what the collector writes while it works (its lists, the
        // page's marks). The states follow at CacheLineSize, on lines of
        // their own, so that a collector's write to the header never
        // invalidates the line a barrier is storing a state into.
        Metadata* const metadata;
        const uintptr_t data;
        const uint64_t multiplier;
        // copies of the metadata's constants the passes read at every slot
        // (index_of, pointer_of, flags): on this line, one hop less
        const size_t object_size;
        const uint32_t object_count;
        const bool is_array;
        Flags* const flags_ptr;
        size_t page_count = 1;   // > 1 for objects larger than a page
        std::atomic_bool object_created = {false};
        std::atomic_bool state_updated = {false};
        std::atomic_bool on_empty_list = {false};
        // true while a pool allocator hands out slots from the page: then only
        // it touches the free bitmap, and the collector leaves the page alone.
        // Pages of large objects are never owned.
        std::atomic_bool owned = {false};

        // The state the barrier sets in the current epoch, and the collector
        // takes for reachable in the current cycle. A read of it just
        // before the flip followed by a store after it leaves the old
        // parity: harmless for an object that existed before the flip,
        // which the cycle registers and traces, and harmless for one made
        // after it, whose registration the allocation's parity decides
        // (unique_state, set_state<Reachable>).
        static State reachable_state() noexcept {
            return Heap::globals.current_reachable.load(std::memory_order_relaxed);
        }

        // The state a slot is handed out in: UniqueLock with the parity of
        // the epoch as this thread sees it, read with acquire so that the
        // flip (a release) is ordered before everything the object's
        // creator publishes it with, and a thread that got the object from
        // there reads the new epoch in its stores (set_state<Reachable>).
        static State unique_state() noexcept {
            return State(State::UniqueLock | (Heap::globals.current_reachable.load(std::memory_order_acquire) & State::Parity));
        }

        static bool is_unique_state(State s) noexcept {   // UniqueLock of either parity
            return State(s & ~State::Parity) == State::UniqueLock;
        }

        static void flip_epoch(uint32_t e) noexcept {
            Heap::globals.epoch.store(e, std::memory_order_relaxed);
            Heap::set_epoch(e);
            Heap::globals.current_reachable.store(State(State::Reachable | ((e & 1) << 6)), std::memory_order_release);
        }

        // slots freed by the collector since the page was last handed out
        alignas(64) uint16_t unused_counter_gc = {0};   // at most the objects of one page
        // The marking's listing of the page: 0 not listed, Listed (for the
        // pass, by the roots), else the number of the marking thread that
        // holds it (collector.h: Marker::id, _mark_page)
        static constexpr uint8_t Listed = 255;
        uint8_t reachable = {0};
        bool unreachable = {false};
        bool retire = {false};   // collector: states of the other parity to retire after the sweep, set where state_updated is lowered
        // Collector: the page may hold registered slots that are not marked
        // (registered & ~marked somewhere in its flags): raised by the
        // registration (new objects, the marks cleared by a full cycle),
        // lowered by the states pass that finds none and by the fold after
        // the sweep. A young cycle's pass over all the pages reads only the
        // header of a page without (collector.h: _update_page_marks).
        bool unmarked = {true};
        bool is_used = {true};
        // result of the last rebuild of the free bitmap (object_pool_allocator_base.h)
        bool all_free = {false};
        std::atomic_bool unused_occur = {true};
        Page* next_empty = {nullptr};   // the lists of empty pages: a type's, then its allocators' buffer
        Page* next = {nullptr};         // the list a thread publishes its new pages on (thread.h: Data::pages)
    };
    static_assert(sizeof(Page) == config::CacheLineSize, "the page header is one line of CacheLineSize; the states follow it");
}

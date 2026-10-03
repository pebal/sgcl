//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "tests/types.h"

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <typeinfo>
#include <unordered_set>
#include <vector>

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

// What a slot holds before its constructor runs, where the collector may
// already read it (maker.h: _init): slots taken raw, with the collector
// parked, and the words where the type holds pointers read.
namespace slot_probe {
    // The data pages objects were made on (raw addresses, no roots)
    struct PageSet {
        std::unordered_set<uintptr_t> pages;

        void add(const void* p) {
            pages.insert(sgcl::detail::Page::page_of(p)->data);
        }

        bool has(const void* p) const {
            return pages.count(sgcl::detail::Page::page_of(p)->data) != 0;
        }
    };

    struct Probed {
        size_t reused = 0;    // slots on the given pages
        size_t nonzero = 0;   // non-zero words among the checked ones of those slots
    };

    // n slots of T taken raw (no constructor) with the collector parked,
    // so that nothing reads them meanwhile: the words at the byte offsets
    // `offsets` (every word when empty) of the slots on `pages` counted,
    // then nulled, so that the destructor the dropped slot runs and the
    // next cycle meet nothing stale
    template<class T>
    SGCL_NOINLINE Probed probe_slots(size_t n, const PageSet& pages, std::vector<size_t> offsets = {}) {
        if (offsets.empty()) {
            for (size_t o = 0; o + sizeof(uintptr_t) <= sizeof(T); o += sizeof(uintptr_t)) {
                offsets.push_back(o);
            }
        }
        Probed r;
        collector::stepper parked(true);
        parked.advance_to(collector::stepper::phase::start);
        for (size_t i = 0; i < n; ++i) {
            auto slot = sgcl::detail::Maker<T>::make_tracked_data();
            auto bytes = (unsigned char*)slot.get();
            if (pages.has(bytes)) {
                ++r.reused;
                for (auto o : offsets) {
                    uintptr_t w;
                    std::memcpy(&w, bytes + o, sizeof(w));
                    if (w) {
                        ++r.nonzero;
                        std::memset(bytes + o, 0, sizeof(w));
                    }
                }
            }
        }
        parked.finish_cycle();
        return r;
    }

    // Byte offsets of the given words of an object of T, from a shape of
    // it (storage of its size: only addresses are computed)
    template<class T>
    struct Shape {
        alignas(T) unsigned char bytes[sizeof(T)];

        const T* operator->() const noexcept {
            return (const T*)bytes;
        }

        std::vector<size_t> offsets(std::initializer_list<const void*> words) const {
            std::vector<size_t> r;
            for (auto w : words) {
                r.push_back(size_t((const char*)w - (const char*)bytes));
            }
            return r;
        }
    };

    // A private nested type of a container (its node) named through an
    // explicit instantiation, whose arguments the access rules do not
    // cover: `template struct Expose<Tag, C::Node>;` and a declaration
    // `auto exposed(Tag);` in this namespace, then
    // `decltype(exposed(Tag{}))::type`
    template<class Tag, class T>
    struct Expose {
        friend auto exposed(Tag) {
            return std::type_identity<T>{};
        }
    };
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_base.h"
#include "child_pointers.h"
#include "header_slab.h"

namespace sgcl::detail {
    struct SharedHolder;   // tracked_ptr.h

    // What a Metadata is made of: constants only, one row per type
    // (page_info.h: private_metadata, a constexpr static of the type) or
    // per size class of a pool described by data (the strings,
    // string_data.h, and the buffers, maker.h), made into the Metadata by one function for all of
    // them (a constructor per type was ~330 bytes of code each).
    struct TypeConstants {
        void (*destroy)(void*) noexcept;
        void (*free)(Page*) noexcept;
        const std::type_info* type;
        size_t object_size;
        size_t header_size;
        unsigned object_count;
        bool is_array;
        bool pool_allocated;
        bool is_weak_cell;
        bool is_cell_block;
        bool is_root_holder;
        bool is_string;
    };

    struct Metadata;

    // The record of one managed type (page_info.h: PageInfo<T>::record),
    // constinit: its constants, the two flags its pointer map is made
    // from, its number for the allocators of the threads (thread.h:
    // alocator) and the slots of its Metadata and its pointer map, null
    // until the type's first use on any thread (Metadata::of_type,
    // ChildPointers::of_type). One record per type instead of two
    // function-local statics, each with a guard and its code per type:
    // the record is data, and the functions that fill it take its
    // address, one for all the types. Constant-initialized (the slots
    // null, the number 0), so a use from a global's initializer in any
    // translation unit is ordered with nothing (pull request #13).
    struct TypeRecord {
        std::atomic<unsigned> slot = {0};                       // the type's number, 0 until the first allocation of the type on any thread (thread.h: _slot_index)
        std::atomic<Metadata*> metadata = {nullptr};            // the type's Metadata, set once
        std::atomic<ChildPointers*> child_pointers = {nullptr}; // the type's pointer map, set once
        const TypeConstants constants;
        const bool may_contain_tracked;
        const bool conservative;
    };

    // What every page of one type shares, one per type (page_info.h:
    // private_metadata): the type's pointer map, its destroy and free
    // functions, the slab of its page headers, the size and count of its
    // objects, the kinds the collector treats apart, and for a pool type
    // the buffer of its emptied pages. The last three
    // fields are the collector's: the list of the type's pages emptied by
    // a cycle, on the way back to the allocators (collector.h:
    // _release_unused_pages).
    struct Metadata {
        // The constants of the type T (its size and count, its destroy and
        // free functions, the kinds the collector treats apart)
        template<class T>
        static constexpr TypeConstants constants_of() noexcept {
            using Info = TypeInfo<T>;
            using Type = std::remove_cv_t<T>;
            return {Info::get_destroy_function(), Info::Allocator::free, &typeid(T), Info::ObjectSize, Info::HeaderSize,
                    (unsigned)Info::ObjectCount, Info::IsArray, Info::Allocator::IsPoolAllocator::value,
                    std::is_same_v<Type, WeakCell>, std::is_same_v<Type, CellBlock>,
                    std::is_same_v<Type, SharedHolder> || std::is_same_v<Type, CellBlock>, Info::IsString};
        }

        // The Metadata of a type or of a pool described by data, from its
        // constants and its pointer map, with a slab of its page headers;
        // never freed, nor its map and slab (the pages point at it)
        SGCL_NOINLINE static Metadata* make(const TypeConstants& c, ChildPointers& child_pointers) {
            return new Metadata(c, child_pointers, new HeaderSlab(c.header_size));
        }

        // The Metadata of a pool described by data (a size class of the
        // strings, string_data.h, or of the buffers, maker.h), published in
        // `published` on the class's first use on any thread: a plain
        // pointer map (no tracked words) of its own. Cold: once per class
        // and thread. Threads racing to make it each make one and publish
        // it by a CAS from null; the losers delete theirs, which nothing has
        // seen. The acquire pairs with the winner's release: the Metadata
        // is whole.
        SGCL_NOINLINE static Metadata& of_pool(std::atomic<Metadata*>& published, const TypeConstants& c) {
            auto m = published.load(std::memory_order_acquire);
            if (m) {
                return *m;
            }
            auto mine = make(c, *ChildPointers::make(false, c.object_size, *c.type, false));
            if (published.compare_exchange_strong(m, mine, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return *mine;
            }
            delete &mine->child_pointers;
            delete mine->header_slab;
            delete mine;
            return *m;
        }

        // The Metadata of a type, published in its record on the type's
        // first use on any thread, with the type's pointer map. Cold: once
        // per type and thread (the thread's allocator for the type keeps
        // it), and for a type larger than a page once per page and per
        // object (page.h, object_allocator.h). Threads racing to make it
        // each make one and publish it by a CAS from null, as of_pool does;
        // the losers delete theirs, which nothing has seen, and keep the
        // map, which is the record's and shared. A CAS rather than a
        // once-guard: making a Metadata registers nothing anywhere (two
        // heap objects, no global list: the collector finds a Metadata
        // through the pages that name it), so a loser's copy is safe to
        // drop, and no thread ever waits on another; at most one spare
        // per racing thread, at the type's first use only. The acquire
        // pairs with the winner's release: the Metadata is whole.
        SGCL_NOINLINE static Metadata& of_type(TypeRecord& r) {
            auto m = r.metadata.load(std::memory_order_acquire);
            if (m) {
                return *m;
            }
            auto mine = make(r.constants, ChildPointers::of_type(r));
            if (r.metadata.compare_exchange_strong(m, mine, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return *mine;
            }
            delete mine->header_slab;
            delete mine;
            return *m;
        }

        Metadata(const TypeConstants& c, ChildPointers& cp, HeaderSlab* slab) noexcept
        : child_pointers(cp)
        , destroy(c.destroy)
        , free(c.free)
        , header_slab(slab)
        , object_size(c.object_size)
        , object_count(c.object_count)
        , is_array(c.is_array)
        , pool_allocated(c.pool_allocated)
        , is_weak_cell(c.is_weak_cell)
        , is_cell_block(c.is_cell_block)
        , is_root_holder(c.is_root_holder)
        , is_string(c.is_string)
        , type_info(*c.type) {
        }

        ChildPointers& child_pointers;
        void (*const destroy)(void*) noexcept;
        void (*const free)(Page*) noexcept;
        HeaderSlab* const header_slab;
        const size_t object_size;
        const unsigned object_count;
        const bool is_array;
        const bool pool_allocated;   // slots with a free bitmap, not page ranges
        const bool is_weak_cell;     // weak_cell.h: the pages the weak phase visits
        const bool is_cell_block;    // cell_block.h: the pages of the blocks of cells, traced by a map that stays full and released by state
        const bool is_root_holder;   // a SharedHolder (tracked_ptr.h: the object under a to_shared) or a CellBlock (cell_block.h: the cells of the root_ptrs): a root by state, never the target of a tracked_ptr, so a word naming one (the word of a shared_ptr's holder or of a root_ptr inside a managed object) is data (collector.h: _mark_childs)
        const bool is_string;        // string_data.h: the object holds a string's characters; a slice of the whole of one becomes the string (string.h)
        const std::type_info& type_info;
        std::atomic<Page*> pages_buffer = {nullptr};   // a pool type's emptied pages, sorted, for the allocators of every thread to refill from (object_pool_allocator_base.h, under its _buffers_mutex)
        Page* empty_page = {nullptr};
        Metadata* next = {nullptr};
        bool used = {false};
    };

    // The pointer map of a type, published in its record on first use, by
    // a CAS from null like the Metadata (Metadata::of_type, which names
    // it, and the ArrayMetadata of the type's buffers, array_metadata.h):
    // a loser deletes its own map, which nothing has seen
    inline ChildPointers& ChildPointers::of_type(TypeRecord& r) {
        auto p = r.child_pointers.load(std::memory_order_acquire);
        if (p) {
            return *p;
        }
        auto mine = make(r.may_contain_tracked, r.constants.object_size, *r.constants.type, r.conservative);
        if (r.child_pointers.compare_exchange_strong(p, mine, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return *mine;
        }
        delete mine;
        return *p;
    }
}

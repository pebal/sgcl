//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/cell_allocator.h"
#include "tracked_ptr.h"
#include "unique_ptr.h"

namespace sgcl {
    // A root that lives anywhere: a pointer to a managed object held from
    // unmanaged memory (a global, a std container, a handle table, a
    // lambda on the heap, the frame of a plain coroutine), the object
    // reachable for as long as the root_ptr exists. Under it, a cell: one
    // word of a managed block of a cache line of them (detail/cell_block.h)
    // that is a root by state, taken from the thread's allocator
    // (detail/cell_allocator.h) in the constructor and given back by the
    // destructor, and this root_ptr's for the whole time between. The
    // cell is a tracked word inside a managed object, so the root_ptr is
    // a tracked_ptr held one step away: ptr() is that tracked_ptr, by
    // reference, every read is its read and every store its store, with
    // its barrier, and an atomic_ref over it is the atomic of the root.
    // No store ever allocates, so two threads storing into the same
    // root_ptr race on one atomic word, as they do on a tracked_ptr, and
    // never on the making of a cell; no move ever takes a cell from
    // another root_ptr, so a thread reading through the cell of a
    // root_ptr another thread moves from reads a cell that lives as long
    // as its root_ptr. What it costs: a cell per root_ptr, one managed
    // allocation per block of them, a null included (the root_ptr may be
    // assigned to later); one indirection per access; a cell of its own
    // per copy. Copyable, movable (the pointer moves, the cells stay, the
    // source is null), convertible from and to a tracked_ptr. Threads
    // share a root_ptr the way they share a tracked_ptr (README, rule 6).
    template<class T>
    class root_ptr {
    public:
        using element_type = typename tracked_ptr<T>::element_type;

        // Every constructor takes a cell, on a registered thread (the
        // allocator's block is made through the thread's allocators)
        root_ptr() noexcept
        : _cell(_take()) {
        }

        root_ptr(std::nullptr_t) noexcept
        : root_ptr() {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr(const tracked_ptr<U>& p) noexcept
        : root_ptr() {
            ptr() = p;
        }

        // The object released from its owner: the barrier of the store
        // takes it out of the unique state, as in tracked_ptr
        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr(unique_ptr<U>&& u) noexcept
        : root_ptr() {
            ptr() = std::move(u);
        }

        root_ptr(const root_ptr& o) noexcept
        : root_ptr() {
            ptr() = o.ptr();
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename root_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr(const root_ptr<U>& o) noexcept
        : root_ptr() {
            ptr() = o.ptr();
        }

        // The pointer moves into a cell of its own; the source is null
        root_ptr(root_ptr&& o) noexcept
        : root_ptr() {
            ptr() = o.ptr();
            o.ptr() = nullptr;
        }

        ~root_ptr() noexcept {
            _free();
        }

        root_ptr& operator=(const root_ptr& o) noexcept {
            ptr() = o.ptr();
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename root_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(const root_ptr<U>& o) noexcept {
            ptr() = o.ptr();
            return *this;
        }

        root_ptr& operator=(root_ptr&& o) noexcept {
            if (this != &o) {
                ptr() = o.ptr();
                o.ptr() = nullptr;
            }
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(const tracked_ptr<U>& p) noexcept {
            ptr() = p;
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(unique_ptr<U>&& u) noexcept {
            ptr() = std::move(u);
            return *this;
        }

        root_ptr& operator=(std::nullptr_t) noexcept {
            ptr() = nullptr;
            return *this;
        }

        element_type* get() const noexcept {
            return ptr().get();
        }

        template<class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U& operator*() const noexcept {
            assert(get() != nullptr);
            return *get();
        }

        element_type* operator->() const noexcept {
            assert(get() != nullptr);
            return get();
        }

        explicit operator bool() const noexcept {
            return get() != nullptr;
        }

        // The tracked_ptr this root holds its object by: the cell's word,
        // inside a managed object, where a tracked_ptr lives. A reference,
        // not a copy: for the code that lives where a tracked_ptr may, and
        // for an atomic_ref (atomic_ref.h) over the root
        tracked_ptr<T>& ptr() noexcept {
            return *reinterpret_cast<tracked_ptr<T>*>(_cell);
        }

        const tracked_ptr<T>& ptr() const noexcept {
            return *reinterpret_cast<const tracked_ptr<T>*>(_cell);
        }

        operator tracked_ptr<T>&() noexcept {
            return ptr();
        }

        operator const tracked_ptr<T>&() const noexcept {
            return ptr();
        }

        void reset() noexcept {
            ptr() = nullptr;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        void reset(const tracked_ptr<U>& p) noexcept {
            ptr() = p;
        }

        // The pointers exchanged; the cells stay with their root_ptrs
        void swap(root_ptr& o) noexcept {
            ptr().swap(o.ptr());
        }

        // The dynamic type of the object, as tracked_ptr has them
        const std::type_info& type() const noexcept {
            return ptr().type();
        }

        template<class U>
        bool is() const noexcept {
            return ptr().template is<U>();
        }

        template<class U>
        tracked_ptr<U> as() const noexcept {
            return ptr().template as<U>();
        }

    private:
        // Out of line, both: the allocator and the release inlined into
        // every constructor and destructor of a root_ptr cost the caller
        // their code, and next to a managed allocation the call is nothing
        SGCL_NOINLINE static detail::Pointer* _take() noexcept {
            detail::ensure_thread_registered();
            return detail::cell_allocator.take();
        }

        SGCL_NOINLINE void _free() noexcept {
            detail::CellAllocator::free(_cell);
        }

        detail::Pointer* const _cell;   // a word of a managed detail::CellBlock, this root's for its life

        template<class> friend class root_ptr;
    };

    template<class T>
    root_ptr(tracked_ptr<T>) -> root_ptr<T>;

    template<class T>
    root_ptr(unique_ptr<T>&&) -> root_ptr<T>;

    template<class T>
    void swap(root_ptr<T>& l, root_ptr<T>& r) noexcept {
        l.swap(r);
    }

    // The comparisons: by the object pointed at, with a root_ptr, a
    // tracked_ptr, or null
    template<class T, class U>
    bool operator==(const root_ptr<T>& l, const root_ptr<U>& r) noexcept {
        return l.get() == r.get();
    }

    template<class T, class U>
    bool operator==(const root_ptr<T>& l, const tracked_ptr<U>& r) noexcept {
        return l.get() == r.get();
    }

    template<class T, class U>
    bool operator==(const tracked_ptr<T>& l, const root_ptr<U>& r) noexcept {
        return l.get() == r.get();
    }

    template<class T>
    bool operator==(const root_ptr<T>& l, std::nullptr_t) noexcept {
        return !l;
    }

    template<class T, class U>
    std::strong_ordering operator<=>(const root_ptr<T>& l, const root_ptr<U>& r) noexcept {
        return std::compare_three_way()(static_cast<const void*>(l.get()), static_cast<const void*>(r.get()));
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const root_ptr<T>& p) {
        s << p.get();
        return s;
    }
}

namespace std {
    template<class T>
    struct hash<sgcl::root_ptr<T>> {
        size_t operator()(const sgcl::root_ptr<T>& p) const noexcept {
            return hash<const void*>()(p.get());
        }
    };
}

//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "tracked_ptr.h"

namespace sgcl {
    // A root that lives anywhere: a pointer to a managed object held from
    // unmanaged memory (a global, a std container, a handle table, a
    // lambda on the heap), the object reachable for as long as the
    // root_ptr exists. Under it, a managed holder (detail::SharedHolder,
    // the one of to_shared) owned by a unique_ptr, so a root by the state
    // of its slot, with a tracked_ptr inside it that the collector
    // follows; the holder is made once, in the constructor, and lives as
    // long as the root_ptr does: a root_ptr is never without one, and an
    // empty one holds null. Where gc::tracked_ptr is the pointer that
    // lives anywhere by choosing its mode from its address, root_ptr says
    // in its type what it is, with no mode and no test: for code that
    // knows it stands outside the managed heap and wants a root there.
    // What it costs: a managed allocation per root_ptr (the holder, one
    // word), one indirection per access, a new holder per copy; a
    // gc::tracked_ptr takes a cell from a block instead and pays a test of
    // its mode per access. Copyable (a holder of its own each), movable
    // (the pointer moves, the holders stay), convertible from and to a
    // tracked_ptr of either kind: ptr() is the tracked_ptr to use where
    // one may live. Threads share a root_ptr the way they share a
    // tracked_ptr (README, rule 6). The same type under gc::.
    template<class T>
    class root_ptr {
    public:
        using element_type = typename tracked_ptr<T>::element_type;
        using tracked_type = tracked_ptr<T>;

        root_ptr()
        : _holder(make_tracked<detail::SharedHolder>()) {
        }

        root_ptr(std::nullptr_t)
        : root_ptr() {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr(const tracked_ptr<U>& p)
        : root_ptr() {
            _holder->ptr = p;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename gc::tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr(const gc::tracked_ptr<U>& p)
        : root_ptr() {
            _holder->ptr = tracked_ptr<U>(p);
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr(unique_ptr<U>&& u)
        : root_ptr() {
            _holder->ptr = tracked_ptr<U>(std::move(u));
        }

        root_ptr(const root_ptr& o)
        : root_ptr() {
            _holder->ptr = o._holder->ptr;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename root_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr(const root_ptr<U>& o)
        : root_ptr() {
            _holder->ptr = o._holder->ptr;
        }

        // The pointer moves into a holder of its own; the source is null
        root_ptr(root_ptr&& o)
        : root_ptr() {
            _holder->ptr = o._holder->ptr;
            o._holder->ptr = nullptr;
        }

        root_ptr& operator=(const root_ptr& o) noexcept {
            _holder->ptr = o._holder->ptr;
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename root_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(const root_ptr<U>& o) noexcept {
            _holder->ptr = o._holder->ptr;
            return *this;
        }

        root_ptr& operator=(root_ptr&& o) noexcept {
            if (this != &o) {
                _holder->ptr = o._holder->ptr;
                o._holder->ptr = nullptr;
            }
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(const tracked_ptr<U>& p) noexcept {
            _holder->ptr = p;
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename gc::tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(const gc::tracked_ptr<U>& p) noexcept {
            _holder->ptr = tracked_ptr<U>(p);
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(unique_ptr<U>&& u) noexcept {
            _holder->ptr = tracked_ptr<U>(std::move(u));
            return *this;
        }

        root_ptr& operator=(std::nullptr_t) noexcept {
            _holder->ptr = nullptr;
            return *this;
        }

        element_type* get() const noexcept {
            return static_cast<element_type*>(_holder->ptr.get());
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

        // The pointer as a tracked_ptr, for the code that lives where one may
        tracked_ptr<T> ptr() const noexcept {
            return tracked_ptr<T>(get());
        }

        operator tracked_ptr<T>() const noexcept {
            return ptr();
        }

        void reset() noexcept {
            _holder->ptr = nullptr;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        void reset(const tracked_ptr<U>& p) noexcept {
            _holder->ptr = p;
        }

        // The pointers exchanged; the holders stay with their root_ptrs
        void swap(root_ptr& o) noexcept {
            tracked_ptr<void> tmp = _holder->ptr;
            _holder->ptr = o._holder->ptr;
            o._holder->ptr = tmp;
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
        const unique_ptr<detail::SharedHolder> _holder;

        template<class> friend class root_ptr;
    };

    template<class T>
    root_ptr(tracked_ptr<T>) -> root_ptr<T>;

    template<class T>
    root_ptr(gc::tracked_ptr<T>) -> root_ptr<T>;

    template<class T>
    root_ptr(unique_ptr<T>&&) -> root_ptr<T>;

    template<class T>
    void swap(root_ptr<T>& l, root_ptr<T>& r) noexcept {
        l.swap(r);
    }

    // The comparisons: by the object pointed at, with a root_ptr, a
    // tracked_ptr of either kind, or null
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

    template<class T, class U>
    bool operator==(const root_ptr<T>& l, const gc::tracked_ptr<U>& r) noexcept {
        return l.get() == r.get();
    }

    template<class T, class U>
    bool operator==(const gc::tracked_ptr<T>& l, const root_ptr<U>& r) noexcept {
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
}

namespace std {
    template<class T>
    struct hash<sgcl::root_ptr<T>> {
        size_t operator()(const sgcl::root_ptr<T>& p) const noexcept {
            return hash<const void*>()(p.get());
        }
    };
}

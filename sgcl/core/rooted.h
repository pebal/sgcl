//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "make_tracked.h"
#include "root_ptr.h"

#include <cassert>
#include <type_traits>
#include <utility>

namespace sgcl {
    // A value kept in a managed object of its own, held by a root_ptr: for
    // a value with tracked pointers inside (a string, a slice, a container,
    // a function, an object with a tracked_ptr member) that has to lie
    // where a tracked_ptr may not — an exception object, which the runtime
    // allocates; a std container; a global; the closure a platform keeps
    // (a block, a callback's context). The value is made in the managed
    // heap by the constructor, from its arguments or from a value moved
    // in, and lives for as long as any rooted holding it does, through
    // the copies the runtime makes of an exception included. A handle, as
    // root_ptr is: a copy shares the object (a cell of its own, the same
    // object), a move leaves the source empty, to be destroyed or assigned
    // to and nothing else; a copy of the value is rooted<T>(*r). Against
    // the root_ptr inside it, two guarantees more: never null (no default
    // constructor, so a member cannot be left empty — a root_ptr member,
    // const or not, default-constructs to null), and the value made by
    // the constructor, so the type says what the member holds. What it
    // costs: a managed object and a root cell per rooted, a cell per
    // copy, one indirection per access.
    template<class T>
    class rooted {
        static_assert(!std::is_array_v<T> && !std::is_reference_v<T>, "a rooted value is one object");

        template<class U>
        static constexpr bool NotRooted = !std::is_same_v<std::remove_cvref_t<U>, rooted>;

    public:
        using value_type = T;

        // The value made in place from its arguments
        template<class... A>
        requires std::is_constructible_v<T, A...>
        explicit rooted(std::in_place_t, A&&... a)
        : _p(make_tracked<T>(std::forward<A>(a)...)) {
        }

        // The value copied or moved in
        template<class U = T>
        requires NotRooted<U> && std::is_constructible_v<T, U&&>
        rooted(U&& value)
        : _p(make_tracked<T>(std::forward<U>(value))) {
        }

        rooted(const rooted&) noexcept = default;
        rooted(rooted&&) noexcept = default;
        rooted& operator=(const rooted&) noexcept = default;
        rooted& operator=(rooted&&) noexcept = default;
        ~rooted() = default;

        T* get() const noexcept {
            return _p.get();
        }

        T& operator*() const noexcept {
            assert(_p && "a rooted moved from holds nothing");
            return *_p;
        }

        T* operator->() const noexcept {
            assert(_p && "a rooted moved from holds nothing");
            return _p.get();
        }

        // The object as a tracked_ptr, for code that lives where one may
        tracked_ptr<T> ptr() const noexcept {
            return _p.ptr();
        }

        void swap(rooted& o) noexcept {
            _p.swap(o._p);
        }

    private:
        root_ptr<T> _p;
    };

    template<class T>
    rooted(T) -> rooted<T>;

    template<class T>
    void swap(rooted<T>& a, rooted<T>& b) noexcept {
        a.swap(b);
    }
}

//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/value_storage.h"

#include <any>
#include <initializer_list>
#include <typeinfo>
#include <utility>

namespace sgcl {
    using std::bad_any_cast;

    // An any with the interface of std::any, safe to hold a tracked
    // pointer or an object with tracked pointers inside. std::any keeps a
    // small value in a buffer inside itself, where a tracked_ptr would
    // share its word with the data of other values (the offset leaves the
    // type's pointer map by elimination: README, Pointer maps), and a
    // large one on the unmanaged heap, where a tracked_ptr may not live.
    // Here a value goes to one of three places by what it is
    // (detail/value_storage.h): a pointer word (tracked_ptr of either
    // kind, weak_ptr) into a word of the any that holds null or an
    // address and nothing else; a small value that cannot hold a pointer
    // into a buffer of 16 bytes; anything else into a managed node of its
    // own, held by a pointer in the same word and traced through its own
    // pointer map, destroyed the moment the any drops it (the node
    // reclaimed later, as a container's), so that a value pointing back
    // at the any's owner is a cycle collected like any other. The
    // interface is that of std::any: the constructors, in_place_type,
    // emplace, reset, swap, has_value, type, any_cast and make_any; a
    // copy of a value in a node is a node of its own. Ptr is the kind of
    // the word, and so where the any lives: tracked_ptr (sgcl::any) on a
    // stack or in a managed object, gc::tracked_ptr (gc::any) anywhere.
    // A value larger than a page is not supported. 32 bytes.
    template<template<class> class Ptr>
    class basic_any : detail::ValueStorage<Ptr> {
        using Storage = detail::ValueStorage<Ptr>;

        template<class T>
        struct IsInPlace : std::false_type {};

        template<class T>
        struct IsInPlace<std::in_place_type_t<T>> : std::true_type {};

    public:
        basic_any() noexcept = default;

        basic_any(const basic_any& o) = default;

        basic_any(basic_any&& o) noexcept = default;

        template<class T, class VT = std::decay_t<T>>
        requires (!std::is_same_v<VT, basic_any> && !IsInPlace<VT>::value && std::is_copy_constructible_v<VT>)
        basic_any(T&& value) {
            this->template _emplace<VT, true, nullptr>(std::forward<T>(value));
        }

        template<class T, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, A...> && std::is_copy_constructible_v<VT>
        explicit basic_any(std::in_place_type_t<T>, A&&... a) {
            this->template _emplace<VT, true, nullptr>(std::forward<A>(a)...);
        }

        template<class T, class U, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, std::initializer_list<U>&, A...> && std::is_copy_constructible_v<VT>
        explicit basic_any(std::in_place_type_t<T>, std::initializer_list<U> il, A&&... a) {
            this->template _emplace<VT, true, nullptr>(il, std::forward<A>(a)...);
        }

        ~basic_any() = default;

        basic_any& operator=(const basic_any& o) {
            basic_any(o).swap(*this);
            return *this;
        }

        basic_any& operator=(basic_any&& o) noexcept {
            basic_any(std::move(o)).swap(*this);
            return *this;
        }

        template<class T, class VT = std::decay_t<T>>
        requires (!std::is_same_v<VT, basic_any> && std::is_copy_constructible_v<VT>)
        basic_any& operator=(T&& value) {
            basic_any(std::forward<T>(value)).swap(*this);
            return *this;
        }

        template<class T, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, A...> && std::is_copy_constructible_v<VT>
        VT& emplace(A&&... a) {
            reset();
            return this->template _emplace<VT, true, nullptr>(std::forward<A>(a)...);
        }

        template<class T, class U, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, std::initializer_list<U>&, A...> && std::is_copy_constructible_v<VT>
        VT& emplace(std::initializer_list<U> il, A&&... a) {
            reset();
            return this->template _emplace<VT, true, nullptr>(il, std::forward<A>(a)...);
        }

        // The value destroyed now, on this thread, wherever it is
        void reset() noexcept {
            this->_reset();
        }

        void swap(basic_any& o) noexcept {
            this->_swap(o);
        }

        bool has_value() const noexcept {
            return this->_manager != nullptr;
        }

        const std::type_info& type() const noexcept {
            return this->_manager ? this->_manager->type : typeid(void);
        }

    private:
        template<class T, template<class> class P>
        friend const T* any_cast(const basic_any<P>*) noexcept;
        template<class T, template<class> class P>
        friend T* any_cast(basic_any<P>*) noexcept;
    };

    using any = basic_any<tracked_ptr>;

    template<template<class> class Ptr>
    void swap(basic_any<Ptr>& l, basic_any<Ptr>& r) noexcept {
        l.swap(r);
    }

    template<class T, class... A>
    any make_any(A&&... a) {
        return any(std::in_place_type<T>, std::forward<A>(a)...);
    }

    template<class T, class U, class... A>
    any make_any(std::initializer_list<U> il, A&&... a) {
        return any(std::in_place_type<T>, il, std::forward<A>(a)...);
    }

    template<class T, template<class> class Ptr>
    const T* any_cast(const basic_any<Ptr>* a) noexcept {
        static_assert(!std::is_void_v<T>, "any_cast to void");
        return a && a->_manager && a->_manager->type == typeid(T) ? static_cast<const T*>(a->_manager->get(*a)) : nullptr;
    }

    template<class T, template<class> class Ptr>
    T* any_cast(basic_any<Ptr>* a) noexcept {
        static_assert(!std::is_void_v<T>, "any_cast to void");
        return a && a->_manager && a->_manager->type == typeid(T) ? static_cast<T*>(a->_manager->get(*a)) : nullptr;
    }

    template<class T, template<class> class Ptr, class U = std::remove_cvref_t<T>>
    requires std::is_constructible_v<T, const U&>
    T any_cast(const basic_any<Ptr>& a) {
        auto p = any_cast<U>(&a);
        if (!p) {
            throw bad_any_cast();
        }
        return static_cast<T>(*p);
    }

    template<class T, template<class> class Ptr, class U = std::remove_cvref_t<T>>
    requires std::is_constructible_v<T, U&>
    T any_cast(basic_any<Ptr>& a) {
        auto p = any_cast<U>(&a);
        if (!p) {
            throw bad_any_cast();
        }
        return static_cast<T>(*p);
    }

    template<class T, template<class> class Ptr, class U = std::remove_cvref_t<T>>
    requires std::is_constructible_v<T, U>
    T any_cast(basic_any<Ptr>&& a) {
        auto p = any_cast<U>(&a);
        if (!p) {
            throw bad_any_cast();
        }
        return static_cast<T>(std::move(*p));
    }
}

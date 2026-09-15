//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/pointer_word.h"
#include "make_tracked.h"

#include <any>
#include <cstring>
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
    // (detail/pointer_word.h): a pointer word (tracked_ptr of either kind,
    // weak_ptr) into a word of the any that holds null or an address and
    // nothing else; a small value that cannot hold a pointer into a
    // buffer of 16 bytes; anything else into a managed object of its own,
    // owned the way a unique_ptr owns (a root by the state of its slot,
    // destroyed the moment the any drops it, traced through its own
    // pointer map), whose address the same word holds. The interface is
    // that of std::any: the constructors, in_place_type, emplace, reset,
    // swap, has_value, type, any_cast and make_any; a copy of a value in
    // a managed object is a managed object. What it holds follows the
    // rules of its type where the any lives, as a member would: an any
    // holding an sgcl::tracked_ptr lives where a tracked_ptr may, one
    // holding a gc::tracked_ptr anywhere; a value in a managed object is
    // anywhere's. A value larger than a page is not supported.
    class any {
        static constexpr size_t BufferSize = 16;
        static constexpr size_t BufferAlign = 8;

        template<class T>
        static constexpr bool Inline = !detail::MayContainTracked<T>::value && sizeof(T) <= BufferSize && alignof(T) <= BufferAlign && std::is_nothrow_move_constructible_v<T>;

        template<class T>
        static constexpr bool Word = detail::IsPointerWord<T>;

        struct Manager {
            const std::type_info& type;
            void (*copy)(any& to, const any& from);
            void (*move)(any& to, any& from) noexcept;
            void (*destroy)(any& a) noexcept;
            void* (*get)(const any& a) noexcept;
        };

        template<class T>
        struct IsInPlace : std::false_type {};

        template<class T>
        struct IsInPlace<std::in_place_type_t<T>> : std::true_type {};

        // The value in the buffer
        template<class T>
        struct InlineOps {
            static void copy(any& to, const any& from) {
                ::new(to._buffer) T(*static_cast<const T*>(get(from)));
                to._manager = from._manager;
            }
            static void move(any& to, any& from) noexcept {
                ::new(to._buffer) T(std::move(*static_cast<T*>(get(from))));
                to._manager = from._manager;
                destroy(from);
            }
            static void destroy(any& a) noexcept {
                static_cast<T*>(get(a))->~T();
                a._manager = nullptr;
            }
            static void* get(const any& a) noexcept {
                return const_cast<unsigned char*>(a._buffer);
            }
            static constexpr Manager manager = {typeid(T), copy, move, destroy, get};
        };

        // The pointer word constructed in the any's own word
        template<class T>
        struct WordOps {
            static void copy(any& to, const any& from) {
                ::new(to._word) T(*static_cast<const T*>(get(from)));
                to._manager = from._manager;
            }
            static void move(any& to, any& from) noexcept {
                ::new(to._word) T(std::move(*static_cast<T*>(get(from))));
                to._manager = from._manager;
                destroy(from);
            }
            static void destroy(any& a) noexcept {
                static_cast<T*>(get(a))->~T();   // leaves null in the word
                a._manager = nullptr;
            }
            static void* get(const any& a) noexcept {
                return const_cast<unsigned char*>(a._word);
            }
            static constexpr Manager manager = {typeid(T), copy, move, destroy, get};
        };

        // The value in a managed object of its own, the word its address
        template<class T>
        struct ObjectOps {
            static void copy(any& to, const any& from) {
                to._set_object(make_tracked<T>(*static_cast<const T*>(get(from))).release());
                to._manager = from._manager;
            }
            static void move(any& to, any& from) noexcept {
                to._set_object(from._object());
                to._manager = from._manager;
                from._set_object(nullptr);
                from._manager = nullptr;
            }
            static void destroy(any& a) noexcept {
                detail::Collector::delete_unique(a._object());
                a._set_object(nullptr);
                a._manager = nullptr;
            }
            static void* get(const any& a) noexcept {
                return a._object();
            }
            static constexpr Manager manager = {typeid(T), copy, move, destroy, get};
        };

        template<class T>
        using Ops = std::conditional_t<Word<T>, WordOps<T>, std::conditional_t<Inline<T>, InlineOps<T>, ObjectOps<T>>>;

    public:
        any() noexcept = default;

        any(const any& o) {
            if (o._manager) {
                o._manager->copy(*this, o);
            }
        }

        any(any&& o) noexcept {
            if (o._manager) {
                o._manager->move(*this, o);
            }
        }

        template<class T, class VT = std::decay_t<T>>
        requires (!std::is_same_v<VT, any> && !IsInPlace<VT>::value && std::is_copy_constructible_v<VT>)
        any(T&& value) {
            _emplace<VT>(std::forward<T>(value));
        }

        template<class T, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, A...> && std::is_copy_constructible_v<VT>
        explicit any(std::in_place_type_t<T>, A&&... a) {
            _emplace<VT>(std::forward<A>(a)...);
        }

        template<class T, class U, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, std::initializer_list<U>&, A...> && std::is_copy_constructible_v<VT>
        explicit any(std::in_place_type_t<T>, std::initializer_list<U> il, A&&... a) {
            _emplace<VT>(il, std::forward<A>(a)...);
        }

        ~any() {
            reset();
        }

        any& operator=(const any& o) {
            any(o).swap(*this);
            return *this;
        }

        any& operator=(any&& o) noexcept {
            any(std::move(o)).swap(*this);
            return *this;
        }

        template<class T, class VT = std::decay_t<T>>
        requires (!std::is_same_v<VT, any> && std::is_copy_constructible_v<VT>)
        any& operator=(T&& value) {
            any(std::forward<T>(value)).swap(*this);
            return *this;
        }

        template<class T, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, A...> && std::is_copy_constructible_v<VT>
        VT& emplace(A&&... a) {
            reset();
            return _emplace<VT>(std::forward<A>(a)...);
        }

        template<class T, class U, class... A, class VT = std::decay_t<T>>
        requires std::is_constructible_v<VT, std::initializer_list<U>&, A...> && std::is_copy_constructible_v<VT>
        VT& emplace(std::initializer_list<U> il, A&&... a) {
            reset();
            return _emplace<VT>(il, std::forward<A>(a)...);
        }

        // The value destroyed now, on this thread, wherever it is
        void reset() noexcept {
            if (_manager) {
                _manager->destroy(*this);
            }
        }

        void swap(any& o) noexcept {
            if (this == &o) {
                return;
            }
            any tmp(std::move(o));
            if (_manager) {
                _manager->move(o, *this);
            }
            if (tmp._manager) {
                tmp._manager->move(*this, tmp);
            }
        }

        bool has_value() const noexcept {
            return _manager != nullptr;
        }

        const std::type_info& type() const noexcept {
            return _manager ? _manager->type : typeid(void);
        }

    private:
        template<class T, class... A>
        T& _emplace(A&&... a) {
            if constexpr(Word<T>) {
                auto p = ::new(_word) T(std::forward<A>(a)...);
                _manager = &WordOps<T>::manager;
                return *p;
            } else if constexpr(Inline<T>) {
                auto p = ::new(_buffer) T(std::forward<A>(a)...);
                _manager = &InlineOps<T>::manager;
                return *p;
            } else {
                static_assert(sizeof(detail::Array<sizeof(T)>) <= detail::PageDataSize, "a value larger than a page is not supported");
                auto p = make_tracked<T>(std::forward<A>(a)...).release();
                _set_object(p);
                _manager = &ObjectOps<T>::manager;
                return *p;
            }
        }

        void* _object() const noexcept {
            void* p;
            std::memcpy(&p, _word, sizeof(p));
            return p;
        }

        void _set_object(void* p) noexcept {
            std::memcpy(_word, &p, sizeof(p));
        }

        template<class T>
        friend const T* any_cast(const any*) noexcept;
        template<class T>
        friend T* any_cast(any*) noexcept;

        const Manager* _manager = nullptr;
        // null or an address, never data: a pointer word constructed in
        // place, or the address of the managed object holding the value
        alignas(BufferAlign) unsigned char _word[sizeof(void*)] = {};
        alignas(BufferAlign) unsigned char _buffer[BufferSize];
    };

    inline void swap(any& l, any& r) noexcept {
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

    template<class T>
    const T* any_cast(const any* a) noexcept {
        static_assert(!std::is_void_v<T>, "any_cast to void");
        return a && a->_manager && a->_manager->type == typeid(T) ? static_cast<const T*>(a->_manager->get(*a)) : nullptr;
    }

    template<class T>
    T* any_cast(any* a) noexcept {
        static_assert(!std::is_void_v<T>, "any_cast to void");
        return a && a->_manager && a->_manager->type == typeid(T) ? static_cast<T*>(a->_manager->get(*a)) : nullptr;
    }

    template<class T, class U = std::remove_cvref_t<T>>
    requires std::is_constructible_v<T, const U&>
    T any_cast(const any& a) {
        auto p = any_cast<U>(&a);
        if (!p) {
            throw bad_any_cast();
        }
        return static_cast<T>(*p);
    }

    template<class T, class U = std::remove_cvref_t<T>>
    requires std::is_constructible_v<T, U&>
    T any_cast(any& a) {
        auto p = any_cast<U>(&a);
        if (!p) {
            throw bad_any_cast();
        }
        return static_cast<T>(*p);
    }

    template<class T, class U = std::remove_cvref_t<T>>
    requires std::is_constructible_v<T, U>
    T any_cast(any&& a) {
        auto p = any_cast<U>(&a);
        if (!p) {
            throw bad_any_cast();
        }
        return static_cast<T>(std::move(*p));
    }
}

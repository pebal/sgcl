//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Any: a value of any type, held on the managed heap, so
// that a Ptr inside it is traced. Get<T>() is the value as a pointer,
// null when it is not a T; As<T>() the value itself (std::bad_any_cast
// when it is not one).
#pragma once

#include "../../core/any.h"

namespace Sgcl {
    class Any {
    public:
        using InnerType = sgcl::any;

        Any() noexcept = default;
        Any(const Any&) = default;
        Any(Any&&) noexcept = default;
        Any& operator=(const Any&) = default;
        Any& operator=(Any&&) noexcept = default;

        template<class T>
        requires (!std::is_same_v<std::remove_cvref_t<T>, Any>) && (!std::is_same_v<std::remove_cvref_t<T>, InnerType>)
        Any(T&& value)
        : _a(std::forward<T>(value)) {
        }

        template<class T, class... A>
        explicit Any(std::in_place_type_t<T> t, A&&... a)
        : _a(t, std::forward<A>(a)...) {
        }

        template<class T, class U, class... A>
        explicit Any(std::in_place_type_t<T> t, std::initializer_list<U> il, A&&... a)
        : _a(t, il, std::forward<A>(a)...) {
        }

        // From the object inside itself. A template, so that a copy of an
        // Any is never tried through a conversion to the InnerType (whose own
        // converting constructor would ask whether an Any is copyable,
        // and round it goes)
        template<class I>
        requires std::is_same_v<std::remove_cvref_t<I>, InnerType>
        explicit Any(I&& a) noexcept
        : _a(std::forward<I>(a)) {
        }

        template<class T>
        requires (!std::is_same_v<std::remove_cvref_t<T>, Any>) && (!std::is_same_v<std::remove_cvref_t<T>, InnerType>)
        Any& operator=(T&& value) {
            _a = std::forward<T>(value);
            return *this;
        }

        template<class T, class... A>
        std::decay_t<T>& Emplace(A&&... a) {
            return _a.template emplace<T>(std::forward<A>(a)...);
        }

        template<class T, class U, class... A>
        std::decay_t<T>& Emplace(std::initializer_list<U> il, A&&... a) {
            return _a.template emplace<T>(il, std::forward<A>(a)...);
        }

        bool HasValue() const noexcept {
            return _a.has_value();
        }

        const std::type_info& Type() const noexcept {
            return _a.type();
        }

        template<class T>
        bool Is() const noexcept {
            return _a.type() == typeid(T);
        }

        // The value as a T, null when it is not one
        template<class T>
        T* Get() noexcept {
            return sgcl::any_cast<T>(&_a);
        }

        template<class T>
        const T* Get() const noexcept {
            return sgcl::any_cast<T>(&_a);
        }

        // The value as a T (a copy, or a reference for T = U&)
        template<class T>
        T As() & {
            return sgcl::any_cast<T>(_a);
        }

        template<class T>
        T As() const& {
            return sgcl::any_cast<T>(_a);
        }

        template<class T>
        T As() && {
            return sgcl::any_cast<T>(std::move(_a));
        }

        void Reset() noexcept {
            _a.reset();
        }

        void Swap(Any& o) noexcept {
            _a.swap(o._a);
        }

        InnerType& Inner() noexcept {
            return _a;
        }

        const InnerType& Inner() const noexcept {
            return _a;
        }

    private:
        InnerType _a;
    };

    inline void swap(Any& a, Any& b) noexcept {
        a.Swap(b);
    }

    template<class T, class... A>
    Any MakeAny(A&&... a) {
        return Any(std::in_place_type<T>, std::forward<A>(a)...);
    }

    template<class T, class U, class... A>
    Any MakeAny(std::initializer_list<U> il, A&&... a) {
        return Any(std::in_place_type<T>, il, std::forward<A>(a)...);
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...


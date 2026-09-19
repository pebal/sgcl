//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Array<T, N>: N elements in place, or, N left
// out, a count fixed at construction in a managed buffer.
#pragma once

#include "../../containers/array.h"
#include "MSequence.h"

#include <algorithm>
#include <tuple>

namespace Sgcl {
    template<class T, size_t N = sgcl::dynamic_extent>
    class Array : public MSequence<Array<T, N>> {   // the algorithms as members
    public:
        using ValueType = T;
        using InnerType = sgcl::array<T, N>;
        using SizeType = size_t;

        static constexpr bool IsDynamic = N == sgcl::dynamic_extent;

        Array() = default;

        // A count of elements (the dynamic one)
        constexpr explicit Array(SizeType count) requires IsDynamic
        : _a(count) {
        }

        constexpr Array(SizeType count, const T& value) requires IsDynamic
        : _a(count, value) {
        }

        template<std::input_iterator It>
        constexpr Array(It first, It last) requires IsDynamic
        : _a(first, last) {
        }

        constexpr Array(std::initializer_list<T> il) {
            if constexpr (IsDynamic) {
                _a = InnerType(il);
            } else {
                std::copy_n(il.begin(), std::min(il.size(), N), _a.begin());
            }
        }

        constexpr explicit Array(InnerType a) noexcept(IsDynamic)
        : _a(std::move(a)) {
        }

        Array(const Array&) = default;
        Array(Array&&) noexcept = default;
        Array& operator=(const Array&) = default;
        Array& operator=(Array&&) noexcept = default;

        constexpr T& operator[](SizeType i) noexcept {
            return _a[i];
        }

        constexpr const T& operator[](SizeType i) const noexcept {
            return _a[i];
        }

        constexpr T& First() noexcept {
            return _a.front();
        }

        constexpr const T& First() const noexcept {
            return _a.front();
        }

        constexpr T& Last() noexcept {
            return _a.back();
        }

        constexpr const T& Last() const noexcept {
            return _a.back();
        }

        constexpr T* Data() noexcept {
            return _a.data();
        }

        constexpr const T* Data() const noexcept {
            return _a.data();
        }

        constexpr SizeType Count() const noexcept {
            return _a.size();
        }

        constexpr bool IsEmpty() const noexcept {
            return _a.size() == 0;
        }

        constexpr void Swap(Array& o) noexcept {
            _a.swap(o._a);
        }

        constexpr InnerType& Inner() noexcept {
            return _a;
        }

        constexpr const InnerType& Inner() const noexcept {
            return _a;
        }

        friend bool operator==(const Array& a, const Array& b) {
            return a._a == b._a;
        }

        friend auto operator<=>(const Array& a, const Array& b) {
            return a._a <=> b._a;
        }

    private:
        InnerType _a;
    };

    template<class T, class... U>
    Array(T, U...) -> Array<std::enable_if_t<(std::is_same_v<T, U> && ...), T>, 1 + sizeof...(U)>;

    template<std::input_iterator It>
    Array(It, It) -> Array<std::iter_value_t<It>>;

    // The I-th element of a fixed Array, and an Array from a built-in
    // array: the tuple interface, for structured bindings
    template<size_t I, class T, size_t N>
    T& Get(Array<T, N>& a) noexcept {
        static_assert(I < N, "Get<I>: I out of range");
        return a[I];
    }

    template<size_t I, class T, size_t N>
    const T& Get(const Array<T, N>& a) noexcept {
        static_assert(I < N, "Get<I>: I out of range");
        return a[I];
    }

    template<size_t I, class T, size_t N>
    T&& Get(Array<T, N>&& a) noexcept {
        static_assert(I < N, "Get<I>: I out of range");
        return std::move(a[I]);
    }

    // `get` for structured bindings (found by ADL); Get is the name to write
    template<size_t I, class T, size_t N>
    T& get(Array<T, N>& a) noexcept {
        return Get<I>(a);
    }

    template<size_t I, class T, size_t N>
    const T& get(const Array<T, N>& a) noexcept {
        return Get<I>(a);
    }

    template<size_t I, class T, size_t N>
    T&& get(Array<T, N>&& a) noexcept {
        return Get<I>(std::move(a));
    }

    template<class T, size_t N>
    Array<std::remove_cv_t<T>, N> ToArray(T (&a)[N]) {
        Array<std::remove_cv_t<T>, N> r;
        std::copy_n(a, N, r.Data());
        return r;
    }

    template<class T, size_t N>
    Array<std::remove_cv_t<T>, N> ToArray(T (&&a)[N]) {
        Array<std::remove_cv_t<T>, N> r;
        std::move(a, a + N, r.Data());
        return r;
    }

    template<class T, size_t N>
    constexpr auto begin(Array<T, N>& a) noexcept {
        return a.Inner().begin();
    }

    template<class T, size_t N>
    constexpr auto end(Array<T, N>& a) noexcept {
        return a.Inner().end();
    }

    template<class T, size_t N>
    constexpr auto begin(const Array<T, N>& a) noexcept {
        return a.Inner().begin();
    }

    template<class T, size_t N>
    constexpr auto end(const Array<T, N>& a) noexcept {
        return a.Inner().end();
    }

    template<class T, size_t N>
    void swap(Array<T, N>& a, Array<T, N>& b) noexcept {
        a.Swap(b);
    }
}

template<class T, size_t N>
requires (N != sgcl::dynamic_extent)
struct std::tuple_size<Sgcl::Array<T, N>> : std::integral_constant<size_t, N> {};

template<size_t I, class T, size_t N>
requires (N != sgcl::dynamic_extent)
struct std::tuple_element<I, Sgcl::Array<T, N>> {
    using type = T;
};

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...


//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Range<It>: a pair of iterators as a range, what a lookup of a key with
// several values hands back (MultiDictionary::Values); Range(n) and
// Range(first, last) count integers; for a range-for. The sgcl::range
// inside, every method a forward.
#pragma once

#include "../../core/range.h"

#include <concepts>
#include <cstddef>
#include <ranges>

namespace Sgcl {
    template<class It>
    class Range {
    public:
        using Iterator = It;
        using InnerType = sgcl::range<It>;
        using ValueType = typename InnerType::value_type;

        Range() = default;

        Range(It first, It last) noexcept
        : _r(first, last) {
        }

        template<class Pair>
        requires requires(Pair p) { It(p.first); It(p.second); }
        Range(Pair p) noexcept
        : _r(p) {
        }

        template<std::integral T>
        requires std::same_as<It, sgcl::detail::counter<T>>
        explicit Range(T last) noexcept
        : _r(last) {
        }

        template<std::integral T>
        requires std::same_as<It, sgcl::detail::counter<T>>
        Range(T first, T last) noexcept
        : _r(first, last) {
        }

        explicit Range(InnerType r) noexcept
        : _r(r) {
        }

        bool IsEmpty() const noexcept {
            return _r.empty();
        }

        size_t Count() const {
            return _r.size();
        }

        decltype(auto) First() const noexcept {
            return _r.front();
        }

        It Begin() const noexcept {
            return _r.begin();
        }

        It End() const noexcept {
            return _r.end();
        }

        InnerType& Inner() noexcept {
            return _r;
        }

        const InnerType& Inner() const noexcept {
            return _r;
        }

    private:
        InnerType _r;
    };

    template<std::integral T>
    Range(T last) -> Range<sgcl::detail::counter<T>>;

    template<std::integral T>
    Range(T first, T last) -> Range<sgcl::detail::counter<T>>;

    template<class Pair>
    Range(Pair p) -> Range<decltype(p.first)>;

    template<class It>
    It begin(const Range<It>& r) noexcept {
        return r.Begin();
    }

    template<class It>
    It end(const Range<It>& r) noexcept {
        return r.End();
    }
}

// Owns nothing, so an iterator into it outlives the Range object (as with
// std::ranges::subrange): std::ranges takes a temporary
template<class It>
inline constexpr bool std::ranges::enable_borrowed_range<Sgcl::Range<It>> = true;

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

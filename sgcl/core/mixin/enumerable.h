//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../detail/os.h"
#include "../aliases.h"
#include "../req.h"

#include <algorithm>
#include <iterator>

namespace sgcl::mixin {
    // enumerable<Derived>: the questions asked of the elements of a
    // range, as members of every container of the library that iterates,
    // and the declaration that it does: req::enumerable<R> is "R carries
    // enumerable". A mixin (m_): a static interface, no virtual method,
    // no state; its constructor and destructor are protected, so it
    // exists only as the base of the class that names itself as Derived
    // and gives its elements through begin() and end(). The questions
    // that compare elements exist only for elements that compare
    // (req::equatable, req::comparable, on each method); the ones that take a
    // predicate or a comparator ask nothing of the element. A container
    // whose own answer is better (a sorted_set's contains by the key, its
    // min as *begin()) hides the mixin's with a method of the same name — every
    // overload of the name with it, since a constrained overload of the
    // base and an unconstrained one of the class would be ambiguous.
    // Each method is noexcept as far as what it calls is: the element's
    // == or <, the function given (and its copy).
    template<class Derived>
    class enumerable {
    public:
        // Searching by a predicate: the position of the first element
        // accepted (npos when none), a pointer to it (null when none). A
        // range whose iterator gives values, not elements (range(n), the
        // runes of a text), has no element to point to: find_if gives the
        // value found in an optional (empty when none), tested and read in
        // one if as the pointer is
        template<class Pred>
        SGCL_INLINE_HOT constexpr size_t find_index(Pred pred) const noexcept(detail::nothrow_callback<Pred, detail::ElementReference<const Derived>>) requires detail::ElementPredicate<Pred, Derived> {
            auto it = std::ranges::find_if(_begin(), _end(), pred);
            return it == _end() ? npos : size_t(std::ranges::distance(_begin(), it));
        }

        template<class Pred>
        SGCL_INLINE_HOT constexpr auto find_if(Pred pred) noexcept(detail::nothrow_callback<Pred, detail::ElementReference<Derived>> && _nothrow_found<Derived>()) requires detail::ElementPredicate<Pred, Derived> {
            return _found(std::ranges::find_if(_begin(), _end(), pred), _end());
        }

        template<class Pred>
        SGCL_INLINE_HOT constexpr auto find_if(Pred pred) const noexcept(detail::nothrow_callback<Pred, detail::ElementReference<const Derived>> && _nothrow_found<const Derived>()) requires detail::ElementPredicate<Pred, Derived> {
            return _found(std::ranges::find_if(_begin(), _end(), pred), _end());
        }

        // Whether some element satisfies the predicate; whether every one
        // does; how many do
        template<class Pred>
        SGCL_INLINE_HOT constexpr bool exists(Pred pred) const noexcept(detail::nothrow_callback<Pred, detail::ElementReference<const Derived>>) requires detail::ElementPredicate<Pred, Derived> {
            return std::ranges::find_if(_begin(), _end(), pred) != _end();
        }

        template<class Pred>
        SGCL_INLINE_HOT constexpr bool all(Pred pred) const noexcept(detail::nothrow_callback<Pred, detail::ElementReference<const Derived>>) requires detail::ElementPredicate<Pred, Derived> {
            return std::ranges::all_of(_begin(), _end(), pred);
        }

        template<class Pred>
        SGCL_INLINE_HOT constexpr size_t count_of(Pred pred) const noexcept(detail::nothrow_callback<Pred, detail::ElementReference<const Derived>>) requires detail::ElementPredicate<Pred, Derived> {
            return size_t(std::ranges::count_if(_begin(), _end(), pred));
        }

        // Visiting
        template<class F>
        SGCL_INLINE_HOT constexpr void for_each(F f) noexcept(detail::nothrow_callback<F, detail::ElementReference<Derived>>) requires detail::ElementVisitor<F, Derived> {
            std::ranges::for_each(_begin(), _end(), f);
        }

        template<class F>
        SGCL_INLINE_HOT constexpr void for_each(F f) const noexcept(detail::nothrow_callback<F, detail::ElementReference<const Derived>>) requires detail::ElementVisitor<F, const Derived> {
            std::ranges::for_each(_begin(), _end(), f);
        }

        // Searching by a value: elements that compare equal
        SGCL_INLINE_HOT constexpr bool contains(const auto& value) const noexcept(detail::nothrow_equal<detail::ElementValue<Derived>, std::remove_cvref_t<decltype(value)>>) requires detail::EquatableElements<Derived> {
            return std::ranges::find(_begin(), _end(), value) != _end();
        }

        // The position of the first element equal to value, npos when none
        SGCL_INLINE_HOT constexpr size_t index_of(const auto& value) const noexcept(detail::nothrow_equal<detail::ElementValue<Derived>, std::remove_cvref_t<decltype(value)>>) requires detail::EquatableElements<Derived> {
            auto it = std::ranges::find(_begin(), _end(), value);
            return it == _end() ? npos : size_t(std::ranges::distance(_begin(), it));
        }

        constexpr size_t last_index_of(const auto& value) const noexcept(detail::nothrow_equal<detail::ElementValue<Derived>, std::remove_cvref_t<decltype(value)>>) requires detail::EquatableElements<Derived> {
            size_t found = npos, i = 0;
            for (auto it = _begin(); it != _end(); ++it, ++i) {
                if (*it == value) {
                    found = i;
                }
            }
            return found;
        }

        // The smallest and the largest element: undefined on an empty
        // range, as front(); by <, or by the comparator given. A reference
        // into the range, or a value where the iterator gives values
        // (range(n))
        SGCL_INLINE_HOT constexpr decltype(auto) min() const noexcept(detail::nothrow_less<detail::ElementValue<Derived>>) requires detail::ComparableElements<Derived> {
            return *std::ranges::min_element(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        SGCL_INLINE_HOT constexpr decltype(auto) min(Compare cmp) const noexcept(detail::nothrow_callback<Compare, detail::ElementReference<const Derived>, detail::ElementReference<const Derived>>) requires detail::ElementOrder<Compare, Derived> {
            return *std::ranges::min_element(_begin(), _end(), cmp);
        }

        SGCL_INLINE_HOT constexpr decltype(auto) max() const noexcept(detail::nothrow_less<detail::ElementValue<Derived>>) requires detail::ComparableElements<Derived> {
            return *std::ranges::max_element(_begin(), _end(), detail::Less{});
        }

        template<class Compare>
        SGCL_INLINE_HOT constexpr decltype(auto) max(Compare cmp) const noexcept(detail::nothrow_callback<Compare, detail::ElementReference<const Derived>, detail::ElementReference<const Derived>>) requires detail::ElementOrder<Compare, Derived> {
            return *std::ranges::max_element(_begin(), _end(), cmp);
        }

    protected:
        enumerable() = default;
        ~enumerable() = default;

    private:
        // What find_if gives for the iterator found: a pointer to the
        // element, or the value in an optional where the iterator gives
        // values; the copy of the value the one step that may throw
        template<class It, class End>
        SGCL_INLINE_HOT static constexpr auto _found(It it, End end) noexcept(std::is_lvalue_reference_v<std::iter_reference_t<It>> || std::is_nothrow_constructible_v<std::iter_value_t<It>, std::iter_reference_t<It>>) {
            if constexpr (std::is_lvalue_reference_v<std::iter_reference_t<It>>) {
                return it == end ? nullptr : &*it;
            } else {
                return it == end ? optional<std::iter_value_t<It>>() : optional<std::iter_value_t<It>>(*it);
            }
        }

        template<class D>
        SGCL_INLINE_HOT static constexpr bool _nothrow_found() noexcept {
            using It = std::ranges::iterator_t<D&>;
            return std::is_lvalue_reference_v<std::iter_reference_t<It>> || std::is_nothrow_constructible_v<std::iter_value_t<It>, std::iter_reference_t<It>>;
        }

        SGCL_INLINE_HOT constexpr auto _begin() noexcept { return static_cast<Derived&>(*this).begin(); }
        SGCL_INLINE_HOT constexpr auto _end() noexcept { return static_cast<Derived&>(*this).end(); }
        SGCL_INLINE_HOT constexpr auto _begin() const noexcept { return static_cast<const Derived&>(*this).begin(); }
        SGCL_INLINE_HOT constexpr auto _end() const noexcept { return static_cast<const Derived&>(*this).end(); }
    };
}

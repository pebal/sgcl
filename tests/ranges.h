//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The check every range of the library passes: its iterator
// models the concept of the category it declares (iterator_concept, else
// iterator_category; a pointer is contiguous), a type with size() is a
// sized_range, and the views of the standard library take it — filter,
// transform and take always, reverse when it is bidirectional.
// SGCL_CHECK_RANGE(concept, type) asserts that and the concept named.
#pragma once

#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace ranges_check {
    template<class I>
    auto declared_tag() {
        if constexpr (std::is_pointer_v<I>) {
            return std::contiguous_iterator_tag{};
        } else if constexpr (requires { typename I::iterator_concept; }) {
            return typename I::iterator_concept{};
        } else if constexpr (requires { typename I::iterator_category; }) {
            return typename I::iterator_category{};
        }
    }

    template<class I>
    constexpr bool models_declared() {
        using Tag = decltype(declared_tag<I>());
        if constexpr (std::is_void_v<Tag>) {
            return false;
        } else if constexpr (std::derived_from<Tag, std::contiguous_iterator_tag>) {
            return std::contiguous_iterator<I>;
        } else if constexpr (std::derived_from<Tag, std::random_access_iterator_tag>) {
            return std::random_access_iterator<I>;
        } else if constexpr (std::derived_from<Tag, std::bidirectional_iterator_tag>) {
            return std::bidirectional_iterator<I>;
        } else if constexpr (std::derived_from<Tag, std::forward_iterator_tag>) {
            return std::forward_iterator<I>;
        } else {
            return std::input_iterator<I>;
        }
    }

    inline constexpr auto any_value = [](const auto&) { return true; };
    inline constexpr auto same_value = [](auto&& v) -> decltype(auto) { return std::forward<decltype(v)>(v); };

    template<class R, class A>
    constexpr bool view_ok(A adaptor) {
        return requires(R& r) {
            r | adaptor;
            requires std::ranges::input_range<std::remove_reference_t<decltype(r | adaptor)>>;
        };
    }

    template<class R>
    constexpr bool ok() {
        return models_declared<std::ranges::iterator_t<R&>>()
            && std::ranges::input_range<R&>
            && (!requires(R& r) { r.size(); } || std::ranges::sized_range<R&>)
            && view_ok<R>(std::views::filter(any_value))
            && view_ok<R>(std::views::transform(same_value))
            && view_ok<R>(std::views::take(1))
            && (!std::ranges::bidirectional_range<R&> || view_ok<R>(std::views::reverse));
    }
}

#define SGCL_CHECK_RANGE(Concept, ...) \
    static_assert(Concept<__VA_ARGS__&> && ranges_check::ok<__VA_ARGS__>(), #__VA_ARGS__)

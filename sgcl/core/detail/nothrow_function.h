//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <functional>
#include <type_traits>

namespace sgcl::detail {
    // The function objects of the standard library that do not declare
    // their call noexcept (std::hash for some types, the comparisons of
    // <functional> always): accepted as they are, the operator they apply
    // being the key type's own business (DESIGN 356)
    template<class F>
    inline constexpr bool is_std_function_object = false;

    template<class T>
    inline constexpr bool is_std_function_object<std::hash<T>> = true;

    template<class T>
    inline constexpr bool is_std_function_object<std::equal_to<T>> = true;

    template<class T>
    inline constexpr bool is_std_function_object<std::not_equal_to<T>> = true;

    template<class T>
    inline constexpr bool is_std_function_object<std::less<T>> = true;

    template<class T>
    inline constexpr bool is_std_function_object<std::greater<T>> = true;

    template<class T>
    inline constexpr bool is_std_function_object<std::less_equal<T>> = true;

    template<class T>
    inline constexpr bool is_std_function_object<std::greater_equal<T>> = true;

    // The Hash, KeyEqual and Compare of a container: a call that throws is
    // a programming error, rejected at compile time (DESIGN 356). F called
    // with A, the key the container passes; the standard ones above pass.
    template<class F, class... A>
    inline constexpr bool nothrow_function_object = is_std_function_object<std::remove_cv_t<F>> || std::is_nothrow_invocable_v<F&, A...>;
}

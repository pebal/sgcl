//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The rule of the tests for a result that is an expected: a variable is
// checked first, ASSERT_TRUE/ASSERT_FALSE, and only then read (its value,
// or its error()); a result read at once, without a variable, goes
// through error_of() or value_of(). An unchecked error() of a value is
// terminate (a noexcept access to the wrong alternative), which ends the
// whole test program; these turn it into an exception, which gtest
// reports as the failure of the one test, with the place of the call.
// The expression is evaluated once, and the test stays one line:
// `EXPECT_EQ(error_of(json::parse<int>(text("1.5"))).code(), errc::type_mismatch)`.

#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace test_detail {
    inline std::string where(const std::source_location& at) {
        return std::string(at.file_name()) + ":" + std::to_string(at.line());
    }
}

// The error of a result the test says is one; a value there is the
// failure of the test
template<class X>
decltype(auto) error_of(X&& x, const std::source_location& at = std::source_location::current()) {
    if (x.has_value()) {
        throw std::logic_error(test_detail::where(at) + ": an error expected, a value found");
    }
    return std::forward<X>(x).error();
}

// The value of a result the test says is one; an error there is the
// failure of the test, with the error's message when it has one
template<class X>
decltype(auto) value_of(X&& x, const std::source_location& at = std::source_location::current()) {
    if (!x.has_value()) {
        std::ostringstream s;
        s << test_detail::where(at) << ": a value expected, an error found";
        if constexpr (requires(std::ostringstream& o) { o << x.error().message(); }) {
            s << ": " << x.error().message();
        }
        throw std::logic_error(s.str());
    }
    return *std::forward<X>(x);
}

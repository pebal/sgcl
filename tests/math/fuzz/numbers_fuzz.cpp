//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// big_integer and rational from any text, without an oracle. The first byte
// picks the path and the base; a second number follows a NUL. What must
// hold:
//   - a big_integer read in a base writes back in that base as the
//     canonical text of the input (no '+', no leading zeros, small
//     letters, "0" for any zero), which reads to the same number, and so
//     does its decimal text;
//   - with a second number, the ring holds: (a + b) - b == a,
//     (a * b) / b == a, a == (a / b) * b + a % b with |a % b| < |b|, the
//     order of a and b is the order of a - b against zero;
//   - a rational read writes as a fraction in lowest terms with a
//     positive denominator that reads to the same number, and the field
//     holds with a second one: (x + y) - y == x, (x * y) / y == x;
//   - a few bytes never ask for more than the parse's limits allow: an
//     input runs in bounded time (libFuzzer's -timeout says how long).
// Built with libFuzzer (tests/fuzz/run.sh tests/math/fuzz/numbers_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/math/math.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::math;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    std::string canonical(std::string_view text) {
        bool negative = false;
        if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
            negative = text[0] == '-';
            text.remove_prefix(1);
        }
        while (text.size() > 1 && text[0] == '0') {
            text.remove_prefix(1);
        }
        std::string out(text);
        for (auto& c : out) {
            if (c >= 'A' && c <= 'Z') {
                c = char(c + 32);
            }
        }
        if (out == "0") {
            return out;
        }
        return negative ? "-" + out : out;
    }

    void integers(int base, std::string_view a_text, std::string_view b_text) {
        auto a = big_integer::parse(string(a_text), base);
        if (!a) {
            return;
        }
        check(a->to_string(base).view() == canonical(a_text));
        auto again = big_integer::parse(a->to_string());
        check(again.has_value() && *again == *a);
        auto b = big_integer::parse(string(b_text), base);
        if (!b) {
            return;
        }
        check((*a + *b) - *b == *a);
        check((*a - *b) + *b == *a);
        auto order = *a <=> *b;
        auto diff = *a - *b;
        check((order < 0) == (diff < big_integer(0)) && (order == 0) == (diff == big_integer(0)));
        if (*b != big_integer(0)) {
            check((*a * *b) / *b == *a);
            auto q = *a / *b;
            auto r = *a % *b;
            check(q * *b + r == *a);
            check(r.abs() < b->abs());
        }
    }

    // The exponent of a decimal text, when it has one and it is all
    // digits: its magnitude, saturated
    uint64_t exponent_of(std::string_view t) {
        auto e = t.find_first_of("eE");
        if (e == std::string_view::npos) {
            return 0;
        }
        auto d = t.substr(e + 1);
        if (!d.empty() && (d[0] == '+' || d[0] == '-')) {
            d.remove_prefix(1);
        }
        uint64_t v = 0;
        for (char c : d) {
            if (c < '0' || c > '9') {
                return 0;
            }
            v = v > 100000000 ? v : v * 10 + uint64_t(c - '0');
        }
        return v;
    }

    void rationals(std::string_view x_text, std::string_view y_text) {
        // the limit: an exponent past a million either way is refused
        // (a few bytes would ask for megabytes); one below it is taken, and
        // the round trip is asked only of the ones that stay small — a
        // million digits written is 47 ms in Release and past libFuzzer's
        // five seconds under ASan
        if (exponent_of(x_text) > 1000000 || exponent_of(y_text) > 1000000) {
            check(!rational::parse(string(exponent_of(x_text) > 1000000 ? x_text : y_text)));
            return;
        }
        if (exponent_of(x_text) > 20000 || exponent_of(y_text) > 20000) {
            (void)rational::parse(string(x_text));
            return;
        }
        auto x = rational::parse(string(x_text));
        if (!x) {
            check(x.error().offset() <= x_text.size());
            return;
        }
        check(x->denominator() > big_integer(0));
        check(x->numerator().gcd(x->denominator()) == big_integer(1) || x->numerator() == big_integer(0));
        string text = x->to_string();
        auto again = rational::parse(text);
        check(again.has_value() && *again == *x);
        check(again->to_string() == text);
        auto y = rational::parse(string(y_text));
        if (!y) {
            return;
        }
        check((*x + *y) - *y == *x);
        if (y->numerator() != big_integer(0)) {
            check((*x * *y) / *y == *x);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    size_t nul = rest.find('\0');
    std::string_view first = rest.substr(0, nul);
    std::string_view second = nul == std::string_view::npos ? std::string_view() : rest.substr(nul + 1);
    if (mode & 1) {
        rationals(first, second);
    } else {
        int base = 2 + (mode >> 1) % 35;
        if ((mode >> 1) >= 105) {
            base = 10;
        }
        integers(base, first, second);
    }
    return 0;
}

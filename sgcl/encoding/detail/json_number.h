//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "text_scan.h"
#include "../../core/aliases.h"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // The numbers of JSON (RFC 8259 section 6): the grammar, scanned in
    // pieces when the text comes in pieces; the value of a literal as an
    // integer (exactly, or not at all), a double or a float (each rounded
    // once, from the decimal: a float is never a double rounded again);
    // and the text of a double or a float as ECMAScript's Number::toString
    // lays it out — what JSON.stringify and Go write.

    // Where a scan of a number is: the states of the grammar
    //   -? (0 | [1-9][0-9]*) (. [0-9]+)? ([eE] [+-]? [0-9]+)?
    enum class NumberState : uint8_t {
        start,          // nothing yet
        sign,           // after '-'
        zero,           // after the leading 0: the integer part is done
        integer,        // in the digits of the integer part
        point,          // after '.', a digit wanted
        fraction,       // in the digits of the fraction
        exponent,       // after 'e', a sign or a digit wanted
        exponent_sign,  // after the sign, a digit wanted
        exponent_digits
    };

    constexpr bool number_can_end(NumberState s) noexcept {
        return s == NumberState::zero || s == NumberState::integer || s == NumberState::fraction || s == NumberState::exponent_digits;
    }

    enum class ScanStatus : uint8_t {
        done,       // the token ends before end (or at the end of the input)
        more,       // the data ends inside the token: more is needed
        failed      // the byte at p cannot be here
    };

    inline bool is_digit(char c) noexcept {
        return uint8_t(c - '0') < 10;
    }

    // One step of a number: from p, in state s, to the first byte that
    // does not continue it. done: p is past the number; more: the data
    // ended and the input has more (last is false), s is where to go on
    // from; failed: p is the byte that breaks the grammar (the end, when
    // the input ended inside the number). `plain` is cleared at a '.' or
    // an 'e': the literal is not an integer's.
    // The digits from p: eight at a time while eight are there
    inline const char* skip_digits(const char* p, const char* end) noexcept {
        while (end - p >= 8) {
            uint64_t w = load_word(p);
            // a byte is a digit when its high half is 3 and adding 6 keeps
            // it so; a carry out of a byte comes from one that is not a
            // digit, below the bytes it spoils
            uint64_t bad = ((w & 0xF0F0F0F0F0F0F0F0ull) ^ 0x3030303030303030ull) | (((w + 0x0606060606060606ull) & 0xF0F0F0F0F0F0F0F0ull) ^ 0x3030303030303030ull);
            if (bad) {
                return p + (std::countr_zero(bad) >> 3);
            }
            p += 8;
        }
        while (p != end && is_digit(*p)) {
            ++p;
        }
        return p;
    }

    inline ScanStatus scan_number(const char*& p, const char* end, bool last, NumberState& s, bool& plain) noexcept {
        // the common case first: a whole number before the end of the
        // data, in one straight pass; anything else goes by the states
        if (s == NumberState::start) {
            const char* q = p;
            q += q != end && *q == '-';
            if (q != end && *q == '0') {
                ++q;
            } else if (q != end && *q >= '1' && *q <= '9') {
                q = skip_digits(q + 1, end);
            } else {
                goto states;
            }
            bool fraction = false;
            if (q != end && *q == '.') {
                if (q + 1 == end || !is_digit(q[1])) {
                    goto states;
                }
                q = skip_digits(q + 2, end);
                fraction = true;
            }
            if (q != end && (*q == 'e' || *q == 'E')) {
                goto states;
            }
            if (q == end && !last) {
                goto states;   // the data may go on with more digits
            }
            plain = !fraction;
            p = q;
            return ScanStatus::done;
        }
    states:
        for (;;) {
            if (p == end) {
                if (!last) {
                    return ScanStatus::more;
                }
                return number_can_end(s) ? ScanStatus::done : ScanStatus::failed;
            }
            char c = *p;
            switch (s) {
                case NumberState::start:
                    if (c == '-') {
                        s = NumberState::sign;
                        ++p;
                        continue;
                    }
                    [[fallthrough]];
                case NumberState::sign:
                    if (c == '0') {
                        s = NumberState::zero;
                    } else if (c >= '1' && c <= '9') {
                        s = NumberState::integer;
                    } else {
                        return ScanStatus::failed;
                    }
                    ++p;
                    continue;
                case NumberState::integer:
                    p = skip_digits(p, end);
                    if (p == end) {
                        continue;
                    }
                    c = *p;
                    [[fallthrough]];
                case NumberState::zero:
                    if (c == '.') {
                        s = NumberState::point;
                        plain = false;
                        ++p;
                        continue;
                    }
                    if (c == 'e' || c == 'E') {
                        s = NumberState::exponent;
                        plain = false;
                        ++p;
                        continue;
                    }
                    return ScanStatus::done;
                case NumberState::point:
                    if (!is_digit(c)) {
                        return ScanStatus::failed;
                    }
                    s = NumberState::fraction;
                    ++p;
                    continue;
                case NumberState::fraction:
                    p = skip_digits(p, end);
                    if (p == end) {
                        continue;
                    }
                    if (*p == 'e' || *p == 'E') {
                        s = NumberState::exponent;
                        ++p;
                        continue;
                    }
                    return ScanStatus::done;
                case NumberState::exponent:
                    if (c == '+' || c == '-') {
                        s = NumberState::exponent_sign;
                        ++p;
                        continue;
                    }
                    [[fallthrough]];
                case NumberState::exponent_sign:
                    if (!is_digit(c)) {
                        return ScanStatus::failed;
                    }
                    s = NumberState::exponent_digits;
                    ++p;
                    continue;
                case NumberState::exponent_digits:
                    while (p != end && is_digit(*p)) {
                        ++p;
                    }
                    if (p == end) {
                        continue;
                    }
                    return ScanStatus::done;
            }
        }
    }

    // The value of an integer literal (no '.', no exponent) when an int64
    // or a uint64 holds it: the kind says which, none when neither does
    enum class IntegerFit : uint8_t {
        none,
        int64,
        uint64
    };

    struct IntegerValue {
        IntegerFit fit = IntegerFit::none;
        int64_t i = 0;
        uint64_t u = 0;
    };

    inline IntegerValue integer_of(std::string_view lit) noexcept {
        IntegerValue r;
        bool negative = !lit.empty() && lit[0] == '-';
        size_t i = negative;
        uint64_t v = 0;
        size_t n = lit.size();
        // up to 19 digits cannot overflow a uint64
        size_t fast = std::min(n, i + 19);
        for (; i < fast; ++i) {
            v = v * 10 + uint64_t(lit[i] - '0');
        }
        for (; i < n; ++i) {
            uint64_t d = uint64_t(lit[i] - '0');
            if (v > (UINT64_MAX - d) / 10) {
                return r;
            }
            v = v * 10 + d;
        }
        if (negative) {
            if (v <= uint64_t(INT64_MAX) + 1) {
                r.fit = IntegerFit::int64;
                r.i = v == uint64_t(INT64_MAX) + 1 ? INT64_MIN : -int64_t(v);
            }
        } else if (v <= uint64_t(INT64_MAX)) {
            r.fit = IntegerFit::int64;
            r.i = int64_t(v);
        } else {
            r.fit = IntegerFit::uint64;
            r.u = v;
        }
        return r;
    }

    // The parts of a valid literal: its sign, its significant digits
    // (the integer and the fraction's, leading and trailing zeros taken
    // off) and the power of ten of the last of them; zero has no digits
    struct Decimal {
        bool negative = false;
        std::string_view integer;   // the digits before the point
        std::string_view fraction;  // after it
        int64_t exponent = 0;       // the literal's own, saturated at +-2^40
    };

    inline Decimal decimal_of(std::string_view lit) noexcept {
        Decimal d;
        size_t i = 0;
        if (i < lit.size() && lit[i] == '-') {
            d.negative = true;
            ++i;
        }
        size_t s = i;
        while (i < lit.size() && is_digit(lit[i])) {
            ++i;
        }
        d.integer = lit.substr(s, i - s);
        if (i < lit.size() && lit[i] == '.') {
            s = ++i;
            while (i < lit.size() && is_digit(lit[i])) {
                ++i;
            }
            d.fraction = lit.substr(s, i - s);
        }
        if (i < lit.size() && (lit[i] == 'e' || lit[i] == 'E')) {
            ++i;
            bool minus = false;
            if (lit[i] == '+' || lit[i] == '-') {
                minus = lit[i] == '-';
                ++i;
            }
            int64_t e = 0;
            for (; i < lit.size(); ++i) {
                if (e < (int64_t(1) << 40)) {
                    e = e * 10 + (lit[i] - '0');
                }
            }
            d.exponent = minus ? -e : e;
        }
        return d;
    }

    // The literal's value as an integer when it is one and the type holds
    // it exactly: 100, 1e2, 100.0 and 1.00e2 are all the integer 100;
    // 1.5, 1e-1 and 2^64 into an int64 are not. No rounding anywhere.
    template<class T>
    optional<T> exact_integer(std::string_view lit) noexcept {
        static_assert(std::is_integral_v<T>);
        Decimal d = decimal_of(lit);
        // value = int(integer fraction) * 10^(exponent - |fraction|), the
        // fraction's trailing zeros taken off first (they only scale)
        std::string_view a = d.integer, b = d.fraction;
        while (!b.empty() && b.back() == '0') {
            b.remove_suffix(1);
        }
        int64_t shift = d.exponent - int64_t(b.size());
        // the digits a b, their leading zeros off
        while (!a.empty() && a.front() == '0') {
            a.remove_prefix(1);
        }
        if (a.empty()) {
            while (!b.empty() && b.front() == '0') {
                b.remove_prefix(1);
            }
        }
        size_t count = a.size() + b.size();
        if (count == 0) {
            return T(0);
        }
        auto digit_at = [&](size_t k) -> char { return k < a.size() ? a[k] : b[k - a.size()]; };
        // a negative shift is paid by trailing zeros of the digits
        while (shift < 0 && digit_at(count - 1) == '0') {
            --count;
            ++shift;
        }
        if (shift < 0 || int64_t(count) + shift > 20) {
            return nullopt;
        }
        uint64_t v = 0;
        for (size_t k = 0; k < count; ++k) {
            uint64_t dgt = uint64_t(digit_at(k) - '0');
            if (v > (UINT64_MAX - dgt) / 10) {
                return nullopt;
            }
            v = v * 10 + dgt;
        }
        for (int64_t z = 0; z < shift; ++z) {
            if (v > UINT64_MAX / 10) {
                return nullopt;
            }
            v *= 10;
        }
        if (d.negative) {
            if constexpr (std::is_signed_v<T>) {
                if (v > uint64_t(std::numeric_limits<T>::max()) + 1) {
                    return nullopt;
                }
                return v == 0 ? T(0) : T(-int64_t(v - 1) - 1);
            } else {
                if (v != 0) {
                    return nullopt;
                }
                return T(0);
            }
        }
        if (v > uint64_t(std::numeric_limits<T>::max())) {
            return nullopt;
        }
        return T(v);
    }

    // Whether a literal the parser rounded out of range was too small
    // (then it is zero, as Go reads it) and not too large: the power of
    // ten of its first significant digit
    inline bool underflows(std::string_view lit) noexcept {
        Decimal d = decimal_of(lit);
        std::string_view a = d.integer;
        while (!a.empty() && a.front() == '0') {
            a.remove_prefix(1);
        }
        int64_t magnitude;
        if (!a.empty()) {
            magnitude = int64_t(a.size()) + d.exponent;
        } else {
            size_t zeros = 0;
            while (zeros < d.fraction.size() && d.fraction[zeros] == '0') {
                ++zeros;
            }
            if (zeros == d.fraction.size()) {
                return true;   // zero itself
            }
            magnitude = d.exponent - int64_t(zeros);
        }
        return magnitude <= 0;
    }

    // A floating literal (valid JSON) rounded once to F; nullopt when it
    // is too large for F. Too small is zero with the literal's sign.
    template<class F>
    optional<F> floating_of(std::string_view lit) noexcept {
        if constexpr (std::is_same_v<F, double>) {
            // Clinger's fast path: a literal of the plain form (-, digits, a
            // point and digits, an exponent) whose digits make an integer
            // m <= 2^53 and whose power of ten is within 10^±22 — m and the
            // power are both exact doubles, so one multiplication or
            // division, rounded once by the hardware, is the nearest double.
            // Most numbers of data are such (12.375, 0.5, 1e3); the rest go
            // to from_chars, which also refuses what is not a number. A
            // literal of more than 16 characters goes there at once: its
            // digits are too many for m (the coordinates of canada.json,
            // 17 digits each, were read twice otherwise, 7.8 -> 9.2 ms).
            if (lit.size() <= 16) {
                const char* p = lit.data();
                const char* end = p + lit.size();
                bool negative = p != end && *p == '-';
                p += negative;
                uint64_t m = 0;
                int digits = 0;
                int scale = 0;
                const char* first = p;
                while (p != end && unsigned(*p - '0') < 10) {
                    m = m * 10 + unsigned(*p - '0');
                    digits += m != 0;
                    ++p;
                }
                bool plain = p != first && digits <= 19;
                if (plain && p != end && *p == '.') {
                    const char* fraction = ++p;
                    while (p != end && unsigned(*p - '0') < 10) {
                        m = m * 10 + unsigned(*p - '0');
                        digits += m != 0;
                        --scale;
                        ++p;
                    }
                    plain = p != fraction && digits <= 19;
                }
                if (plain && p != end && (*p == 'e' || *p == 'E')) {
                    ++p;
                    bool down = p != end && *p == '-';
                    p += p != end && (*p == '-' || *p == '+');
                    int e = 0;
                    while (p != end && unsigned(*p - '0') < 10) {
                        e = e * 10 + (*p - '0');
                        ++p;
                    }
                    scale += down ? -e : e;
                }
                if (plain && p == end && m <= (uint64_t(1) << 53) && scale >= -22 && scale <= 22) {
                    static constexpr double Powers[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
                                                        1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
                    double v = double(m);
                    v = scale < 0 ? v / Powers[-scale] : v * Powers[scale];
                    return negative ? -v : v;
                }
            }
        }
        F v{};
        auto [end, ec] = std::from_chars(lit.data(), lit.data() + lit.size(), v, std::chars_format::general);
        if (ec == std::errc()) {
            return v;
        }
        if (ec == std::errc::result_out_of_range && underflows(lit)) {
            return lit[0] == '-' ? -F(0) : F(0);
        }
        return nullopt;
    }

    // The most characters number_text writes: a sign, 17 digits, a point,
    // "e-324", and room for the zeros of the fixed forms (up to 21 + 6)
    inline constexpr size_t NumberTextSize = 40;

    // The text of a finite double or float as ECMAScript lays it out: the
    // shortest digits that read back to the same value (to_chars), fixed
    // from 1e-6 to 1e21 and with an exponent outside (1e+21, 1e-7), no
    // ".0" on an integer, -0 as "-0". The characters written.
    template<class F>
    size_t number_text(char* out, F x) noexcept {
        static_assert(std::is_floating_point_v<F>);
        char* o = out;
        if (x == 0) {
            if (std::signbit(x)) {
                *o++ = '-';
            }
            *o++ = '0';
            return size_t(o - out);
        }
        if (x < 0) {
            *o++ = '-';
            x = -x;
        }
        // d.ddddde[+-]XX: the digits and the exponent
        char buf[48];
        auto r = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::scientific);
        char digits[24];
        int k = 0;
        const char* q = buf;
        for (; q != r.ptr && *q != 'e'; ++q) {
            if (*q != '.') {
                digits[k++] = *q;
            }
        }
        int e10 = 0;
        if (q != r.ptr) {
            ++q;
            bool minus = *q == '-';
            if (*q == '-' || *q == '+') {
                ++q;
            }
            for (; q != r.ptr; ++q) {
                e10 = e10 * 10 + (*q - '0');
            }
            if (minus) {
                e10 = -e10;
            }
        }
        while (k > 1 && digits[k - 1] == '0') {
            --k;
        }
        // x = 0.digits * 10^n, as ECMAScript's n
        int n = e10 + 1;
        if (k <= n && n <= 21) {
            std::memcpy(o, digits, size_t(k));
            o += k;
            for (int z = k; z < n; ++z) {
                *o++ = '0';
            }
        } else if (0 < n && n <= 21) {
            std::memcpy(o, digits, size_t(n));
            o += n;
            *o++ = '.';
            std::memcpy(o, digits + n, size_t(k - n));
            o += k - n;
        } else if (-6 < n && n <= 0) {
            *o++ = '0';
            *o++ = '.';
            for (int z = 0; z < -n; ++z) {
                *o++ = '0';
            }
            std::memcpy(o, digits, size_t(k));
            o += k;
        } else {
            *o++ = digits[0];
            if (k > 1) {
                *o++ = '.';
                std::memcpy(o, digits + 1, size_t(k - 1));
                o += k - 1;
            }
            *o++ = 'e';
            int e = n - 1;
            *o++ = e < 0 ? '-' : '+';
            if (e < 0) {
                e = -e;
            }
            char eb[8];
            int en = 0;
            do {
                eb[en++] = char('0' + e % 10);
                e /= 10;
            } while (e);
            while (en) {
                *o++ = eb[--en];
            }
        }
        return size_t(o - out);
    }

    // The text of an integer
    template<class I>
    size_t integer_text(char* out, I v) noexcept {
        auto r = std::to_chars(out, out + NumberTextSize, v);
        return size_t(r.ptr - out);
    }
}

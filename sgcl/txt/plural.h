//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"
#include "locale.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <type_traits>

// Which form of a word a number takes in a language: "1 plik", "2 pliki",
// "5 plików" — the plural rules of CLDR (TR35 Part 3 §5), cardinal and
// ordinal, for every language CLDR has rules for. A rule asks about the
// number as it is written, not as a value: "1" and "1.0" are different
// numbers to English ("1 file", "1.0 files"), so the forms that take a
// fraction take it as text, or as the shortest digits of a double.
namespace sgcl::txt {
    // The six categories CLDR names; a language uses some of them, and
    // "other" always
    enum class plural : uint8_t {
        zero,
        one,
        two,
        few,
        many,
        other,
    };

    namespace detail::cldr {
        // The operands of TR35 §5.1: i the integer digits, v and w the
        // count of the visible fraction digits with and without the
        // trailing zeros, f and t those digits as an integer, e the
        // exponent of a compact form ("1.2c6"), and whether the number
        // has a fraction at all, which n is asked through. An integer of
        // more than eighteen digits keeps its last eighteen and 10^18 on
        // top: every modulus of a rule divides 10^18, and no constant of
        // a rule reaches it.
        struct PluralOperands {
            uint64_t i = 0;
            uint64_t f = 0;
            uint64_t t = 0;
            uint32_t v = 0;
            uint32_t w = 0;
            uint32_t e = 0;
            bool fraction = false;
        };

        using PluralRule = txt::plural (*)(const PluralOperands&) noexcept;
    }
}

#include "detail/cldr_plurals.h"

namespace sgcl::txt {
    namespace detail::cldr {
        inline constexpr uint64_t PluralBig = 1000000000000000000ull;

        // The operands of a number written as decimal text: an optional
        // sign, digits, a point and digits, and a compact exponent after
        // 'c' or 'e'. Anything else ends the number where it stands.
        inline PluralOperands operands_of(std::string_view s) noexcept {
            PluralOperands o;
            size_t p = 0;
            if (p < s.size() && (s[p] == '-' || s[p] == '+')) {
                ++p;
            }
            // the digits, the point's position among them, and the exponent
            char digits[64];
            size_t n = 0, point = std::string_view::npos, skipped = 0;
            for (; p < s.size(); ++p) {
                char c = s[p];
                if (c >= '0' && c <= '9') {
                    if (n < sizeof digits) {
                        digits[n++] = c;
                    } else if (point == std::string_view::npos) {
                        ++skipped;   // integer digits past 64: only how many there are counts
                    }
                } else if (c == '.' && point == std::string_view::npos) {
                    point = n;
                } else {
                    break;
                }
            }
            uint32_t exponent = 0;
            if (p < s.size() && (s[p] == 'c' || s[p] == 'e' || s[p] == 'C' || s[p] == 'E')) {
                for (++p; p < s.size() && s[p] >= '0' && s[p] <= '9'; ++p) {
                    exponent = exponent < 1000 ? exponent * 10 + uint32_t(s[p] - '0') : exponent;
                }
            }
            if (point == std::string_view::npos) {
                point = n;
            }
            // the compact exponent moves the point: 1.2c3 is 1200
            size_t int_digits = point + exponent;
            o.e = exponent;
            uint64_t i = 0;
            bool big = skipped > 0;
            for (size_t k = 0; k < int_digits; ++k) {
                char c = k < n ? digits[k] : '0';
                i = i * 10 + uint64_t(c - '0');
                if (i >= PluralBig) {
                    big = true;
                    i %= PluralBig;
                }
            }
            o.i = big ? i + PluralBig : i;
            if (int_digits < n) {
                o.v = uint32_t(n - int_digits);
                uint64_t f = 0;
                size_t last = int_digits;
                for (size_t k = int_digits; k < n; ++k) {
                    f = f < PluralBig ? f * 10 + uint64_t(digits[k] - '0') : f;
                    if (digits[k] != '0') {
                        last = k + 1;
                    }
                }
                o.f = f;
                o.w = uint32_t(last - int_digits);
                uint64_t t = f;
                for (uint32_t k = o.w; k < o.v; ++k) {
                    t /= 10;
                }
                o.t = t;
                o.fraction = o.f != 0;
            }
            return o;
        }

        inline PluralOperands operands_of(double x) noexcept {
            if (!std::isfinite(x)) {
                PluralOperands o;
                o.i = PluralBig;
                return o;
            }
            char text[40];
            auto r = std::to_chars(text, text + sizeof text, std::fabs(x));
            std::string_view s(text, size_t(r.ptr - text));
            // the shortest form may be written with an exponent: 1e+20
            size_t at = s.find('e');
            if (at == std::string_view::npos) {
                return operands_of(s);
            }
            int exp = 0;
            std::from_chars(s.data() + at + 1 + (s[at + 1] == '+'), s.data() + s.size(), exp);
            std::string_view mantissa = s.substr(0, at);
            char shifted[400];
            size_t n = 0;
            if (exp >= 0) {
                // digits with the point moved right
                size_t point = mantissa.find('.');
                std::string_view whole = mantissa.substr(0, point);
                std::string_view frac = point == std::string_view::npos ? std::string_view() : mantissa.substr(point + 1);
                for (char c : whole) {
                    shifted[n++] = c;
                }
                for (int k = 0; k < exp; ++k) {
                    shifted[n++] = size_t(k) < frac.size() ? frac[size_t(k)] : '0';
                }
                if (size_t(exp) < frac.size()) {
                    shifted[n++] = '.';
                    for (size_t k = size_t(exp); k < frac.size(); ++k) {
                        shifted[n++] = frac[k];
                    }
                }
            } else {
                shifted[n++] = '0';
                shifted[n++] = '.';
                for (int k = 1; k < -exp && n < sizeof shifted - 32; ++k) {
                    shifted[n++] = '0';
                }
                for (char c : mantissa) {
                    if (c != '.' && n < sizeof shifted) {
                        shifted[n++] = c;
                    }
                }
            }
            return operands_of(std::string_view(shifted, n));
        }

        template<class T>
        SGCL_INLINE_HOT PluralOperands operands_of_integer(T n) noexcept {
            PluralOperands o;
            uint64_t a;
            if constexpr (std::is_signed_v<T>) {
                a = n < 0 ? uint64_t(0) - uint64_t(n) : uint64_t(n);
            } else {
                a = uint64_t(n);
            }
            o.i = a >= PluralBig ? a % PluralBig + PluralBig : a;
            return o;
        }

        // The rule sets of a locale: cardinal << 8 | ordinal, by the
        // language and region, else the language, else root
        inline uint16_t plural_sets(const locale& l) noexcept {
            uint64_t key = LocaleAccess::key(l);
            uint32_t language = LocaleAccess::language(key);
            if (uint32_t region = LocaleAccess::region(key)) {
                size_t i = find(PluralKeys, LocaleAccess::compose(language, 0, region));
                if (i != std::size(PluralKeys)) {
                    return PluralSets[i];
                }
            }
            size_t i = find(PluralKeys, LocaleAccess::compose(language, 0, 0));
            if (i == std::size(PluralKeys)) {
                i = find(PluralKeys, uint64_t(0));
            }
            return PluralSets[i];
        }

        SGCL_INLINE_HOT plural cardinal(const PluralOperands& o, const locale& l) noexcept {
            return Cardinals[plural_sets(l) >> 8](o);
        }

        SGCL_INLINE_HOT plural ordinal(const PluralOperands& o, const locale& l) noexcept {
            return Ordinals[plural_sets(l) & 0xFF](o);
        }
    }

    // The cardinal category of an integer: "5 plików" is many in Polish
    template<class T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    SGCL_INLINE_HOT plural plural_of(T n, const locale& l) noexcept {
        return detail::cldr::cardinal(detail::cldr::operands_of_integer(n), l);
    }

    // The cardinal category of a double, by its shortest decimal digits:
    // 1.5 has one fraction digit, 1.0 is the integer 1
    inline plural plural_of(double n, const locale& l) noexcept {
        return detail::cldr::cardinal(detail::cldr::operands_of(n), l);
    }

    // The cardinal category of a number written as decimal text, as it
    // will be shown: "1.50" has two fraction digits, "1.2c6" is a compact
    // 1.2 million
    inline plural plural_of(const string& number, const locale& l) noexcept {
        return detail::cldr::cardinal(detail::cldr::operands_of(number.view()), l);
    }

    // The ordinal category of an integer: 2 is two in English ("2nd")
    template<class T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    SGCL_INLINE_HOT plural ordinal_of(T n, const locale& l) noexcept {
        return detail::cldr::ordinal(detail::cldr::operands_of_integer(n), l);
    }
}

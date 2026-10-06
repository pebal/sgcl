//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/rounding.h"
#include "big_integer.h"
#include "rational.h"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

// A decimal number of any precision: a whole number of any size, the
// unscaled part, and a scale, the value being unscaled × 10^-scale — what
// java.math.BigDecimal is, and PostgreSQL's NUMERIC, which a program
// reading money, measures and database columns needs exactly as written:
// 0.1 + 0.2 is 0.3, and 1.50 keeps its two places.
//
// The unscaled part is a big_integer, so a value of up to eighteen digits
// lives in its word and allocates nothing; the scale is an int32_t,
// positive for digits after the point, negative for zeros before it
// (1.5e3 read from text is 15 at scale -2). +, - and * are exact, as for
// big_integer and rational; a quotient, a square root and a rounding to
// fewer places take the scale or the number of digits wanted and the
// rounding, explicitly (rounding::half_even when not given).
//
// NaN and the two infinities are values of the type, because PostgreSQL's
// NUMERIC has them (NaN always, the infinities since version 14) and a
// column holding them must read back whole. They follow PostgreSQL:
// arithmetic on finite values never makes one (a division by zero is
// domain_error, as for rational), NaN goes through every operation,
// inf - inf and 0 × inf are NaN, and NaN equals NaN and orders above
// every other value, so that ==, <=> and the hash are one total order:
// -inf < every finite value < +inf < NaN. There is no -0 and no
// signalling NaN, which PostgreSQL does not have either.
//
// Equality is of the values: 1.0 == 1.00, and the two hash alike;
// identical() asks for the same scale as well. A decimal holds a
// tracked_ptr (the big_integer's), so it lives where one may.
namespace sgcl::math {
    namespace detail {
        // 10^0 … 10^18, every power of ten an int64_t holds
        inline constexpr int64_t Pow10[19] = {1,
                                              10,
                                              100,
                                              1000,
                                              10000,
                                              100000,
                                              1000000,
                                              10000000,
                                              100000000,
                                              1000000000,
                                              10000000000,
                                              100000000000,
                                              1000000000000,
                                              10000000000000,
                                              100000000000000,
                                              1000000000000000,
                                              10000000000000000,
                                              100000000000000000,
                                              1000000000000000000};

        // The decimal digits of m, 1 for zero
        SGCL_INLINE_HOT int decimal_digits(uint64_t m) noexcept {
            // floor(log10(m)) from the bit length, corrected by one look
            // at the table: 1233/4096 is just above log10(2)
            int bits = 64 - std::countl_zero(m | 1);
            int d = (bits * 1233) >> 12;
            if (d < 19 && m >= uint64_t(Pow10[d])) {
                ++d;
            } else if (d == 19 && m >= 10000000000000000000ull) {
                ++d;
            }
            return d ? d : 1;
        }

        using Wide = unsigned __int128;

        // 10^0 … 10^38, every power of ten 128 bits hold
        inline constexpr auto Pow10Wide = [] {
            std::array<Wide, 39> p{};
            p[0] = 1;
            for (size_t i = 1; i < p.size(); ++i) {
                p[i] = p[i - 1] * 10;
            }
            return p;
        }();

        // The decimal digits of m, 1 for zero
        SGCL_INLINE_HOT int decimal_digits_wide(Wide m) noexcept {
            if (!(m >> 64)) {
                return decimal_digits(uint64_t(m));
            }
            int bits = 128 - std::countl_zero(uint64_t(m >> 64));
            int d = (bits * 1233) >> 12;
            return m >= Pow10Wide[size_t(d)] ? d + 1 : d;
        }

        // The magnitude of u when it fits 128 bits
        SGCL_INLINE_HOT bool magnitude_wide(const big_integer& u, Wide& m) noexcept {
            size_t n;
            Limb room;
            const Limb* p = BigIntAccess::magnitude(u, room, n);
            if (n > 2) {
                return false;
            }
            m = n == 2 ? (Wide(p[1]) << 64) | p[0] : n ? p[0] : 0;
            return true;
        }

        // 10^k, k >= 0: from the table up to 10^38, past it 5^k shifted by
        // k, a third fewer bits to raise
        inline big_integer pow10(int64_t k) noexcept {
            if (k <= 18) {
                return Pow10[k];
            }
            if (k <= 38) {
                return big_integer(Pow10Wide[size_t(k)]);
            }
            return big_integer(5).pow(k) << k;
        }

        // u · 10^k, k >= 0; within int64_t on the processor's own numbers
        SGCL_INLINE_HOT big_integer times_pow10(const big_integer& u, int64_t k) noexcept {
            if (k <= 18) {
                if (auto v = u.to_int64()) {
                    return big_integer(__int128(*v) * Pow10[k]);
                }
                return u * Pow10[k];
            }
            return u * pow10(k);
        }

        // The decimal digits of |u|, 1 for zero
        inline size_t decimal_digits(const big_integer& u) noexcept {
            if (auto v = u.to_int64()) {
                return size_t(decimal_digits(*v < 0 ? uint64_t(0) - uint64_t(*v) : uint64_t(*v)));
            }
            if (Wide m; magnitude_wide(u, m)) {
                return size_t(decimal_digits_wide(m));
            }
            // (bits - 1)·log10(2), taken a little low so that it is never
            // above the true count: the digits are then this or one more
            // (or, past the double's rounding, two more)
            size_t bits = u.bit_length();
            auto d = size_t(double(bits - 1) * 0.30102999566398120 - 1e-6) + 1;
            big_integer a = u.abs();
            big_integer p = pow10(int64_t(d));
            while (a >= p) {
                ++d;
                p *= 10;
            }
            return d;
        }

        // Whether |q| goes one further from zero: the dropped part against
        // a half (cmp: below, a tie, above), whether anything was dropped,
        // the sign of the exact value and the parity of q
        SGCL_INLINE_HOT bool round_away(rounding mode, bool negative, bool odd, int cmp, bool inexact) {
            switch (mode) {
                case rounding::half_even:
                    return cmp > 0 || (cmp == 0 && odd);
                case rounding::half_up:
                    return cmp >= 0 && inexact;
                case rounding::half_down:
                    return cmp > 0;
                case rounding::up:
                    return inexact;
                case rounding::down:
                    return false;
                case rounding::ceiling:
                    return inexact && !negative;
                case rounding::floor:
                    return inexact && negative;
                case rounding::unnecessary:
                    if (inexact) {
                        throw domain_error("sgcl::math::decimal: rounding::unnecessary, and the result is not exact");
                    }
                    return false;
            }
            return false;
        }

        // n / d rounded to a whole number, d != 0; sticky says the true
        // dividend is a little more than n in magnitude (something below
        // it was cut off), so a remainder of zero is not exact and a tie
        // is above the half
        inline big_integer round_div(const big_integer& n, const big_integer& d, rounding mode, bool sticky = false) {
            auto sn = n.to_int64();
            auto sd = d.to_int64();
            if (sn && sd) {
                __int128 a = *sn;
                __int128 b = *sd;
                __int128 q = a / b;
                __int128 r = a % b;
                bool negative = (a < 0) != (b < 0);
                auto ar = (unsigned __int128)(r < 0 ? -r : r);
                auto ab = (unsigned __int128)(b < 0 ? -b : b);
                int cmp = 2 * ar < ab ? -1 : 2 * ar == ab ? 0 : 1;
                if (sticky && cmp == 0) {
                    cmp = 1;
                }
                if (round_away(mode, negative, q & 1, cmp, ar != 0 || sticky)) {
                    q += negative ? -1 : 1;
                }
                return big_integer(q);
            }
            auto [q, r] = n.div_rem(d);
            bool negative = (n.sign() < 0) != (d.sign() < 0);
            auto c = (r.abs() << 1) <=> d.abs();
            int cmp = c < 0 ? -1 : c == 0 ? 0 : 1;
            if (sticky && cmp == 0) {
                cmp = 1;
            }
            if (round_away(mode, negative, q.bit(0), cmp, r.sign() != 0 || sticky)) {
                q += negative ? -1 : 1;
            }
            return q;
        }

        // n / (d · 10^k) rounded, d != 0, k >= 0: when n is too short to
        // reach a tenth of the divisor the quotient is zero and what is
        // dropped is below a half, without making 10^k
        inline big_integer round_div_pow10(const big_integer& n, const big_integer& d, int64_t k, rounding mode) {
            if (k > 18 && int64_t(decimal_digits(n)) + 1 < int64_t(decimal_digits(d)) + k) {
                bool negative = (n.sign() < 0) != (d.sign() < 0);
                bool inexact = n.sign() != 0;
                return round_away(mode, negative, false, -1, inexact) ? big_integer(negative ? -1 : 1) : big_integer();
            }
            if (k == 0) {
                return round_div(n, d, mode);
            }
            return round_div(n, times_pow10(d, k), mode);
        }

        // A scale computed in 64 bits, refused past int32_t
        SGCL_INLINE_HOT int32_t checked_scale(int64_t scale) {
            if (scale < INT32_MIN || scale > INT32_MAX) {
                throw length_error("sgcl::math::decimal: a scale past int32_t");
            }
            return int32_t(scale);
        }

        // q without its trailing zeros while the scale stays above
        // `lowest`: the scale goes down by one for each
        inline void strip_zeros(big_integer& q, int64_t& scale, int64_t lowest) noexcept {
            if (q.sign() == 0) {
                return;
            }
            if (auto v = q.to_int64()) {
                int64_t m = *v;
                while (scale > lowest && m % 10 == 0) {
                    m /= 10;
                    --scale;
                }
                q = m;
                return;
            }
            while (scale - 18 >= lowest) {
                auto [d, r] = q.div_rem(Pow10[18]);
                if (r.sign() != 0) {
                    break;
                }
                q = d;
                scale -= 18;
            }
            while (scale > lowest) {
                auto [d, r] = q.div_rem(10);
                if (r.sign() != 0) {
                    break;
                }
                q = d;
                --scale;
            }
        }

        // The digits of |u| appended to out
        inline void append_digits(std::string& out, const big_integer& u) noexcept {
            BigIntAccess::append_magnitude(out, u, 10);
        }

        struct DecimalAccess;
    }

    class decimal {
        enum Kind : uint8_t {
            Finite,
            NaN,
            PositiveInfinity,
            NegativeInfinity
        };

    public:
        // Zero, at scale 0
        SGCL_INLINE_HOT decimal() noexcept
        : _scale(0)
        , _kind(Finite) {
        }

        // A whole number, at scale 0, implicitly — so that d * 3, d + 1
        // and d == 0 read as written; from any whole number of the
        // language but bool, allocating nothing for a value int64_t holds
        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        SGCL_INLINE_HOT decimal(T value) noexcept
        : _unscaled(value)
        , _scale(0)
        , _kind(Finite) {
        }

        SGCL_INLINE_HOT decimal(big_integer value) noexcept
        : _unscaled(std::move(value))
        , _scale(0)
        , _kind(Finite) {
        }

        // unscaled × 10^-scale: decimal(150, 2) is 1.50, decimal(15, -2)
        // is 1500 written as 1.5e+3. The whole number of the language has
        // a form of its own, so that it goes here and not to the rational
        // below, which takes one too
        SGCL_INLINE_HOT decimal(big_integer unscaled, int32_t scale) noexcept
        : _unscaled(std::move(unscaled))
        , _scale(scale)
        , _kind(Finite) {
        }

        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        SGCL_INLINE_HOT decimal(T unscaled, int32_t scale) noexcept
        : _unscaled(unscaled)
        , _scale(scale)
        , _kind(Finite) {
        }

        // The double exactly, as every finite double is a decimal of at
        // most 767 significant digits: decimal(0.1) is
        // 0.1000000000000000055511151231257827021181583404541015625, at the
        // smallest scale that holds it (0 for a whole number). NaN and the
        // infinities are this type's own.
        explicit decimal(double value) noexcept;

        // Not from a bool, which would arrive as a whole number, nor from a
        // long double, which would arrive rounded to a double; the bool one
        // a template taking a bool alone, as big_integer's
        template<std::same_as<bool> B>
        decimal(B) = delete;
        explicit decimal(long double) = delete;

        // The fraction rounded to `scale` places: decimal(rational(2, 3), 4)
        // is 0.6667
        decimal(const rational& value, int32_t scale, rounding mode = rounding::half_even);

        // The shortest decimal that reads back as the double (0.1 is 0.1,
        // what std::to_chars writes), at the smallest scale not below zero
        // that holds it; NaN and the infinities as themselves, -0.0 as 0
        static decimal shortest(double value) noexcept;

        SGCL_INLINE_HOT static decimal nan() noexcept {
            return decimal(NaN);
        }

        // +infinity; -decimal::infinity() is the other
        SGCL_INLINE_HOT static decimal infinity() noexcept {
            return decimal(PositiveInfinity);
        }

        // A decimal "-12.50", "1.5e3", ".5", "7.", "-0.0012E-4": an optional
        // sign, digits with an optional point (digits on one side of it at
        // least), an optional exponent; or "NaN", "Infinity" or "Inf" in any
        // case, the infinities with an optional sign. The scale is the
        // text's: "1.50" is at scale 2, "1.5e3" at scale -2. No space, no
        // separator. A value whose exponent in scientific form (the place of
        // its first digit, what to_scientific writes) is past a million
        // either way is an error of the data, as a few bytes would otherwise
        // ask for megabytes when the value meets another.
        static expected<decimal, parse_error> parse(const string& text) noexcept;

        // The number a literal in the program writes: parse's value, or
        // bad_expected_access<parse_error> with parse's message. Input is
        // parsed; a text the program itself wrote is constructed (DESIGN 234)
        SGCL_INLINE_HOT explicit decimal(const string& text)
        : decimal(parse(text).value()) {
        }

        // Exact: the scale of a sum is the larger of the two, of a product
        // their sum (a product's past int32_t is length_error)
        SGCL_INLINE_HOT friend decimal operator+(const decimal& a, const decimal& b) noexcept {
            if (!(a._kind | b._kind) && a._scale == b._scale) {
                return decimal(a._unscaled + b._unscaled, a._scale);
            }
            return _add(a, b, false);
        }

        SGCL_INLINE_HOT friend decimal operator-(const decimal& a, const decimal& b) noexcept {
            if (!(a._kind | b._kind) && a._scale == b._scale) {
                return decimal(a._unscaled - b._unscaled, a._scale);
            }
            return _add(a, b, true);
        }

        SGCL_INLINE_HOT friend decimal operator*(const decimal& a, const decimal& b) {
            if (!(a._kind | b._kind)) {
                return decimal(a._unscaled * b._unscaled, detail::checked_scale(int64_t(a._scale) + b._scale));
            }
            return _special_mul(a, b);
        }

        // The remainder of the quotient cut towards zero, with the
        // dividend's sign (C++'s % and Python's for Decimal), at the larger
        // scale; by zero is domain_error
        friend decimal operator%(const decimal& a, const decimal& b) {
            return a.div_rem(b).second;
        }

        SGCL_INLINE_HOT decimal operator-() const noexcept {
            switch (_kind) {
                case PositiveInfinity:
                    return decimal(NegativeInfinity);
                case NegativeInfinity:
                    return decimal(PositiveInfinity);
                case NaN:
                    return *this;
                default:
                    return decimal(-_unscaled, _scale);
            }
        }

        SGCL_INLINE_HOT decimal& operator+=(const decimal& b) noexcept {
            return *this = *this + b;
        }

        SGCL_INLINE_HOT decimal& operator-=(const decimal& b) noexcept {
            return *this = *this - b;
        }

        SGCL_INLINE_HOT decimal& operator*=(const decimal& b) {
            return *this = *this * b;
        }

        SGCL_INLINE_HOT decimal& operator%=(const decimal& b) {
            return *this = *this % b;
        }

        // The quotient rounded to `scale` places: decimal(1).div(3, 4) is
        // 0.3333, decimal(2).div(3, 0, rounding::down) is 0. By zero is
        // domain_error; NaN divided by anything is NaN.
        decimal div(const decimal& by, int32_t scale, rounding mode = rounding::half_even) const;

        // The quotient rounded to `digits` significant digits, as Python's
        // decimal divides in a context of that precision: an exact quotient
        // drops the zeros at its end down to the scale of this less the
        // divisor's (1/4 is 0.25, 1/0.01 is 1e+2), an inexact one has all
        // the digits. digits below 1 is domain_error.
        decimal div_precision(const decimal& by, int32_t digits, rounding mode = rounding::half_even) const;

        // The quotient cut towards zero, at scale 0, and the remainder (as
        // %): {7.5 / 2 → 3, 1.5}
        pair<decimal, decimal> div_rem(const decimal& by) const;

        // The value at `scale` places, rounded when that drops digits:
        // 2.675 to 2 places is 2.68 (half-even), 2.5 to 0 is 2, 1.5 to 3 is
        // 1.500. Java's setScale, Python's quantize.
        decimal rescale(int32_t scale, rounding mode = rounding::half_even) const;

        // The value rounded to `digits` significant digits, unchanged when
        // it has no more: 123.456 to 4 is 123.5, 999.96 to 4 is 1000;
        // digits below 1 is domain_error
        decimal round_precision(int32_t digits, rounding mode = rounding::half_even) const;

        // The value at the smallest scale not below zero that holds it
        // (PostgreSQL's trim_scale): 1.500 is 1.5, 2.00 is 2, 1.5e+3 is 1500
        decimal trim_scale() const noexcept;

        // The square root rounded to `digits` significant digits; an exact
        // root keeps the zeros at its end down to half the scale (as
        // Python's sqrt: the root of 4 is 2, of 0.0625 is 0.25). A negative
        // value and digits below 1 are domain_error.
        decimal sqrt(int32_t digits, rounding mode = rounding::half_even) const;

        SGCL_INLINE_HOT decimal abs() const noexcept {
            return sign() < 0 ? -*this : *this;
        }

        // -1, 0 or 1; the infinities by their sign, NaN 0
        SGCL_INLINE_HOT int sign() const noexcept {
            switch (_kind) {
                case Finite:
                    return _unscaled.sign();
                case PositiveInfinity:
                    return 1;
                case NegativeInfinity:
                    return -1;
                default:
                    return 0;
            }
        }

        // The unscaled part, the value being unscaled × 10^-scale; zero
        // for NaN and the infinities, at scale 0
        SGCL_INLINE_HOT const big_integer& unscaled() const noexcept {
            return _unscaled;
        }

        SGCL_INLINE_HOT int32_t scale() const noexcept {
            return _scale;
        }

        // The digits of the unscaled part: 3 for 1.50, 1 for 0
        SGCL_INLINE_HOT size_t precision() const noexcept {
            return detail::decimal_digits(_unscaled);
        }

        SGCL_INLINE_HOT bool is_nan() const noexcept {
            return _kind == NaN;
        }

        SGCL_INLINE_HOT bool is_infinite() const noexcept {
            return _kind == PositiveInfinity || _kind == NegativeInfinity;
        }

        SGCL_INLINE_HOT bool is_finite() const noexcept {
            return _kind == Finite;
        }

        // The values, whatever the scales: 1.0 == 1.00. NaN equals NaN
        SGCL_INLINE_HOT friend bool operator==(const decimal& a, const decimal& b) noexcept {
            if (!(a._kind | b._kind) && a._scale == b._scale) {
                return a._unscaled == b._unscaled;
            }
            return _compare(a, b) == 0;
        }

        // The order of the values: -inf < every finite value < +inf < NaN;
        // weak, since 1.0 and 1.00 are equivalent but not the same
        SGCL_INLINE_HOT friend std::weak_ordering operator<=>(const decimal& a, const decimal& b) noexcept {
            int c = !(a._kind | b._kind) && a._scale == b._scale ? _order(a._unscaled <=> b._unscaled) : _compare(a, b);
            return c < 0 ? std::weak_ordering::less : c > 0 ? std::weak_ordering::greater : std::weak_ordering::equivalent;
        }

        // The same value at the same scale: 1.0 and 1.00 are equal and not
        // identical
        SGCL_INLINE_HOT bool identical(const decimal& other) const noexcept {
            return _kind == other._kind && _scale == other._scale && _unscaled == other._unscaled;
        }

        // Plain, never an exponent: "1500", "-0.0012", "1.50"; "NaN",
        // "Infinity", "-Infinity". More characters than a string holds is
        // length_error.
        string to_string() const;

        // Every digit of the unscaled part in scientific form: "1.5e+3",
        // "-1.2e-3", "1.50e+0", "5e+0" (Python's format(d, 'e')); more
        // characters than a string holds is length_error
        string to_scientific() const;

        // The nearest double, a tie to the even one, rounded once from the
        // exact value; an infinity past the largest double, a zero below the
        // smallest; NaN and the infinities as themselves
        double to_double() const noexcept;

        // The value when it is whole and int64_t holds it: 2.00 is 2, 2.50
        // nothing
        optional<int64_t> to_int64() const noexcept;

        // The value rounded to a whole number, towards zero by default (a
        // cast's way); NaN and the infinities are domain_error
        big_integer to_big_integer(rounding mode = rounding::down) const;

        // The value as a fraction, exactly: 1.50 is 3/2; NaN and the
        // infinities are domain_error
        rational to_rational() const;

    private:
        struct Raw {};

        SGCL_INLINE_HOT explicit decimal(Kind kind) noexcept
        : _scale(0)
        , _kind(kind) {
        }

        SGCL_INLINE_HOT static int _order(std::strong_ordering c) noexcept {
            return c < 0 ? -1 : c > 0 ? 1 : 0;
        }

        // The rank of a value in the total order: -inf, finite, +inf, NaN
        SGCL_INLINE_HOT int _rank() const noexcept {
            switch (_kind) {
                case NegativeInfinity:
                    return 0;
                case Finite:
                    return 1;
                case PositiveInfinity:
                    return 2;
                default:
                    return 3;
            }
        }

        static decimal _add(const decimal& a, const decimal& b, bool subtract) noexcept;
        static decimal _div_precision_wide(detail::Wide a, detail::Wide b, bool negative, int64_t sa, int64_t sb,
                                           int32_t digits, rounding mode);
        static decimal _special_mul(const decimal& a, const decimal& b) noexcept;
        static int _compare(const decimal& a, const decimal& b) noexcept;
        size_t _hash() const noexcept;

        big_integer _unscaled;
        int32_t _scale;
        Kind _kind;

        friend struct std::hash<decimal>;
        friend struct detail::DecimalAccess;
    };

    inline decimal decimal::_add(const decimal& a, const decimal& b, bool subtract) noexcept {
        if (a._kind | b._kind) {
            if (a._kind == NaN || b._kind == NaN) {
                return nan();
            }
            Kind bk = b._kind;
            if (subtract && bk != Finite) {
                bk = bk == PositiveInfinity ? NegativeInfinity : PositiveInfinity;
            }
            if (a._kind != Finite && bk != Finite) {
                return a._kind == bk ? decimal(bk) : nan();
            }
            return decimal(a._kind != Finite ? a._kind : bk);
        }
        // The one at the smaller scale brought to the larger
        const decimal* x = &a;
        const decimal* y = &b;
        bool negate_x = false;
        bool negate_y = subtract;
        if (a._scale > b._scale) {
            std::swap(x, y);
            std::swap(negate_x, negate_y);
        }
        int64_t k = int64_t(y->_scale) - x->_scale;
        if (k <= 18) {
            auto xs = x->_unscaled.to_int64();
            auto ys = y->_unscaled.to_int64();
            if (xs && ys) {
                // |x| < 2^63, 10^18 < 2^60: the sum stays within 2^124
                __int128 xv = __int128(*xs) * detail::Pow10[k];
                __int128 yv = *ys;
                return decimal(big_integer(negate_x ? yv - xv : negate_y ? xv - yv : xv + yv), y->_scale);
            }
        }
        big_integer xv = detail::times_pow10(x->_unscaled, k);
        if (negate_x) {
            return decimal(y->_unscaled - xv, y->_scale);
        }
        if (negate_y) {
            return decimal(xv - y->_unscaled, y->_scale);
        }
        return decimal(xv + y->_unscaled, y->_scale);
    }

    inline decimal decimal::_special_mul(const decimal& a, const decimal& b) noexcept {
        if (a._kind == NaN || b._kind == NaN) {
            return nan();
        }
        // An infinity by zero has no value; by anything else the signs decide
        int sa = a.sign();
        int sb = b.sign();
        if (!sa || !sb) {
            return nan();
        }
        return decimal(sa == sb ? PositiveInfinity : NegativeInfinity);
    }

    // Finite values of different scales: by sign, then by the estimates
    // of their magnitudes (log10 within a third of a digit), and only when
    // those are close by the values brought to one scale — a difference of
    // scales then bounded by the lengths of the two, so a value at scale
    // 2·10^9 is not multiplied out to be told from 1
    inline int decimal::_compare(const decimal& a, const decimal& b) noexcept {
        int ra = a._rank();
        int rb = b._rank();
        if (ra != rb) {
            return ra < rb ? -1 : 1;
        }
        if (ra != 1) {
            return 0;
        }
        int sa = a._unscaled.sign();
        int sb = b._unscaled.sign();
        if (sa != sb) {
            return sa < sb ? -1 : 1;
        }
        if (!sa) {
            return 0;
        }
        if (a._scale == b._scale) {
            return _order(a._unscaled <=> b._unscaled);
        }
        int64_t k = int64_t(a._scale) - b._scale;   // a's scale above b's: b brought up by k
        if (k >= -18 && k <= 18) {
            auto as = a._unscaled.to_int64();
            auto bs = b._unscaled.to_int64();
            if (as && bs) {
                __int128 x = k < 0 ? __int128(*as) * detail::Pow10[-k] : __int128(*as);
                __int128 y = k > 0 ? __int128(*bs) * detail::Pow10[k] : __int128(*bs);
                return x < y ? -1 : x > y ? 1 : 0;
            }
        }
        double ea = double(a._unscaled.bit_length()) * 0.30102999566398120 - double(a._scale);
        double eb = double(b._unscaled.bit_length()) * 0.30102999566398120 - double(b._scale);
        if (ea - eb > 2) {
            return sa;
        }
        if (eb - ea > 2) {
            return -sa;
        }
        if (k > 0) {
            return _order(a._unscaled <=> detail::times_pow10(b._unscaled, k));
        }
        return _order(detail::times_pow10(a._unscaled, -k) <=> b._unscaled);
    }

    // The value modulo the prime 2^61 - 1, the scale applied as a power of
    // the inverse of ten: one number for every scale of a value, so 1.0
    // and 1.00 hash alike without either being reduced
    inline size_t decimal::_hash() const noexcept {
        if (_kind != Finite) {
            return size_t(0x9e3779b97f4a7c15ull * (uint64_t(_kind) + 1));
        }
        constexpr uint64_t P = (uint64_t(1) << 61) - 1;
        auto mul = [](uint64_t x, uint64_t y) noexcept {
            unsigned __int128 p = (unsigned __int128)x * y;
            uint64_t r = uint64_t(p & P) + uint64_t(p >> 61);
            return r >= P ? r - P : r;
        };
        size_t n;
        detail::Limb room;
        const detail::Limb* p = detail::BigIntAccess::magnitude(_unscaled, room, n);
        uint64_t h = n == 1 && p[0] < P ? p[0] : detail::mod_1(p, n, detail::Divisor(P));
        if (h && _scale) {
            // 10^-scale: the inverse of 10 is 10^(P - 2)
            constexpr uint64_t Inverse10 = 2075258708292324556ull;   // 10 · this ≡ 1 (mod P)
            uint64_t base = _scale > 0 ? Inverse10 : 10;
            uint64_t e = _scale > 0 ? uint64_t(_scale) : uint64_t(-int64_t(_scale));
            uint64_t f = 1;
            while (e) {
                if (e & 1) {
                    f = mul(f, base);
                }
                base = mul(base, base);
                e >>= 1;
            }
            h = mul(h, f);
        }
        if (_unscaled.sign() < 0 && h) {
            h = P - h;
        }
        h ^= h >> 32;
        h *= 0xd6e8feb86659fd93ull;
        h ^= h >> 32;
        return size_t(h);
    }

    inline decimal::decimal(double value) noexcept
    : _scale(0)
    , _kind(Finite) {
        if (std::isnan(value)) {
            _kind = NaN;
            return;
        }
        if (std::isinf(value)) {
            _kind = value > 0 ? PositiveInfinity : NegativeInfinity;
            return;
        }
        if (value == 0) {
            return;
        }
        // value = m · 2^e with m odd: m · 5^-e at scale -e for a negative
        // e, as 2^-k = 5^k / 10^k
        int e;
        double f = std::frexp(std::fabs(value), &e);
        auto m = uint64_t(std::ldexp(f, 53));
        e -= 53;
        int twos = std::countr_zero(m);
        m >>= twos;
        e += twos;
        big_integer u = value < 0 ? -big_integer(m) : big_integer(m);
        if (e >= 0) {
            _unscaled = u << e;
        } else {
            _unscaled = u * big_integer(5).pow(-e);
            _scale = -e;
        }
    }

    inline decimal::decimal(const rational& value, int32_t scale, rounding mode)
    : _scale(scale)
    , _kind(Finite) {
        if (scale >= 0) {
            _unscaled = detail::round_div(detail::times_pow10(value.numerator(), scale), value.denominator(), mode);
        } else {
            _unscaled = detail::round_div_pow10(value.numerator(), value.denominator(), -int64_t(scale), mode);
        }
    }

    inline decimal decimal::shortest(double value) noexcept {
        if (!std::isfinite(value)) {
            return decimal(value);
        }
        if (value == 0) {
            return decimal();
        }
        // The shortest digits that read back, d.ddde±x
        char buf[64];
        auto res = std::to_chars(buf, buf + sizeof buf, value, std::chars_format::scientific);
        const char* p = buf;
        bool negative = *p == '-';
        p += negative;
        uint64_t m = 0;
        int fraction = 0;
        bool after_point = false;
        for (; *p != 'e'; ++p) {
            if (*p == '.') {
                after_point = true;
                continue;
            }
            m = m * 10 + uint64_t(*p - '0');
            fraction += after_point;
        }
        int exponent = 0;
        std::from_chars(p + 1 + (p[1] == '+'), res.ptr, exponent);
        int64_t scale = int64_t(fraction) - exponent;
        big_integer u = negative ? -big_integer(m) : big_integer(m);
        if (scale < 0) {
            return decimal(detail::times_pow10(u, -scale), 0);
        }
        // to_chars gives no zero at the end of the digits, so this is the
        // smallest scale already
        return decimal(std::move(u), int32_t(scale));
    }

    inline expected<decimal, parse_error> decimal::parse(const string& text) noexcept {
        using Reason = parse_error::Reason;
        auto fail = [](Reason reason, size_t at) {
            return unexpected<parse_error>(parse_error(reason, at, 10));
        };
        const char* p = text.data();
        size_t size = text.size();
        if (!size) {
            return fail(Reason::empty, 0);
        }
        size_t at = 0;
        bool negative = false;
        if (p[0] == '+' || p[0] == '-') {
            negative = p[0] == '-';
            at = 1;
        }
        // NaN, Inf, Infinity in any case; NaN without a sign
        auto word = [&](const char* w) {
            size_t n = std::char_traits<char>::length(w);
            if (size - at != n) {
                return false;
            }
            for (size_t i = 0; i < n; ++i) {
                if ((p[at + i] | 0x20) != w[i]) {
                    return false;
                }
            }
            return true;
        };
        if (at < size && (p[at] | 0x20) >= 'a' && (p[at] | 0x20) <= 'z') {
            if (at == 0 && word("nan")) {
                return nan();
            }
            if (word("inf") || word("infinity")) {
                return decimal(negative ? NegativeInfinity : PositiveInfinity);
            }
            return fail(Reason::invalid_digit, at);
        }
        auto is_digit = [](char c) {
            return c >= '0' && c <= '9';
        };
        size_t whole_first = at;
        while (at < size && is_digit(p[at])) {
            ++at;
        }
        size_t whole_last = at;
        size_t fraction_first = at;
        size_t fraction_last = at;
        if (at < size && p[at] == '.') {
            fraction_first = ++at;
            while (at < size && is_digit(p[at])) {
                ++at;
            }
            fraction_last = at;
        }
        if (whole_first == whole_last && fraction_first == fraction_last) {
            return fail(at < size ? Reason::invalid_digit : Reason::no_digits, at);
        }
        int64_t exponent = 0;
        size_t exponent_offset = whole_first;
        if (at < size && (p[at] == 'e' || p[at] == 'E')) {
            size_t exponent_at = ++at;
            bool exponent_negative = false;
            if (at < size && (p[at] == '+' || p[at] == '-')) {
                exponent_negative = p[at] == '-';
                ++at;
            }
            size_t digits_first = at;
            while (at < size && is_digit(p[at])) {
                // held far past any length of text, so that it cannot overflow
                exponent = std::min<int64_t>(exponent * 10 + (p[at] - '0'), int64_t(1) << 50);
                ++at;
            }
            if (at < size) {
                return fail(Reason::invalid_digit, at);
            }
            if (digits_first == at) {
                return fail(Reason::no_digits, at);
            }
            if (exponent_negative) {
                exponent = -exponent;
            }
            exponent_offset = exponent_at;
        }
        if (at < size) {
            return fail(Reason::invalid_digit, at);
        }
        // The exponent of the value in scientific form, the place of its
        // first digit, is held to a million either way: what to_scientific
        // writes reads back, and no text asks for more digits than its own
        // length and a million when the value meets another
        int64_t scale = int64_t(fraction_last - fraction_first) - exponent;
        size_t first = whole_first;
        while (first < fraction_last && (p[first] == '0' || p[first] == '.')) {
            ++first;
        }
        int64_t significant = 1;
        if (first < fraction_last) {
            significant = int64_t(fraction_last - first) - (first < whole_last && fraction_first > whole_last);
        }
        int64_t place = significant - 1 - scale;
        if (place > 1'000'000 || place < -1'000'000 || scale > INT32_MAX || scale < INT32_MIN) {
            return fail(Reason::exponent_out_of_range, exponent_offset);
        }
        // The digits of both sides as one whole number: on the processor's
        // own while they fit, else through big_integer's reading
        size_t whole = whole_last - whole_first;
        size_t fraction = fraction_last - fraction_first;
        if (whole + fraction <= 18) {
            int64_t m = 0;
            for (size_t i = whole_first; i < whole_last; ++i) {
                m = m * 10 + (p[i] - '0');
            }
            for (size_t i = fraction_first; i < fraction_last; ++i) {
                m = m * 10 + (p[i] - '0');
            }
            return decimal(negative ? -m : m, int32_t(scale));
        }
        if (whole + fraction <= 38) {
            detail::Wide m = 0;
            for (size_t i = whole_first; i < whole_last; ++i) {
                m = m * 10 + unsigned(p[i] - '0');
            }
            for (size_t i = fraction_first; i < fraction_last; ++i) {
                m = m * 10 + unsigned(p[i] - '0');
            }
            auto v = __int128(m);
            return decimal(big_integer(negative ? -v : v), int32_t(scale));
        }
        std::string digits(p + whole_first, whole);
        digits.append(p + fraction_first, fraction);
        auto value = big_integer::parse(string(digits));
        return decimal(negative ? -*value : *value, int32_t(scale));
    }

    inline decimal decimal::div(const decimal& by, int32_t scale, rounding mode) const {
        if (_kind == NaN || by._kind == NaN) {
            return nan();
        }
        if (by._kind == Finite && by._unscaled.sign() == 0) {
            throw domain_error("sgcl::math::decimal::div: division by zero");
        }
        if (_kind != Finite) {
            if (by._kind != Finite) {
                return nan();
            }
            return decimal((sign() < 0) != (by.sign() < 0) ? NegativeInfinity : PositiveInfinity);
        }
        if (by._kind != Finite) {
            return decimal(big_integer(), scale);
        }
        // (a·10^-sa) / (b·10^-sb) at scale s: a·10^(s - sa + sb) / b
        int64_t e = int64_t(scale) - _scale + by._scale;
        if (e >= 0) {
            return decimal(detail::round_div(detail::times_pow10(_unscaled, e), by._unscaled, mode), scale);
        }
        return decimal(detail::round_div_pow10(_unscaled, by._unscaled, -e, mode), scale);
    }

    inline decimal decimal::div_precision(const decimal& by, int32_t digits, rounding mode) const {
        if (digits < 1) {
            throw domain_error("sgcl::math::decimal::div_precision: fewer than one digit");
        }
        if (_kind == NaN || by._kind == NaN) {
            return nan();
        }
        if (by._kind == Finite && by._unscaled.sign() == 0) {
            throw domain_error("sgcl::math::decimal::div_precision: division by zero");
        }
        if (_kind != Finite) {
            if (by._kind != Finite) {
                return nan();
            }
            return decimal((sign() < 0) != (by.sign() < 0) ? NegativeInfinity : PositiveInfinity);
        }
        if (by._kind != Finite) {
            return decimal();
        }
        int64_t ideal = int64_t(_scale) - by._scale;
        if (_unscaled.sign() == 0) {
            return decimal(big_integer(), detail::checked_scale(ideal));
        }
        if (detail::Wide a, b; digits <= 37 && detail::magnitude_wide(_unscaled, a) && detail::magnitude_wide(by._unscaled, b)) {
            return _div_precision_wide(a, b, (sign() < 0) != (by.sign() < 0), int64_t(_scale), int64_t(by._scale),
                                       digits, mode);
        }
        // The quotient's first digit is at 10^adj, adj the difference of
        // the operands' or one less; the quotient is computed cut to one
        // digit more than asked when it is the difference, so exactly
        // `digits` or digits + 1, and rounded once from that and the
        // remainder
        auto adjusted = [](const decimal& x) {
            return int64_t(x.precision()) - 1 - x._scale;
        };
        int64_t adj = adjusted(*this) - adjusted(by);
        int64_t t = int64_t(digits) - adj;   // the scale of digits + 1 when the quotient's top is at adj
        int64_t e = t - _scale + by._scale;
        big_integer n = e >= 0 ? detail::times_pow10(_unscaled, e) : _unscaled;
        big_integer d = e >= 0 ? by._unscaled : detail::times_pow10(by._unscaled, -e);
        auto [q, r] = n.div_rem(d);
        bool negative = q.sign() < 0 || (q.sign() == 0 && (n.sign() < 0) != (d.sign() < 0));
        int cmp;
        bool inexact;
        if (detail::decimal_digits(q) > size_t(digits)) {
            // one digit over: it goes, and the remainder makes it sticky
            auto [q10, last] = q.div_rem(10);
            int64_t l = *last.abs().to_int64();
            cmp = l < 5 ? -1 : l > 5 ? 1 : r.sign() != 0 ? 1 : 0;
            inexact = l != 0 || r.sign() != 0;
            q = q10;
            --t;
        } else {
            auto c = (r.abs() << 1) <=> d.abs();
            cmp = c < 0 ? -1 : c == 0 ? 0 : 1;
            inexact = r.sign() != 0;
        }
        if (detail::round_away(mode, negative, q.bit(0), cmp, inexact)) {
            q += negative ? -1 : 1;
            if (detail::decimal_digits(q) > size_t(digits)) {
                q = q / 10;   // 10^digits: a zero at the end, exactly
                --t;
            }
        }
        if (!inexact) {
            detail::strip_zeros(q, t, ideal);
        }
        return decimal(std::move(q), detail::checked_scale(t));
    }

    // div_precision of two unscaled parts within 128 bits and a quotient of
    // at most 37 digits (so 38 with the one to round from, within 128 bits
    // too): the dividend or the divisor brought up by the power of ten on
    // limbs of the stack, one division, and the rounding and the zeros on
    // the processor's own numbers — nothing allocated but the result, when it
    // leaves int64_t
    inline decimal decimal::_div_precision_wide(detail::Wide a, detail::Wide b, bool negative, int64_t sa, int64_t sb,
                                                int32_t digits, rounding mode) {
        using detail::Limb;
        using detail::Wide;
        int da = detail::decimal_digits_wide(a);
        int db = detail::decimal_digits_wide(b);
        // the scale of digits + 1 when the quotient's top digit is at the
        // difference of the operands' and the power that brings to it
        int64_t t = int64_t(digits) - ((da - 1 - sa) - (db - 1 - sb));
        int64_t e = int64_t(digits) - da + db;
        Limb n[8] = {Limb(a), Limb(a >> 64)};
        Limb d[8] = {Limb(b), Limb(b >> 64)};
        size_t nn = 2;
        size_t dn = 2;
        auto scale_up = [](Limb* x, size_t& xn, int64_t k) {
            for (; k > 0; k -= 19) {
                x[xn] = detail::mul_1(x, x, xn, Limb(detail::Pow10Wide[size_t(std::min<int64_t>(k, 19))]));
                ++xn;
            }
            xn = detail::normalized(x, xn);
        };
        if (e >= 0) {
            scale_up(n, nn, e);
            dn = detail::normalized(d, dn);
        } else {
            scale_up(d, dn, -e);
            nn = detail::normalized(n, nn);
        }
        Limb q[8] = {};
        Limb r[8] = {};
        if (detail::compare(n, nn, d, dn) < 0) {
            sgcl::detail::copy_bytes(r, n, nn * sizeof(Limb));
        } else if (dn == 1) {
            r[0] = detail::div_1(q, n, nn, detail::Divisor(d[0]));
        } else {
            detail::div_knuth(q, r, n, nn, d, dn);
        }
        Wide quotient = (Wide(q[1]) << 64) | q[0];
        bool rest = detail::normalized(r, dn) != 0;
        int cmp;
        bool inexact;
        if (detail::decimal_digits_wide(quotient) > digits) {
            // one digit over: it goes, and the remainder makes it sticky
            Limb qq[2] = {Limb(quotient), Limb(quotient >> 64)};
            Limb last = detail::div_1(qq, qq, 2, detail::Divisor(10));
            quotient = (Wide(qq[1]) << 64) | qq[0];
            cmp = last < 5 ? -1 : last > 5 ? 1 : rest ? 1 : 0;
            inexact = last != 0 || rest;
            --t;
        } else {
            // 2r against d
            Limb twice[9] = {};
            twice[dn] = detail::shift_left(twice, r, dn, 1);
            int c = detail::compare(twice, detail::normalized(twice, dn + 1), d, dn);
            cmp = c < 0 ? -1 : c == 0 ? 0 : 1;
            inexact = rest;
        }
        if (detail::round_away(mode, negative, quotient & 1, cmp, inexact)) {
            ++quotient;
            if (quotient == detail::Pow10Wide[size_t(digits)]) {
                quotient = detail::Pow10Wide[size_t(digits) - 1];   // a zero at the end, exactly
                --t;
            }
        }
        if (!inexact) {
            int64_t ideal = sa - sb;
            while (t > ideal && (quotient >> 64)) {
                Limb qq[2] = {Limb(quotient), Limb(quotient >> 64)};
                Limb last = detail::div_1(qq, qq, 2, detail::Divisor(10));
                if (last) {
                    break;
                }
                quotient = (Wide(qq[1]) << 64) | qq[0];
                --t;
            }
            if (!(quotient >> 64)) {
                auto m = uint64_t(quotient);
                while (t > ideal && m % 10 == 0 && m) {
                    m /= 10;
                    --t;
                }
                quotient = m;
            }
        }
        auto v = __int128(quotient);
        return decimal(big_integer(negative ? -v : v), detail::checked_scale(t));
    }

    inline pair<decimal, decimal> decimal::div_rem(const decimal& by) const {
        if (_kind == NaN || by._kind == NaN) {
            return {nan(), nan()};
        }
        if (by._kind == Finite && by._unscaled.sign() == 0) {
            throw domain_error("sgcl::math::decimal: division by zero");
        }
        if (_kind != Finite) {
            return {nan(), nan()};
        }
        if (by._kind != Finite) {
            return {decimal(), *this};
        }
        // Both at the larger scale: the whole quotient and the remainder
        // of the unscaled parts, the remainder at that scale
        int32_t s = std::max(_scale, by._scale);
        big_integer a = detail::times_pow10(_unscaled, int64_t(s) - _scale);
        big_integer b = detail::times_pow10(by._unscaled, int64_t(s) - by._scale);
        auto [q, r] = a.div_rem(b);
        return {decimal(std::move(q)), decimal(std::move(r), s)};
    }

    inline decimal decimal::rescale(int32_t scale, rounding mode) const {
        if (_kind != Finite || scale == _scale) {
            return *this;
        }
        if (scale > _scale) {
            return decimal(detail::times_pow10(_unscaled, int64_t(scale) - _scale), scale);
        }
        return decimal(detail::round_div_pow10(_unscaled, 1, int64_t(_scale) - scale, mode), scale);
    }

    inline decimal decimal::round_precision(int32_t digits, rounding mode) const {
        if (digits < 1) {
            throw domain_error("sgcl::math::decimal::round_precision: fewer than one digit");
        }
        if (_kind != Finite) {
            return *this;
        }
        auto p = int64_t(precision());
        if (p <= digits) {
            return *this;
        }
        int64_t drop = p - digits;
        int64_t t = int64_t(_scale) - drop;
        big_integer q = detail::round_div(_unscaled, detail::pow10(drop), mode);
        if (detail::decimal_digits(q) > size_t(digits)) {
            q = q / 10;
            --t;
        }
        return decimal(std::move(q), detail::checked_scale(t));
    }

    inline decimal decimal::trim_scale() const noexcept {
        if (_kind != Finite) {
            return *this;
        }
        if (_unscaled.sign() == 0) {
            return decimal();
        }
        if (_scale < 0) {
            return decimal(detail::times_pow10(_unscaled, -int64_t(_scale)), 0);
        }
        big_integer q = _unscaled;
        int64_t t = _scale;
        detail::strip_zeros(q, t, 0);
        return decimal(std::move(q), int32_t(t));
    }

    // The root at the scale t that gives `digits` digits: the whole part
    // of the square root of u·10^(2t - s), when 2t - s is not negative;
    // when it is, t is raised to make it so and the extra digits rounded
    // off after, with what the whole root left behind as a sticky bit. A
    // square root is never exactly at a half of its last digit.
    inline decimal decimal::sqrt(int32_t digits, rounding mode) const {
        if (digits < 1) {
            throw domain_error("sgcl::math::decimal::sqrt: fewer than one digit");
        }
        if (_kind == NaN || _kind == PositiveInfinity) {
            return *this;
        }
        if (sign() < 0) {
            throw domain_error("sgcl::math::decimal::sqrt: the square root of a negative number");
        }
        // The ideal scale, Python's ideal exponent floor(-s/2) negated
        int64_t ideal = _scale >= 0 ? (int64_t(_scale) + 1) / 2 : -((-int64_t(_scale)) / 2);
        if (_unscaled.sign() == 0) {
            return decimal(big_integer(), int32_t(ideal));
        }
        int64_t adj = int64_t(precision()) - 1 - _scale;
        int64_t root_adj = adj >= 0 ? adj / 2 : -((-adj + 1) / 2);   // floor(adj / 2)
        int64_t t = int64_t(digits) - 1 - root_adj;
        int64_t k = 2 * t - _scale;
        int64_t extra = 0;
        if (k < 0) {
            extra = (-k + 1) / 2;
            k += 2 * extra;
        }
        big_integer n = detail::times_pow10(_unscaled, k);
        big_integer root = n.sqrt();
        big_integer rest = n - root * root;
        bool inexact = rest.sign() != 0;
        big_integer q;
        if (extra) {
            q = detail::round_div(root, detail::pow10(extra), mode, inexact);
            if (!inexact) {
                auto [unused, r] = root.div_rem(detail::pow10(extra));
                inexact = r.sign() != 0;
            }
        } else {
            // the dropped fraction is above a half when rest > root, since
            // n < (root + 1/2)^2 = root^2 + root + 1/4 otherwise
            int cmp = rest > root ? 1 : -1;
            q = root;
            if (detail::round_away(mode, false, q.bit(0), cmp, inexact)) {
                q += 1;
            }
        }
        if (detail::decimal_digits(q) > size_t(digits)) {
            q = q / 10;
            --t;
        }
        if (!inexact) {
            detail::strip_zeros(q, t, ideal);
        }
        return decimal(std::move(q), detail::checked_scale(t));
    }

    namespace detail {
        // The digits of m written backwards from `end` (at most 39), the
        // first returned: 10^19 off the top by the invariant divisor, the
        // chunks by constant divisions
        inline char* wide_digits(char* end, Wide m) noexcept {
            constexpr Divisor Ten19(10000000000000000000ull);
            char* at = end;
            while (m >> 64) {
                Limb q[2] = {Limb(m), Limb(m >> 64)};
                Limb rem = div_1(q, q, 2, Ten19);
                m = (Wide(q[1]) << 64) | q[0];
                for (int i = 0; i < 19; ++i) {
                    *--at = char('0' + rem % 10);
                    rem /= 10;
                }
            }
            auto v = uint64_t(m);
            do {
                *--at = char('0' + v % 10);
                v /= 10;
            } while (v);
            return at;
        }

        // The digits of |u|: in `room` when 128 bits hold it, in `big`
        // otherwise
        SGCL_INLINE_HOT std::string_view decimal_text(const big_integer& u, char (&room)[40], std::string& big) noexcept {
            if (Wide m; magnitude_wide(u, m)) {
                char* first = wide_digits(room + 40, m);
                return {first, size_t(room + 40 - first)};
            }
            append_digits(big, u);
            return big;
        }

        // Text built on the stack while it is short
        struct ShortText {
            char room[96];
            size_t size = 0;

            SGCL_INLINE_HOT void append(const char* p, size_t n) noexcept {
                sgcl::detail::copy_bytes(room + size, p, n);
                size += n;
            }

            SGCL_INLINE_HOT void append(size_t n, char c) noexcept {
                sgcl::detail::fill_bytes(room + size, c, n);
                size += n;
            }
        };

        struct LongText {
            std::string text;

            SGCL_INLINE_HOT void append(const char* p, size_t n) {
                text.append(p, n);
            }

            SGCL_INLINE_HOT void append(size_t n, char c) {
                text.append(n, c);
            }
        };
    }

    inline string decimal::to_string() const {
        switch (_kind) {
            case NaN:
                return string("NaN");
            case PositiveInfinity:
                return string("Infinity");
            case NegativeInfinity:
                return string("-Infinity");
            default:
                break;
        }
        char room[40];
        std::string big;
        std::string_view digits = detail::decimal_text(_unscaled, room, big);
        size_t n = digits.size();
        bool negative = _unscaled.sign() < 0;
        bool zero = _unscaled.sign() == 0;
        uint64_t length = negative + (_scale <= 0             ? uint64_t(n) + (zero ? 0 : uint64_t(-int64_t(_scale)))
                                      : size_t(_scale) >= n ? uint64_t(_scale) + 2
                                                            : uint64_t(n) + 1);
        if (length > string::max_size()) {
            throw length_error("sgcl::math::decimal::to_string: more characters than a string holds");
        }
        auto write = [&](auto& out) {
            if (negative) {
                out.append(1, '-');
            }
            if (_scale <= 0) {
                out.append(digits.data(), n);
                if (!zero) {
                    out.append(size_t(-int64_t(_scale)), '0');
                }
            } else if (size_t(_scale) >= n) {
                out.append("0.", 2);
                out.append(size_t(_scale) - n, '0');
                out.append(digits.data(), n);
            } else {
                out.append(digits.data(), n - size_t(_scale));
                out.append(1, '.');
                out.append(digits.data() + n - size_t(_scale), size_t(_scale));
            }
        };
        if (length <= sizeof(detail::ShortText::room)) {
            detail::ShortText out;
            write(out);
            return string(out.room, out.size);
        }
        detail::LongText out;
        out.text.reserve(size_t(length));
        write(out);
        return string(out.text);
    }

    inline string decimal::to_scientific() const {
        if (_kind != Finite) {
            return to_string();
        }
        char room[40];
        std::string big;
        std::string_view digits = detail::decimal_text(_unscaled, room, big);
        if (digits.size() + 16 > string::max_size()) {
            throw length_error("sgcl::math::decimal::to_scientific: more characters than a string holds");
        }
        int64_t exponent = int64_t(digits.size()) - 1 - _scale;
        char tail[24];
        char* end = tail + sizeof tail;
        char* at = std::to_chars(tail + 2, end, exponent < 0 ? uint64_t(-exponent) : uint64_t(exponent)).ptr;
        tail[0] = 'e';
        tail[1] = exponent < 0 ? '-' : '+';
        auto write = [&](auto& out) {
            if (_unscaled.sign() < 0) {
                out.append(1, '-');
            }
            out.append(digits.data(), 1);
            if (digits.size() > 1) {
                out.append(1, '.');
                out.append(digits.data() + 1, digits.size() - 1);
            }
            out.append(tail, size_t(at - tail));
        };
        if (digits.size() + 26 <= sizeof(detail::ShortText::room)) {
            detail::ShortText out;
            write(out);
            return string(out.room, out.size);
        }
        detail::LongText out;
        write(out);
        return string(out.text);
    }

    inline double decimal::to_double() const noexcept {
        switch (_kind) {
            case NaN:
                return std::numeric_limits<double>::quiet_NaN();
            case PositiveInfinity:
                return HUGE_VAL;
            case NegativeInfinity:
                return -HUGE_VAL;
            default:
                break;
        }
        int sign = _unscaled.sign();
        if (!sign) {
            return 0.0;
        }
        // Both the digits and the power of ten exact in a double: one
        // operation, rounded once (Clinger's fast path)
        if (auto v = _unscaled.to_int64()) {
            uint64_t m = *v < 0 ? uint64_t(0) - uint64_t(*v) : uint64_t(*v);
            constexpr double Powers[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                         1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
            if (m < (uint64_t(1) << 53) && _scale >= -22 && _scale <= 22) {
                double d = _scale >= 0 ? double(m) / Powers[_scale] : double(m) * Powers[-_scale];
                return sign < 0 ? -d : d;
            }
        }
        auto p = int64_t(precision());
        int64_t adj = p - 1 - _scale;
        if (adj > 309) {
            return sign < 0 ? -HUGE_VAL : HUGE_VAL;
        }
        if (adj < -325) {
            return sign < 0 ? -0.0 : 0.0;
        }
        // A midpoint between two doubles has at most 767 significant digits:
        // 800 digits and a sticky one in place of the rest round alike
        big_integer u = _unscaled;
        int64_t s = _scale;
        if (p > 800) {
            int64_t k = p - 800;
            auto [q, r] = u.div_rem(detail::pow10(k));
            u = q;
            s -= k;
            if (r.sign() != 0) {
                u = u * 10 + sign;
                ++s;
            }
        }
        if (s <= 0) {
            return detail::times_pow10(u, -s).to_double();
        }
        return rational(u, detail::pow10(s)).to_double();
    }

    inline optional<int64_t> decimal::to_int64() const noexcept {
        if (_kind != Finite) {
            return nullopt;
        }
        if (_unscaled.sign() == 0) {
            return 0;
        }
        if (_scale <= 0) {
            if (_scale < -18) {
                return nullopt;
            }
            auto v = _unscaled.to_int64();
            if (!v) {
                return nullopt;
            }
            __int128 w = __int128(*v) * detail::Pow10[-_scale];
            if (w < INT64_MIN || w > INT64_MAX) {
                return nullopt;
            }
            return int64_t(w);
        }
        if (auto v = _unscaled.to_int64(); v && _scale <= 18) {
            if (*v % detail::Pow10[_scale]) {
                return nullopt;
            }
            return *v / detail::Pow10[_scale];
        }
        if (size_t(_scale) >= precision()) {
            return nullopt;   // a nonzero value below one
        }
        auto [q, r] = _unscaled.div_rem(detail::pow10(_scale));
        if (r.sign() != 0) {
            return nullopt;
        }
        return q.to_int64();
    }

    inline big_integer decimal::to_big_integer(rounding mode) const {
        if (_kind != Finite) {
            throw domain_error("sgcl::math::decimal::to_big_integer: a NaN or an infinity is no whole number");
        }
        return rescale(0, mode)._unscaled;
    }

    inline rational decimal::to_rational() const {
        if (_kind != Finite) {
            throw domain_error("sgcl::math::decimal::to_rational: a NaN or an infinity is no fraction");
        }
        if (_scale <= 0) {
            return rational(detail::times_pow10(_unscaled, -int64_t(_scale)));
        }
        return rational(_unscaled, detail::pow10(_scale));
    }

    // txt::format: {} is to_string; {:f} and {:.Nf} (and {:.N}) the value
    // at N places, rounded half-even ({:f} alone as it is); {:e} and {:.Ne}
    // scientific, with every digit or rounded half-even to N after the
    // point and padded with zeros to them; + and a space for the sign of a
    // value not negative, the width, the fill and the alignment (right by
    // default), the zeros of {:010.2f} after the sign
    inline void format_value(txt::format_sink& out, const decimal& v, const txt::format_spec& spec) {
        string text;
        decimal a = v.abs();
        if (!v.is_finite()) {
            text = a.to_string();
        } else if (spec.type == 'e') {
            if (spec.precision >= 0 && a.sign() == 0) {
                // a zero keeps its exponent, the places added to it (Python's
                // format: 0.00 to three places is 0.000e+1)
                auto places = size_t(spec.precision);
                int64_t exponent = int64_t(places) - a.scale();
                std::string s = places ? "0." + std::string(places, '0') : "0";
                s += exponent < 0 ? "e-" : "e+";
                s += std::to_string(exponent < 0 ? -exponent : exponent);
                text = string(s);
            } else if (spec.precision >= 0) {
                auto places = size_t(spec.precision);
                decimal r = a.round_precision(int32_t(std::min<size_t>(places + 1, INT32_MAX)));
                size_t have = r.precision();
                if (have < places + 1) {
                    // padded with zeros to the places asked for
                    r = decimal(detail::times_pow10(r.unscaled(), int64_t(places + 1 - have)),
                                detail::checked_scale(int64_t(r.scale()) + int64_t(places + 1 - have)));
                }
                text = r.to_scientific();
            } else {
                text = a.to_scientific();
            }
        } else if (spec.precision >= 0) {
            text = a.rescale(detail::checked_scale(spec.precision)).to_string();
        } else {
            text = a.to_string();
        }
        char head[1];
        size_t head_size = 0;
        if (v.sign() < 0) {
            head[head_size++] = '-';
        } else if (spec.sign == '+' || spec.sign == ' ') {
            head[head_size++] = spec.sign;
        }
        txt::detail::put_padded(out, {text.data(), text.size()}, spec, {head, head_size}, '>');
    }

    SGCL_INLINE_HOT std::ostream& operator<<(std::ostream& os, const decimal& v) {
        auto s = v.to_string();
        return os << std::string_view(s.data(), s.size());
    }
}

// Which specifications a decimal takes, for the pattern checked where it is
// compiled; the writing is format_value's, above
template<>
struct sgcl::txt::formatter<sgcl::math::decimal> {
    SGCL_INLINE_HOT static constexpr bool takes(char type) noexcept {
        return !type || type == 'f' || type == 'e';
    }

    SGCL_INLINE_HOT static constexpr bool takes_precision() noexcept {
        return true;
    }
};

template<>
struct std::hash<sgcl::math::decimal> {
    SGCL_INLINE_HOT size_t operator()(const sgcl::math::decimal& v) const noexcept {
        return v._hash();
    }
};

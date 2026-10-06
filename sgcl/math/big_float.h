//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/rounding.h"
#include "big_integer.h"
#include "decimal.h"
#include "rational.h"

#include <bit>
#include <cmath>
#include <compare>
#include <cstring>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>

// A binary floating-point number of any precision: what Go's math/big.Float
// is, as a value. A sign, a mantissa of at most `precision` bits, a binary
// exponent, and every result rounded to the precision in one of the modes
// of sgcl::rounding — Go's six (half_even is ToNearestEven, half_up
// ToNearestAway, down ToZero, up AwayFromZero, floor, ceiling) and
// half_down and unnecessary besides.
//
// Precision and mode belong to the value, as Go keeps them on the receiver:
// a result has the larger precision of its two operands and the mode of
// the left one, a precision of 0 (the default value, +0) taking the
// other's. A whole number comes in exactly at 64 bits (more for a longer
// big_integer), a double exactly at 53, as Go's SetInt64, SetInt and
// SetFloat64 do; a fraction, a decimal and text are rounded once to the
// precision asked for.
//
// There is ±0 and ±infinity and no NaN: an operation that would make one
// (∞ - ∞, 0·∞, 0/0, ∞/∞, the square root of a negative number) is an error
// of the program, domain_error, where Go panics with ErrNaN. The exponent
// has Go's range: with the value written 0.m × 2^exp, exp within int32_t;
// past it the result is an infinity or a zero of its sign.
//
// The mantissa is a big_integer without the zeros at its end, so a value
// whose mantissa fits in 63 bits allocates nothing; a big_float holds a
// tracked_ptr through it and lives where one may.
namespace sgcl::math {
    namespace detail {
        struct BigFloatAccess;
    }

    class big_float {
        enum Form : uint8_t {
            Zero,
            Finite,
            Infinite
        };

    public:
        // +0 at precision 0: an operand that takes the other's precision
        SGCL_INLINE_HOT big_float() noexcept
        : _exponent(0)
        , _precision(0)
        , _mode(rounding::half_even)
        , _form(Zero)
        , _negative(false) {
        }

        // A whole number exactly, at 64 bits (128 for the compiler's 128-bit
        // types), implicitly: x * 2, x + 1 and x < 0 read as written
        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        big_float(T value) noexcept
        : big_float(big_integer(value), sizeof(T) > 8 ? 128u : 64u, rounding::half_even) {
        }

        // A big_integer exactly, at 64 bits or its length if longer
        big_float(const big_integer& value) noexcept
        : big_float(value, uint32_t(std::max<size_t>(64, std::min<size_t>(value.bit_length(), MaxPrecision))),
                    rounding::half_even) {
        }

        // A double exactly, at 53 bits; ±0 and the infinities as themselves,
        // NaN domain_error
        explicit big_float(double value);

        template<std::same_as<bool> B>
        big_float(B) = delete;
        explicit big_float(long double) = delete;

        // Rounded to `precision` bits by `mode` (1 to 2^32 - 1 bits; 0 is
        // invalid_argument for a value that is not zero). A whole number of
        // the language has a form of its own, exact before the rounding: it
        // would otherwise reach the double's and be rounded to 53 bits first
        // (9007199254740993 at 200 bits would come out …992)
        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        big_float(T value, uint32_t precision, rounding mode = rounding::half_even)
        : big_float(big_integer(value), precision, mode) {
        }

        big_float(const big_integer& value, uint32_t precision, rounding mode = rounding::half_even);
        big_float(double value, uint32_t precision, rounding mode = rounding::half_even);
        big_float(const rational& value, uint32_t precision, rounding mode = rounding::half_even);
        big_float(const decimal& value, uint32_t precision, rounding mode = rounding::half_even);
        big_float(const big_float& value, uint32_t precision, rounding mode);

        // A move is the copy, as rational's: the mantissa's limbs are a shared
        // word, and a big_integer moved from is zero, which a finite value's
        // mantissa never is
        big_float(const big_float&) noexcept = default;
        big_float& operator=(const big_float&) noexcept = default;

        SGCL_INLINE_HOT big_float(big_float&& other) noexcept
        : big_float(static_cast<const big_float&>(other)) {
        }

        SGCL_INLINE_HOT big_float& operator=(big_float&& other) noexcept {
            return *this = static_cast<const big_float&>(other);
        }

        SGCL_INLINE_HOT static big_float infinity(bool negative = false) noexcept {
            big_float f;
            f._form = Infinite;
            f._negative = negative;
            return f;
        }

        // A number in decimal ("-1.5", "2.5e-300", ".5", "7.") or in
        // hexadecimal with a binary exponent ("0x1.8p3", "0x.cp-1"), or
        // "Inf"/"Infinity" with a sign, any case; rounded once to the
        // precision by the mode. A decimal exponent past a million either
        // way is an error of the data, as for rational and decimal.
        static expected<big_float, parse_error> parse(const string& text, uint32_t precision = 64,
                                                      rounding mode = rounding::half_even) noexcept;

        // The number a literal in the program writes, at 64 bits: parse's
        // value, or bad_expected_access<parse_error>
        SGCL_INLINE_HOT explicit big_float(const string& text)
        : big_float(parse(text).value()) {
        }

        // The arithmetic, each result rounded once to the larger precision
        // of the two by the left one's mode; a result that would be NaN is
        // domain_error; x / 0 is an infinity of the sign, 0 / 0 domain_error
        friend big_float operator+(const big_float& a, const big_float& b) {
            return _add(a, b, false);
        }

        friend big_float operator-(const big_float& a, const big_float& b) {
            return _add(a, b, true);
        }

        friend big_float operator*(const big_float& a, const big_float& b);
        friend big_float operator/(const big_float& a, const big_float& b);

        SGCL_INLINE_HOT big_float operator-() const noexcept {
            big_float f = *this;
            f._negative = !f._negative;
            return f;
        }

        SGCL_INLINE_HOT big_float& operator+=(const big_float& b) {
            return *this = *this + b;
        }

        SGCL_INLINE_HOT big_float& operator-=(const big_float& b) {
            return *this = *this - b;
        }

        SGCL_INLINE_HOT big_float& operator*=(const big_float& b) {
            return *this = *this * b;
        }

        SGCL_INLINE_HOT big_float& operator/=(const big_float& b) {
            return *this = *this / b;
        }

        // The square root at this precision, by this mode; -0 is -0, a
        // value below zero domain_error
        big_float sqrt() const;

        SGCL_INLINE_HOT big_float abs() const noexcept {
            big_float f = *this;
            f._negative = false;
            return f;
        }

        // -1, 0 or 1; -0 is 0
        SGCL_INLINE_HOT int sign() const noexcept {
            return _form == Zero ? 0 : _negative ? -1 : 1;
        }

        // Whether the sign is set: -0 and the negative numbers
        SGCL_INLINE_HOT bool signbit() const noexcept {
            return _negative;
        }

        SGCL_INLINE_HOT uint32_t precision() const noexcept {
            return _precision;
        }

        SGCL_INLINE_HOT rounding mode() const noexcept {
            return _mode;
        }

        SGCL_INLINE_HOT bool is_infinite() const noexcept {
            return _form == Infinite;
        }

        // Whether the value is a whole number (zero included, the infinities not)
        SGCL_INLINE_HOT bool is_integer() const noexcept {
            return _form == Zero || (_form == Finite && _exponent >= 0);
        }

        // The exponent of the value written 0.m × 2^exp with 0.5 <= m < 1,
        // Go's MantExp; 0 for zero and the infinities
        SGCL_INLINE_HOT int64_t exponent() const noexcept {
            return _form == Finite ? _exponent + int64_t(_mantissa.bit_length()) : 0;
        }

        // The values, whatever the precisions: +0 == -0; -∞ below every
        // number, +∞ above
        friend bool operator==(const big_float& a, const big_float& b) noexcept {
            return _compare(a, b) == 0;
        }

        friend std::weak_ordering operator<=>(const big_float& a, const big_float& b) noexcept {
            int c = _compare(a, b);
            return c < 0 ? std::weak_ordering::less : c > 0 ? std::weak_ordering::greater : std::weak_ordering::equivalent;
        }

        // The shortest decimal that reads back to this value at this
        // precision: "0.1", "1e+100", "-3.75", "+Inf" — plain, but for an
        // exponent below -4 or of 21 and more (Go's String chooses at 6, which
        // writes a million as 1e+06)
        string to_string() const;

        // The shortest decimal that reads back, in scientific form:
        // "1e-01", "1.5e+02", "-3.75e+00", at least two digits of exponent
        string to_scientific() const;

        // The exact value in hexadecimal, as C's %a writes a double: the first
        // bit before the point, the rest in hexadecimal digits, the binary
        // exponent — "0x1.8p+3", "-0x1p-1074", "0x0p+0"; parse reads it back
        string to_hex() const;

        // The nearest double, a tie to the even one (subnormals with the bits
        // they have, infinities past the largest)
        double to_double() const noexcept;

        // The value rounded to a whole number, towards zero by default (Go's
        // Int); the infinities are domain_error
        big_integer to_big_integer(rounding mode = rounding::down) const;

        // The value as a fraction, exactly; the infinities are domain_error
        rational to_rational() const;

        // The value as a decimal, exactly (every binary fraction ends in
        // decimal): a mantissa of p bits and an exponent of -e are about
        // 0.3p + 0.7e digits; the infinities are decimal's, -0 is 0
        decimal to_decimal() const;

    private:
        static constexpr uint32_t MaxPrecision = 0xffffffffu;

        static big_float _add(const big_float& a, const big_float& b, bool subtract);
        static int _compare(const big_float& a, const big_float& b) noexcept;
        static big_float _round(bool negative, big_integer m, int64_t e, bool sticky, uint32_t precision, rounding mode);
        static big_float _round_limbs(bool negative, const detail::Limb* p, size_t n, int64_t e, bool sticky,
                                      uint32_t precision, rounding mode);
        static uint32_t _precision_of(const big_float& a, const big_float& b) noexcept {
            return std::max(a._precision, b._precision);
        }
        static rounding _mode_of(const big_float& a, const big_float& b) noexcept {
            return a._precision ? a._mode : b._mode;
        }
        decimal _shortest() const;

        // Go's Text: 'e', 'f', 'g' with `digits` digits after the point ('g':
        // significant), or the fewest that read back for -1; 'p' (-0x.dddp±e,
        // the exact mantissa); 'x' (-0x1.dddp±ee, `digits` hexadecimal digits
        // or as many as the value has for -1). What txt::format writes, and the
        // tests' road to Go's own texts
        string _text(char format, int digits) const;

        big_integer _mantissa;   // the magnitude, odd (its zeros at the end dropped) for a finite value
        int64_t _exponent;       // the value is ±mantissa · 2^exponent
        uint32_t _precision;
        rounding _mode;
        Form _form;
        bool _negative;

        friend struct std::hash<big_float>;
        friend struct detail::BigFloatAccess;
        friend void format_value(txt::format_sink& out, const big_float& v, const txt::format_spec& spec);
    };

    namespace detail {
        // Go's Text of a value, for the tests that hold it against Go
        struct BigFloatAccess {
            static string text(const big_float& v, char format, int digits) {
                return v._text(format, digits);
            }
        };
    }

    namespace detail {
        // Go's MinExp and MaxExp: the range of exp in 0.m × 2^exp
        constexpr int64_t FloatMinExp = INT32_MIN;
        constexpr int64_t FloatMaxExp = INT32_MAX;
    }

    namespace detail {
        // a << bits into r (an + bits/64 + 1 limbs, all written)
        inline size_t float_shift_left(Limb* r, const Limb* a, size_t an, uint64_t bits) noexcept {
            size_t words = size_t(bits / 64);
            auto s = unsigned(bits % 64);
            if (words) {
                std::memset(r, 0, words * sizeof(Limb));
            }
            if (s) {
                r[words + an] = shift_left(r + words, a, an, s);
            } else {
                if (an) {
                    sgcl::detail::copy_bytes(r + words, a, an * sizeof(Limb));
                }
                r[words + an] = 0;
            }
            return words + an + 1;
        }

        SGCL_INLINE_HOT bool float_bit(const Limb* p, size_t n, size_t i) noexcept {
            return i / 64 < n && ((p[i / 64] >> (i % 64)) & 1);
        }

        // Working limbs of an operation: on the stack up to 320 (20480 bits),
        // where the transforms of a few thousand bits want no call to malloc
        class FloatScratch {
        public:
            explicit FloatScratch(size_t n)
            : _p(n <= Inline ? _inline : (_heap = std::make_unique_for_overwrite<Limb[]>(n)).get()) {
            }

            FloatScratch(const FloatScratch&) = delete;
            FloatScratch& operator=(const FloatScratch&) = delete;

            Limb* get() noexcept {
                return _p;
            }

        private:
            static constexpr size_t Inline = 320;
            Limb _inline[Inline];
            std::unique_ptr<Limb[]> _heap;
            Limb* _p;
        };

        // whether a bit below i is set
        inline bool float_any_below(const Limb* p, size_t i) noexcept {
            size_t words = i / 64;
            for (size_t k = 0; k < words; ++k) {
                if (p[k]) {
                    return true;
                }
            }
            unsigned s = unsigned(i % 64);
            return s && (p[words] << (64 - s)) != 0;
        }
    }

    // The magnitude in limbs p[0..n) times 2^e rounded to `precision` bits,
    // as _round below, but on the limbs: the kept bits shifted down once into
    // working memory, the one added there, and the mantissa without its zeros
    // at the end made in one object (none for 63 bits or fewer)
    inline big_float big_float::_round_limbs(bool negative, const detail::Limb* p, size_t n, int64_t e, bool sticky,
                                             uint32_t precision, rounding mode) {
        using detail::Limb;
        n = detail::normalized(p, n);
        if (!n || (sticky && size_t(n) * 64 <= precision + 1)) {
            // nothing, or too few bits for the sticky bit's rule: the general road
            big_integer m;
            if (n) {
                detail::Result r(n);
                sgcl::detail::copy_bytes(r.data(), p, n * sizeof(Limb));
                m = r.finish(n, false);
            }
            return _round(negative, std::move(m), e, sticky, precision, mode);
        }
        if (precision == 0) {
            throw invalid_argument("sgcl::math::big_float: a precision of 0 for a value that is not zero");
        }
        size_t bits = n * 64 - size_t(std::countl_zero(p[n - 1]));
        size_t drop = bits > precision ? bits - precision : 0;
        bool increment = false;
        bool inexact = sticky;
        if (drop) {
            bool half = detail::float_bit(p, n, drop - 1);
            bool rest = sticky || detail::float_any_below(p, drop - 1);
            int cmp = half ? (rest ? 1 : 0) : -1;
            inexact = half || rest;
            increment = detail::round_away(mode, negative, detail::float_bit(p, n, drop), cmp, inexact);
        } else if (sticky) {
            increment = detail::round_away(mode, negative, p[0] & 1, -1, true);
        }
        big_float r;
        r._precision = precision;
        r._mode = mode;
        r._negative = negative;
        if (inexact && mode == rounding::unnecessary) {
            throw domain_error("sgcl::math::big_float: rounding::unnecessary, and the result is not exact");
        }
        if (!increment) {
            // the kept bits as they are: past the drop and the zeros above it,
            // shifted once straight into the mantissa's object
            size_t low = drop;
            while (!detail::float_bit(p, n, low)) {
                if (low % 64 == 0 && !p[low / 64]) {
                    low += 64;
                } else {
                    ++low;
                }
            }
            int64_t top = e + int64_t(bits);
            if (top > detail::FloatMaxExp) {
                r._form = Infinite;
                return r;
            }
            if (top < detail::FloatMinExp) {
                return r;
            }
            size_t words = low / 64;
            auto s = unsigned(low % 64);
            size_t mn = (bits - low + 63) / 64;
            detail::Result m(mn);
            if (s) {
                size_t avail = n - words;
                Limb* out = m.data();
                for (size_t i = 0; i < mn; ++i) {
                    out[i] = (p[words + i] >> s) | (i + 1 < avail ? p[words + i + 1] << (64 - s) : 0);
                }
            } else {
                sgcl::detail::copy_bytes(m.data(), p + words, mn * sizeof(Limb));
            }
            r._form = Finite;
            r._mantissa = m.finish(mn, false);
            r._exponent = e + int64_t(low);
            return r;
        }
        // the kept bits, one limb of room for the carry
        size_t kn = (bits - drop + 63) / 64 + 1;
        detail::FloatScratch kept(kn);
        Limb* q = kept.get();
        size_t words = drop / 64;
        auto s = unsigned(drop % 64);
        size_t avail = n - words;
        if (s) {
            detail::shift_right(q, p + words, avail, s);
        } else {
            sgcl::detail::copy_bytes(q, p + words, avail * sizeof(Limb));
        }
        for (size_t i = avail; i < kn; ++i) {
            q[i] = 0;
        }
        e += int64_t(drop);
        size_t qn = detail::normalized(q, kn);
        if (increment) {
            detail::add_one(q, q, qn);
            qn = detail::normalized(q, qn + 1);
            if (qn * 64 - size_t(std::countl_zero(q[qn - 1])) > precision) {
                // 2^precision: one bit off, exactly
                detail::shift_right(q, q, qn, 1);
                qn = detail::normalized(q, qn);
                ++e;
            }
        }
        size_t zeros = detail::trailing_zeros(q, qn);
        int64_t top = e + int64_t(qn * 64 - size_t(std::countl_zero(q[qn - 1])));
        if (top > detail::FloatMaxExp) {
            r._form = Infinite;
            return r;
        }
        if (top < detail::FloatMinExp) {
            return r;
        }
        size_t zw = zeros / 64;
        auto zs = unsigned(zeros % 64);
        size_t mn = qn - zw;
        detail::Result m(mn);
        if (zs) {
            detail::shift_right(m.data(), q + zw, mn, zs);
        } else {
            sgcl::detail::copy_bytes(m.data(), q + zw, mn * sizeof(Limb));
        }
        r._form = Finite;
        r._mantissa = m.finish(mn, false);
        r._exponent = e + int64_t(zeros);
        return r;
    }

    // The magnitude m·2^e (plus something below its last bit when sticky)
    // rounded to `precision` bits: the bits past it dropped, one added by
    // the mode, a carry that makes 2^precision taking one bit off; the zeros
    // at the end dropped; the range of the exponent checked. A caller that
    // sets sticky gives at least precision + 2 bits, so that the bit below
    // the last kept one is known exactly
    inline big_float big_float::_round(bool negative, big_integer m, int64_t e, bool sticky, uint32_t precision,
                                       rounding mode) {
        big_float r;
        r._precision = precision;
        r._mode = mode;
        r._negative = negative;
        if (m.sign() == 0) {
            if (sticky && mode == rounding::unnecessary) {
                throw domain_error("sgcl::math::big_float: rounding::unnecessary, and the result is not exact");
            }
            return r;   // a zero of the sign
        }
        if (precision == 0) {
            throw invalid_argument("sgcl::math::big_float: a precision of 0 for a value that is not zero");
        }
        size_t bits = m.bit_length();
        if (bits > precision) {
            size_t drop = bits - precision;
            bool half = m.bit(drop - 1);
            bool rest = sticky || (m.trailing_zeros() < drop - 1);
            m = m >> drop;
            e += int64_t(drop);
            int cmp = half ? (rest ? 1 : 0) : -1;
            if (detail::round_away(mode, negative, m.bit(0), cmp, half || rest)) {
                m += 1;
                if (m.bit_length() > precision) {
                    m = m >> 1;
                    ++e;
                }
            }
        } else if (sticky) {
            // a caller that sets sticky gives precision + 2 bits: never here
            if (detail::round_away(mode, negative, m.bit(0), -1, true)) {
                m += 1;
            }
        }
        size_t zeros = m.trailing_zeros();
        if (zeros) {
            m = m >> zeros;
            e += int64_t(zeros);
        }
        int64_t top = e + int64_t(m.bit_length());
        if (top > detail::FloatMaxExp) {
            r._form = Infinite;
            return r;
        }
        if (top < detail::FloatMinExp) {
            return r;
        }
        r._form = Finite;
        r._mantissa = std::move(m);
        r._exponent = e;
        return r;
    }

    inline big_float::big_float(const big_integer& value, uint32_t precision, rounding mode)
    : big_float() {
        *this = _round(value.sign() < 0, value.abs(), 0, false, precision, mode);
    }

    inline big_float::big_float(double value)
    : big_float() {
        if (std::isnan(value)) {
            throw domain_error("sgcl::math::big_float: a NaN is no number");
        }
        _precision = 53;
        _negative = std::signbit(value);
        if (std::isinf(value)) {
            _form = Infinite;
            return;
        }
        if (value == 0) {
            return;
        }
        int e;
        double f = std::frexp(std::fabs(value), &e);
        auto m = uint64_t(std::ldexp(f, 53));
        *this = _round(_negative, big_integer(m), e - 53, false, 53, rounding::half_even);
    }

    inline big_float::big_float(double value, uint32_t precision, rounding mode)
    : big_float(big_float(value), precision, mode) {
    }

    inline big_float::big_float(const big_float& value, uint32_t precision, rounding mode)
    : big_float() {
        if (value._form != Finite) {
            *this = value;
            _precision = precision;
            _mode = mode;
            return;
        }
        *this = _round(value._negative, value._mantissa, value._exponent, false, precision, mode);
    }

    // n/d to precision + 2 bits and the remainder as sticky
    inline big_float::big_float(const rational& value, uint32_t precision, rounding mode)
    : big_float() {
        const big_integer& n = value.numerator();
        const big_integer& d = value.denominator();
        if (n.sign() == 0) {
            _precision = precision;
            _mode = mode;
            return;
        }
        big_integer a = n.abs();
        int64_t shift = int64_t(precision) + 2 + int64_t(d.bit_length()) - int64_t(a.bit_length());
        if (shift < 0) {
            shift = 0;
        }
        auto [q, r] = (a << shift).div_rem(d);
        *this = _round(n.sign() < 0, std::move(q), -shift, r.sign() != 0, precision, mode);
    }

    inline big_float::big_float(const decimal& value, uint32_t precision, rounding mode)
    : big_float() {
        if (value.is_nan()) {
            throw domain_error("sgcl::math::big_float: a NaN is no number");
        }
        if (value.is_infinite()) {
            *this = infinity(value.sign() < 0);
            _precision = precision;
            _mode = mode;
            return;
        }
        *this = big_float(value.to_rational(), precision, mode);
    }

    inline big_float big_float::_add(const big_float& a, const big_float& b, bool subtract) {
        uint32_t precision = _precision_of(a, b);
        rounding mode = _mode_of(a, b);
        bool nb = b._negative != subtract;
        if (a._form == Infinite || b._form == Infinite) {
            if (a._form == Infinite && b._form == Infinite && a._negative != nb) {
                throw domain_error("sgcl::math::big_float: an infinity less itself is no number");
            }
            big_float r = infinity(a._form == Infinite ? a._negative : nb);
            r._precision = precision;
            r._mode = mode;
            return r;
        }
        if (b._form == Zero) {
            if (a._form == Zero) {
                // -0 + -0 is -0, every other sum of zeros +0 (Go's rule, in
                // every mode; IEEE makes +0 + -0 negative when rounding down)
                big_float r;
                r._precision = precision;
                r._mode = mode;
                r._negative = a._negative && nb;
                return r;
            }
            return big_float(a, precision, mode);
        }
        if (a._form == Zero) {
            big_float r(b, precision, mode);
            r._negative = nb;
            return r;
        }
        // b far below the last bit of a widened to precision + 2 bits: a
        // and a sticky bit (or a minus one and a sticky bit) round as the sum
        auto far = [&](const big_float& x, bool nx, const big_float& y, bool ny) -> optional<big_float> {
            int64_t xbits = int64_t(x._mantissa.bit_length());
            int64_t widen = std::max<int64_t>(0, int64_t(precision) + 2 - xbits);
            int64_t low = x._exponent - widen;
            int64_t top_y = y._exponent + int64_t(y._mantissa.bit_length());
            if (top_y >= low) {
                return nullopt;
            }
            big_integer m = x._mantissa << widen;
            if (nx != ny) {
                m -= 1;
            }
            return _round(nx, std::move(m), low, true, precision, mode);
        };
        int64_t top_a = a._exponent + int64_t(a._mantissa.bit_length());
        int64_t top_b = b._exponent + int64_t(b._mantissa.bit_length());
        if (top_a > top_b) {
            if (auto r = far(a, a._negative, b, nb)) {
                return *r;
            }
        } else if (auto r = far(b, nb, a, a._negative)) {
            return *r;
        }
        // both brought to the lower exponent on limbs, added or the smaller
        // taken from the larger, rounded once
        using detail::Limb;
        int64_t e = std::min(a._exponent, b._exponent);
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = detail::BigIntAccess::magnitude(a._mantissa, ar, an);
        const Limb* bp = detail::BigIntAccess::magnitude(b._mantissa, br, bn);
        // only the one of the higher exponent moves; the other is read where it is
        uint64_t sa = uint64_t(a._exponent - e);
        uint64_t sb = uint64_t(b._exponent - e);
        bool a_moves = sa != 0;
        const Limb* still = a_moves ? bp : ap;
        size_t still_n = a_moves ? bn : an;
        detail::FloatScratch moved(a_moves ? an + size_t(sa / 64) + 1 : bn + size_t(sb / 64) + 1);
        size_t moved_n = detail::normalized(moved.get(), a_moves ? detail::float_shift_left(moved.get(), ap, an, sa)
                                                                 : detail::float_shift_left(moved.get(), bp, bn, sb));
        const Limb* x = a_moves ? moved.get() : still;
        size_t xn = a_moves ? moved_n : still_n;
        const Limb* y = a_moves ? still : moved.get();
        size_t yn = a_moves ? still_n : moved_n;
        bool nx = a._negative;
        bool ny = nb;
        if (detail::compare(x, xn, y, yn) < 0) {
            std::swap(x, y);
            std::swap(xn, yn);
            std::swap(nx, ny);
        }
        detail::FloatScratch sum(xn + 1);
        size_t sn;
        if (nx == ny) {
            detail::add(sum.get(), x, xn, y, yn);
            sn = xn + 1;
        } else {
            detail::sub(sum.get(), x, xn, y, yn);
            sn = xn;
        }
        sn = detail::normalized(sum.get(), sn);
        if (!sn) {
            big_float r;
            r._precision = precision;
            r._mode = mode;
            r._negative = mode == rounding::floor;
            return r;
        }
        return _round_limbs(nx, sum.get(), sn, e, false, precision, mode);
    }

    inline big_float operator*(const big_float& a, const big_float& b) {
        uint32_t precision = big_float::_precision_of(a, b);
        rounding mode = big_float::_mode_of(a, b);
        bool negative = a._negative != b._negative;
        if (a._form == big_float::Infinite || b._form == big_float::Infinite) {
            if (a._form == big_float::Zero || b._form == big_float::Zero) {
                throw domain_error("sgcl::math::big_float: zero times an infinity is no number");
            }
            big_float r = big_float::infinity(negative);
            r._precision = precision;
            r._mode = mode;
            return r;
        }
        if (a._form == big_float::Zero || b._form == big_float::Zero) {
            big_float r;
            r._precision = precision;
            r._mode = mode;
            r._negative = negative;
            return r;
        }
        using detail::Limb;
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = detail::BigIntAccess::magnitude(a._mantissa, ar, an);
        const Limb* bp = detail::BigIntAccess::magnitude(b._mantissa, br, bn);
        detail::FloatScratch product(an + bn);
        if (an >= bn) {
            if (bn == 1) {
                product.get()[an] = detail::mul_1(product.get(), ap, an, bp[0]);
            } else {
                detail::mul(product.get(), ap, an, bp, bn);
            }
        } else if (an == 1) {
            product.get()[bn] = detail::mul_1(product.get(), bp, bn, ap[0]);
        } else {
            detail::mul(product.get(), bp, bn, ap, an);
        }
        return big_float::_round_limbs(negative, product.get(), an + bn, a._exponent + b._exponent, false, precision, mode);
    }

    inline big_float operator/(const big_float& a, const big_float& b) {
        uint32_t precision = big_float::_precision_of(a, b);
        rounding mode = big_float::_mode_of(a, b);
        bool negative = a._negative != b._negative;
        auto special = [&](big_float::Form form) {
            big_float r;
            r._form = form;
            r._precision = precision;
            r._mode = mode;
            r._negative = negative;
            return r;
        };
        if (a._form == big_float::Zero && b._form == big_float::Zero) {
            throw domain_error("sgcl::math::big_float: zero over zero is no number");
        }
        if (a._form == big_float::Infinite && b._form == big_float::Infinite) {
            throw domain_error("sgcl::math::big_float: an infinity over an infinity is no number");
        }
        if (a._form == big_float::Infinite || b._form == big_float::Zero) {
            return special(big_float::Infinite);
        }
        if (a._form == big_float::Zero || b._form == big_float::Infinite) {
            return special(big_float::Zero);
        }
        int64_t shift = int64_t(precision) + 2 + int64_t(b._mantissa.bit_length()) - int64_t(a._mantissa.bit_length());
        if (shift < 0) {
            shift = 0;
        }
        using detail::Limb;
        size_t an;
        size_t bn;
        Limb ar;
        Limb br;
        const Limb* ap = detail::BigIntAccess::magnitude(a._mantissa, ar, an);
        const Limb* bp = detail::BigIntAccess::magnitude(b._mantissa, br, bn);
        detail::FloatScratch us(an + size_t(shift) / 64 + 1);
        size_t un = detail::normalized(us.get(), detail::float_shift_left(us.get(), ap, an, uint64_t(shift)));
        size_t qn = un - bn + 1;
        detail::FloatScratch qs(qn + 1);
        bool rest;
        if (bn == 1) {
            rest = detail::div_1(qs.get(), us.get(), un, detail::Divisor(bp[0])) != 0;
        } else {
            detail::FloatScratch rs(bn);
            detail::divide(qs.get(), rs.get(), us.get(), un, bp, bn);
            rest = detail::normalized(rs.get(), bn) != 0;
        }
        return big_float::_round_limbs(negative, qs.get(), qn, a._exponent - shift - b._exponent, rest, precision, mode);
    }

    inline big_float big_float::sqrt() const {
        if (_form == Zero) {
            return *this;
        }
        if (_negative) {
            throw domain_error("sgcl::math::big_float::sqrt: the square root of a negative number");
        }
        if (_form == Infinite) {
            return *this;
        }
        // m·2^e with e even, and room for precision + 2 bits of root
        big_integer m = _mantissa;
        int64_t e = _exponent;
        if (e & 1) {
            m = m << 1;
            --e;
        }
        int64_t root_bits = (int64_t(m.bit_length()) + 1) / 2;
        int64_t shift = std::max<int64_t>(0, int64_t(_precision) + 2 - root_bits);
        m = m << uint64_t(2 * shift);
        e -= 2 * shift;
        big_integer root = m.sqrt();
        bool sticky = root * root != m;
        return _round(false, std::move(root), e / 2, sticky, _precision, _mode);
    }

    inline int big_float::_compare(const big_float& a, const big_float& b) noexcept {
        auto rank = [](const big_float& x) {
            if (x._form == Infinite) {
                return x._negative ? -2 : 2;
            }
            if (x._form == Zero) {
                return 0;
            }
            return x._negative ? -1 : 1;
        };
        int ra = rank(a);
        int rb = rank(b);
        if (ra != rb) {
            return ra < rb ? -1 : 1;
        }
        if (ra == 0 || ra == 2 || ra == -2) {
            return 0;
        }
        int64_t ta = a._exponent + int64_t(a._mantissa.bit_length());
        int64_t tb = b._exponent + int64_t(b._mantissa.bit_length());
        int c;
        if (ta != tb) {
            c = ta < tb ? -1 : 1;
        } else {
            int64_t e = std::min(a._exponent, b._exponent);
            auto o = (a._mantissa << uint64_t(a._exponent - e)) <=> (b._mantissa << uint64_t(b._exponent - e));
            c = o < 0 ? -1 : o > 0 ? 1 : 0;
        }
        return ra < 0 ? -c : c;
    }

    inline double big_float::to_double() const noexcept {
        if (_form == Zero) {
            return _negative ? -0.0 : 0.0;
        }
        if (_form == Infinite) {
            return _negative ? -HUGE_VAL : HUGE_VAL;
        }
        // the value in [2^E, 2^(E+1)): 53 bits, or fewer for a subnormal
        int64_t E = exponent() - 1;
        auto inf = _negative ? -HUGE_VAL : HUGE_VAL;
        if (E > 1023) {
            return inf;
        }
        int64_t precision = E >= -1022 ? 53 : E + 1075;
        if (precision <= 0) {
            // below 2^-1074: the smallest subnormal or zero, the half of it a
            // tie to zero (even) unless something lies above the half
            bool above_half = precision == 0 && !(_mantissa.bit_length() == 1);
            double v = above_half ? 0x1p-1074 : 0.0;
            return _negative ? -v : v;
        }
        big_float r = _round(_negative, _mantissa, _exponent, false, uint32_t(precision), rounding::half_even);
        if (r._form == Zero) {
            return _negative ? -0.0 : 0.0;
        }
        if (r.exponent() - 1 > 1023) {
            return inf;
        }
        double v = std::ldexp(double(*r._mantissa.to_uint64()), int(r._exponent));
        return _negative ? -v : v;
    }

    inline big_integer big_float::to_big_integer(rounding mode) const {
        if (_form == Infinite) {
            throw domain_error("sgcl::math::big_float::to_big_integer: an infinity is no whole number");
        }
        if (_form == Zero) {
            return big_integer();
        }
        big_integer v;
        if (_exponent >= 0) {
            v = _mantissa << uint64_t(_exponent);
        } else {
            v = detail::round_div(_negative ? -_mantissa : _mantissa, big_integer(1) << uint64_t(-_exponent), mode);
            return v;
        }
        return _negative ? -v : v;
    }

    inline rational big_float::to_rational() const {
        if (_form == Infinite) {
            throw domain_error("sgcl::math::big_float::to_rational: an infinity is no fraction");
        }
        if (_form == Zero) {
            return rational();
        }
        big_integer m = _negative ? -_mantissa : _mantissa;
        if (_exponent >= 0) {
            return rational(m << uint64_t(_exponent));
        }
        return rational(m, big_integer(1) << uint64_t(-_exponent));
    }

    inline decimal big_float::to_decimal() const {
        if (_form == Infinite) {
            return _negative ? -decimal::infinity() : decimal::infinity();
        }
        if (_form == Zero) {
            return decimal();
        }
        big_integer m = _negative ? -_mantissa : _mantissa;
        if (_exponent >= 0) {
            return decimal(m << uint64_t(_exponent));
        }
        // m·2^-k = m·5^k / 10^k
        uint64_t k = uint64_t(-_exponent);
        return decimal(m * big_integer(5).pow(int64_t(k)), detail::checked_scale(int64_t(k)));
    }

    // The shortest decimal within half a unit of the last place either side
    // (a quarter below a power of two, where the next value down is nearer;
    // the ends of the interval in when the mantissa is even, as rounding to
    // nearest-even reads them back): the fewest digits k for which a number
    // of k digits lies inside — found by halving, the question being true
    // for every k past the first — and of those the nearest to the value
    inline decimal big_float::_shortest() const {
        // the value as M·2^E with M of exactly `precision` bits
        size_t bits = _mantissa.bit_length();
        int64_t pad = int64_t(_precision) - int64_t(bits);
        big_integer M = pad >= 0 ? _mantissa << uint64_t(pad) : _mantissa;
        int64_t E = _exponent - (pad >= 0 ? pad : 0);
        bool power = M == big_integer(1) << uint64_t(_precision - 1);
        // x, the lower end and the upper end as whole numbers at one decimal
        // scale s: X = M·2^E·10^s, L = (2M - 1)·2^(E-1)·10^s (a quarter for a
        // power of two), U = (2M + 1)·2^(E-1)·10^s
        int64_t scale = std::max<int64_t>(0, 2 - E);
        big_integer five = scale ? big_integer(5).pow(scale) : big_integer(1);
        auto at_scale = [&](const big_integer& m, int64_t twos) {
            // m·5^s·2^(twos + s), twos + s >= 0
            return (m * five) << uint64_t(twos + scale);
        };
        big_integer X = at_scale(M, E);
        big_integer L = power ? at_scale((M << 2) - 1, E - 2) : at_scale((M << 1) - 1, E - 1);
        big_integer U = at_scale((M << 1) + 1, E - 1);
        bool inclusive = !M.bit(0);
        auto inside = [&](const big_integer& d) {
            return inclusive ? L <= d && d <= U : L < d && d < U;
        };
        // every number between L and U shares the digits L and U begin with:
        // the shortest keeps at least those, and is found a digit further at
        // a time, the nearest of the candidates first
        std::string lt = std::string(L.to_string().view());
        std::string ut = std::string(U.to_string().view());
        size_t length = ut.size();
        lt.insert(0, length - lt.size(), '0');
        size_t common = 0;
        while (common < length && lt[common] == ut[common]) {
            ++common;
        }
        for (size_t keep = std::max<size_t>(common, 1); keep <= length; ++keep) {
            size_t cut = length - keep;
            big_integer unit = detail::pow10(int64_t(cut));
            auto [q, r] = X.div_rem(unit);
            big_integer down = q * unit;
            big_integer up = down + unit;
            auto c = (r << 1) <=> unit;
            bool nearest_up = c > 0 || (c == 0 && q.bit(0));
            const big_integer& first = nearest_up ? up : down;
            const big_integer& second = nearest_up ? down : up;
            if (inside(first)) {
                return decimal(first, detail::checked_scale(scale));
            }
            if (inside(second)) {
                return decimal(second, detail::checked_scale(scale));
            }
        }
        return decimal(X, detail::checked_scale(scale));
    }

    namespace detail {
        // A decimal not below zero as Go's decimal type writes it: the
        // digits without zeros at either end and the place of the point,
        // the value 0.digits × 10^point; no digits for zero
        struct GoDigits {
            std::string digits;
            int64_t point = 0;
        };

        inline GoDigits go_digits(const decimal& d) {
            GoDigits g;
            if (d.sign() == 0) {
                return g;
            }
            g.digits = std::string(d.unscaled().abs().to_string().view());
            g.point = int64_t(g.digits.size()) - d.scale();
            while (!g.digits.empty() && g.digits.back() == '0') {
                g.digits.pop_back();
            }
            return g;
        }

        // %e with `places` digits after the point: d.ddde±dd
        inline std::string go_format_e(const GoDigits& g, int64_t places) {
            std::string s(1, g.digits.empty() ? '0' : g.digits[0]);
            if (places > 0) {
                s += '.';
                for (int64_t i = 1; i <= places; ++i) {
                    s += size_t(i) < g.digits.size() ? g.digits[size_t(i)] : '0';
                }
            }
            int64_t exp = g.digits.empty() ? 0 : g.point - 1;
            s += exp < 0 ? "e-" : "e+";
            std::string x = std::to_string(exp < 0 ? -exp : exp);
            return s + (x.size() < 2 ? "0" : "") + x;
        }

        // %f with `places` digits after the point
        inline std::string go_format_f(const GoDigits& g, int64_t places) {
            std::string s;
            if (g.point > 0) {
                for (int64_t i = 0; i < g.point; ++i) {
                    s += size_t(i) < g.digits.size() ? g.digits[size_t(i)] : '0';
                }
            } else {
                s = "0";
            }
            if (places > 0) {
                s += '.';
                for (int64_t i = 0; i < places; ++i) {
                    int64_t at = g.point + i;
                    s += at >= 0 && size_t(at) < g.digits.size() ? g.digits[size_t(at)] : '0';
                }
            }
            return s;
        }
    }

    inline string big_float::_text(char format, int digits) const {
        if (format != 'e' && format != 'f' && format != 'g' && format != 'p' && format != 'x') {
            throw invalid_argument("sgcl::math::big_float: a format other than e, f, g, p or x");
        }
        std::string sign = _negative ? "-" : "";
        if (_form == Infinite) {
            return string(_negative ? "-Inf" : "+Inf");
        }
        if (format == 'p') {
            if (_form == Zero) {
                return string(sign + "0");
            }
            // 0x.<the mantissa in hexadecimal, its zeros at the end dropped>p<exp>
            size_t bits = _mantissa.bit_length();
            size_t pad = (4 - bits % 4) % 4;
            std::string hex = std::string((_mantissa << pad).to_string(16).view());
            while (hex.size() > 1 && hex.back() == '0') {
                hex.pop_back();
            }
            int64_t exp = exponent();
            return string(sign + "0x." + hex + "p" + (exp >= 0 ? "+" : "") + std::to_string(exp));
        }
        if (format == 'x') {
            int64_t exp = 0;
            std::string hex = "0";
            if (_form == Finite) {
                // 1.<hex>p±ee: the first bit before the point, the rest in
                // hexadecimal digits, rounded half-even to `digits` of them
                big_integer m = _mantissa;
                exp = exponent() - 1;
                size_t frac_bits = m.bit_length() - 1;
                if (digits >= 0 && frac_bits > 4 * size_t(digits)) {
                    big_float r = _round(false, m, 0, false, uint32_t(1 + 4 * digits), rounding::half_even);
                    if (r.exponent() > int64_t(m.bit_length())) {
                        ++exp;   // carried up to the next power of two
                    }
                    m = r._mantissa;
                    frac_bits = m.bit_length() - 1;
                }
                size_t want = digits >= 0 ? size_t(digits) : (frac_bits + 3) / 4;
                hex = std::string((m << uint64_t(4 * want - frac_bits)).to_string(16).view());
            } else if (digits > 0) {
                hex += std::string(size_t(digits), '0');
            }
            std::string s = sign + "0x" + hex.substr(0, 1);
            if (hex.size() > 1) {
                s += "." + hex.substr(1);
            }
            std::string x = std::to_string(exp < 0 ? -exp : exp);
            return string(s + "p" + (exp < 0 ? "-" : "+") + (x.size() < 2 ? "0" : "") + x);
        }
        // the decimal formats, as Go's Text writes them: the shortest digits
        // that read back for -1, the exact value rounded half-even otherwise
        bool shortest = digits < 0;
        decimal d = _form == Zero ? decimal() : (shortest ? _shortest() : to_decimal().abs());
        detail::GoDigits g;
        if (format == 'e') {
            if (!shortest && d.sign() != 0) {
                d = d.round_precision(digits + 1);
            }
            g = detail::go_digits(d);
            return string(sign + detail::go_format_e(g, shortest ? std::max<int64_t>(int64_t(g.digits.size()) - 1, 0) : digits));
        }
        if (format == 'f') {
            if (!shortest) {
                d = d.rescale(digits);
            }
            g = detail::go_digits(d);
            int64_t places = shortest ? std::max<int64_t>(int64_t(g.digits.size()) - g.point, 0) : digits;
            return string(sign + detail::go_format_f(g, places));
        }
        int64_t precision = digits;
        if (shortest) {
            g = detail::go_digits(d);
            precision = int64_t(g.digits.size());
        } else {
            if (precision == 0) {
                precision = 1;
            }
            if (d.sign() != 0) {
                d = d.round_precision(int32_t(precision));
            }
            g = detail::go_digits(d);
        }
        int64_t count = int64_t(g.digits.size());
        int64_t eprec = precision;
        if (eprec > count && count >= g.point) {
            eprec = count;
        }
        if (shortest) {
            eprec = 6;
        }
        int64_t exp = g.point - 1;
        if (exp < -4 || exp >= eprec) {
            if (precision > count) {
                precision = count;
            }
            return string(sign + detail::go_format_e(g, std::max<int64_t>(precision - 1, 0)));
        }
        if (precision > g.point) {
            precision = count;
        }
        return string(sign + detail::go_format_f(g, std::max<int64_t>(precision - g.point, 0)));
    }

    inline string big_float::to_string() const {
        if (_form == Infinite) {
            return string(_negative ? "-Inf" : "+Inf");
        }
        if (_form == Zero) {
            return string(_negative ? "-0" : "0");
        }
        // the shortest digits, plain but for an exponent below -4 or of 21
        // and more (Go's String chooses at 6)
        detail::GoDigits g = detail::go_digits(_shortest());
        int64_t count = int64_t(g.digits.size());
        int64_t exp = g.point - 1;
        std::string text = exp < -4 || exp >= 21 ? detail::go_format_e(g, count - 1)
                                                 : detail::go_format_f(g, std::max<int64_t>(count - g.point, 0));
        return string((_negative ? "-" : "") + text);
    }

    inline string big_float::to_scientific() const {
        return _text('e', -1);
    }

    namespace detail {
        // Go's hexadecimal exponent of at least two digits as C's %a writes
        // it, with as many as it takes
        inline string c_hex(const string& go) {
            std::string s(go.data(), go.size());
            size_t p = s.rfind('p');
            if (p != std::string::npos && p + 3 < s.size() && s[p + 2] == '0') {
                s.erase(p + 2, 1);
            }
            return string(s);
        }
    }

    inline string big_float::to_hex() const {
        return detail::c_hex(_text('x', -1));
    }

    inline expected<big_float, parse_error> big_float::parse(const string& text, uint32_t precision, rounding mode) noexcept {
        std::string_view t(text.data(), text.size());
        size_t at = 0;
        bool negative = false;
        if (!t.empty() && (t[0] == '+' || t[0] == '-')) {
            negative = t[0] == '-';
            at = 1;
        }
        // Inf and Infinity, any case
        auto word = [&](std::string_view w) {
            if (t.size() - at != w.size()) {
                return false;
            }
            for (size_t i = 0; i < w.size(); ++i) {
                if ((t[at + i] | 0x20) != w[i]) {
                    return false;
                }
            }
            return true;
        };
        if (word("inf") || word("infinity")) {
            big_float f = infinity(negative);
            f._precision = precision;
            f._mode = mode;
            return f;
        }
        // hexadecimal: 0x, hex digits with a point, an optional p exponent
        if (t.size() - at > 2 && t[at] == '0' && (t[at + 1] | 0x20) == 'x') {
            using Reason = parse_error::Reason;
            size_t i = at + 2;
            std::string digits;
            int64_t fraction = 0;
            bool point = false;
            for (; i < t.size(); ++i) {
                char c = t[i];
                if (c == '.' && !point) {
                    point = true;
                    continue;
                }
                if (detail::digit_value(c) >= 16) {
                    break;
                }
                digits += c;
                fraction += point;
            }
            if (digits.empty()) {
                return unexpected<parse_error>(parse_error(Reason::no_digits, i, 16));
            }
            int64_t exponent = 0;
            if (i < t.size() && (t[i] | 0x20) == 'p') {
                size_t first = ++i;
                bool exponent_negative = false;
                if (i < t.size() && (t[i] == '+' || t[i] == '-')) {
                    exponent_negative = t[i] == '-';
                    ++i;
                }
                size_t digit_first = i;
                for (; i < t.size() && t[i] >= '0' && t[i] <= '9'; ++i) {
                    exponent = std::min<int64_t>(exponent * 10 + (t[i] - '0'), int64_t(1) << 40);
                }
                if (digit_first == i) {
                    return unexpected<parse_error>(parse_error(i < t.size() ? Reason::invalid_digit : Reason::no_digits, i, 10));
                }
                if (exponent > (int64_t(1) << 33)) {
                    return unexpected<parse_error>(parse_error(Reason::exponent_out_of_range, first, 10));
                }
                if (exponent_negative) {
                    exponent = -exponent;
                }
            }
            if (i < t.size()) {
                return unexpected<parse_error>(parse_error(Reason::invalid_digit, i, 16));
            }
            big_integer m = *big_integer::parse(string(digits), 16);
            if (m.sign() == 0) {
                big_float f;
                f._precision = precision;
                f._mode = mode;
                f._negative = negative;
                return f;
            }
            if (precision == 0) {
                return big_float(big_integer(1), 0, mode);   // invalid_argument, as asked
            }
            return _round(negative, std::move(m), exponent - 4 * fraction, false, precision, mode);
        }
        // decimal: read exactly as a rational and rounded once
        auto r = rational::parse(text);
        if (!r) {
            return unexpected<parse_error>(r.error());
        }
        if (r->numerator().sign() == 0) {
            big_float f;
            f._precision = precision;
            f._mode = mode;
            f._negative = negative;
            return f;
        }
        return big_float(*r, precision, mode);
    }

    // txt::format: {} the shortest (to_string()); {:e} {:f} {:g} with the
    // precision as digits after the point (significant ones for g), the
    // shortest that reads back when none is given (Go's formats); {:a} the
    // hexadecimal (to_hex(), or rounded to the precision's hexadecimal
    // digits); + and a space for the sign of a number not negative, width,
    // fill and alignment; the infinities as "+Inf" and "-Inf" whatever is asked
    inline void format_value(txt::format_sink& out, const big_float& v, const txt::format_spec& spec) {
        if (v.is_infinite()) {
            string text = v.to_string();   // "+Inf", "-Inf": the sign their own
            txt::detail::put_padded(out, {text.data(), text.size()}, spec, {}, '>');
            return;
        }
        char type = spec.type ? spec.type : 'g';
        string text = !spec.type && spec.precision < 0 ? v.abs().to_string()
                    : type == 'a' ? detail::c_hex(v.abs()._text('x', spec.precision))
                                  : v.abs()._text(type, spec.precision);
        char head[1];
        size_t head_size = 0;
        if (v.signbit()) {
            head[head_size++] = '-';
        } else if (spec.sign == '+' || spec.sign == ' ') {
            head[head_size++] = spec.sign;
        }
        txt::detail::put_padded(out, {text.data(), text.size()}, spec, {head, head_size}, '>');
    }

    SGCL_INLINE_HOT std::ostream& operator<<(std::ostream& os, const big_float& v) {
        auto s = v.to_string();
        return os << std::string_view(s.data(), s.size());
    }
}

template<>
struct sgcl::txt::formatter<sgcl::math::big_float> {
    SGCL_INLINE_HOT static constexpr bool takes(char type) noexcept {
        return !type || type == 'e' || type == 'f' || type == 'g' || type == 'a';
    }

    SGCL_INLINE_HOT static constexpr bool takes_precision() noexcept {
        return true;
    }
};

template<>
struct std::hash<sgcl::math::big_float> {
    size_t operator()(const sgcl::math::big_float& v) const noexcept {
        if (v._form != sgcl::math::big_float::Finite) {
            return v._form == sgcl::math::big_float::Zero ? 0 : (v._negative ? 1 : 2);
        }
        size_t h = std::hash<sgcl::math::big_integer>()(v._mantissa);
        h ^= size_t(v._exponent) * 0x9e3779b97f4a7c15ull + (v._negative ? 0x5bd1e995 : 0);
        return h;
    }
};

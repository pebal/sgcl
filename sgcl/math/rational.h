//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "big_integer.h"

#include <cmath>
#include <compare>
#include <concepts>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

// A fraction of two whole numbers of any size, always in lowest terms:
// what Go's math/big.Rat is, with the manners of a number. The operators
// are the arithmetic's own and nothing is ever rounded — 1/3 + 1/3 + 1/3
// is 1 — and a big_integer or any whole number of the language goes where a
// rational is wanted, so r * 2, r < 1 and r == big_integer(5) are written
// as they read.
//
// Two big_integers, the numerator with the sign and the denominator above
// zero with no factor in common with it, so each value has one form and
// equality compares the parts. Like big_integer it is a value built of
// immutable objects: a copy is four words, the objects are shared, and it
// lives where a tracked_ptr may. The sums and products take the common
// factors out as Knuth does (TAOCP vol. 2, 4.5.1), so the gcds are of the
// smaller numbers.
//
// A denominator of zero, the inverse of zero, NaN and the infinities made
// into a fraction are errors of the program (domain_error); text that is
// not a fraction is an expected with the reason.
namespace sgcl::math {
    class rational {
    public:
        // Zero
        rational() noexcept
        : _denominator(1) {
        }

        // n/1, implicitly, from any whole number but bool (allocates
        // nothing for a value int64_t holds, as big_integer)
        template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        rational(T value) noexcept(std::is_nothrow_constructible_v<big_integer, T>)
        : _numerator(value)
        , _denominator(1) {
        }

        // n/1, implicitly
        rational(big_integer value) noexcept
        : _numerator(std::move(value))
        , _denominator(1) {
        }

        // numerator/denominator in lowest terms, the sign on the numerator;
        // a denominator of zero is domain_error
        rational(big_integer numerator, big_integer denominator)
        : _numerator(std::move(numerator))
        , _denominator(std::move(denominator)) {
            if (_denominator.sign() == 0) {
                throw domain_error("sgcl::math::rational: a denominator of zero");
            }
            _reduce();
        }

        // The double exactly — every finite double is a fraction whose
        // denominator is a power of two, and that is the one given: so
        // rational(0.1) is 3602879701896397/36028797018963968 and not 1/10.
        // NaN and the infinities are domain_error.
        explicit rational(double value);

        // Not from a bool (which would arrive as a whole number), nor from
        // a long double (which would arrive rounded to a double)
        rational(bool) = delete;
        explicit rational(long double) = delete;

        // A fraction "3/4", "-5", "+0/7", or a decimal "-0.125", "1.5e-3",
        // ".5", "7.", "2E10": an optional sign, then digits and a slash
        // and digits (no sign in the denominator), or a decimal with an
        // optional exponent; nothing else, no space. A denominator of zero
        // and an exponent past a million either way are errors of the data
        // — the second because a few bytes would ask for megabytes — at the
        // offset of the denominator or the exponent.
        static expected<rational, parse_error> parse(const string& text);

        rational(const rational&) noexcept = default;
        rational(rational&&) noexcept = default;
        rational& operator=(const rational&) noexcept = default;
        rational& operator=(rational&&) noexcept = default;

        // The numerator, with the sign, and the denominator, above zero
        // and with no factor in common with it
        const big_integer& numerator() const noexcept {
            return _numerator;
        }

        const big_integer& denominator() const noexcept {
            return _denominator;
        }

        friend rational operator+(const rational& a, const rational& b) {
            return _add(a, b, false);
        }

        friend rational operator-(const rational& a, const rational& b) {
            return _add(a, b, true);
        }

        // (a/b)·(c/d) with gcd(a, d) and gcd(c, b) taken out first, so the
        // product is in lowest terms without a gcd of the products
        friend rational operator*(const rational& a, const rational& b) {
            if (a._denominator == 1 && b._denominator == 1) {
                return rational(a._numerator * b._numerator, Reduced{});
            }
            big_integer g1 = a._numerator.gcd(b._denominator);
            big_integer g2 = b._numerator.gcd(a._denominator);
            return rational(Reduced{}, (a._numerator / g1) * (b._numerator / g2),
                            (a._denominator / g2) * (b._denominator / g1));
        }

        // A division by zero is domain_error
        friend rational operator/(const rational& a, const rational& b) {
            return a * b.inverse();
        }

        rational operator-() const {
            return rational(Reduced{}, -_numerator, _denominator);
        }

        rational& operator+=(const rational& b) {
            return *this = *this + b;
        }

        rational& operator-=(const rational& b) {
            return *this = *this - b;
        }

        rational& operator*=(const rational& b) {
            return *this = *this * b;
        }

        rational& operator/=(const rational& b) {
            return *this = *this / b;
        }

        // Lowest terms make equality a comparison of the parts
        friend bool operator==(const rational& a, const rational& b) noexcept {
            return a._numerator == b._numerator && a._denominator == b._denominator;
        }

        // a/b against c/d as a·d against c·b (the denominators above zero)
        friend std::strong_ordering operator<=>(const rational& a, const rational& b) {
            int sa = a._numerator.sign();
            int sb = b._numerator.sign();
            if (sa != sb) {
                return sa <=> sb;
            }
            if (a._denominator == b._denominator) {
                return a._numerator <=> b._numerator;
            }
            return a._numerator * b._denominator <=> b._numerator * a._denominator;
        }

        rational abs() const {
            return _numerator.sign() < 0 ? -*this : *this;
        }

        // 1/x; the inverse of zero is domain_error
        rational inverse() const {
            if (_numerator.sign() == 0) {
                throw domain_error("sgcl::math::rational::inverse: the inverse of zero");
            }
            if (_numerator.sign() < 0) {
                return rational(Reduced{}, -_denominator, -_numerator);
            }
            return rational(Reduced{}, _denominator, _numerator);
        }

        // x^exponent, a negative exponent the power of the inverse: (2/3)^-2
        // is 9/4; zero to a negative power is domain_error, and 0^0 is 1.
        // The parts are raised each (a power of a fraction in lowest terms
        // is in lowest terms).
        rational pow(int64_t exponent) const {
            if (exponent < 0) {
                rational r = inverse();
                // -INT64_MIN does not fit; the magnitude as unsigned is split
                // into a power of 2^62 and the rest
                uint64_t m = uint64_t(0) - uint64_t(exponent);
                if (m > uint64_t(INT64_MAX)) {
                    rational half = r.pow(int64_t(m / 2));
                    return half * half;
                }
                return r.pow(int64_t(m));
            }
            return rational(Reduced{}, _numerator.pow(exponent), _denominator.pow(exponent));
        }

        // The largest whole number not above, and the smallest not below
        big_integer floor() const {
            auto [q, r] = _numerator.div_rem(_denominator);
            return r.sign() < 0 ? q - 1 : q;
        }

        big_integer ceil() const {
            auto [q, r] = _numerator.div_rem(_denominator);
            return r.sign() > 0 ? q + 1 : q;
        }

        // "3/4", "-5" for a whole number
        string to_string() const {
            if (_denominator == 1) {
                return _numerator.to_string();
            }
            std::string s;
            auto n = _numerator.to_string();
            auto d = _denominator.to_string();
            s.reserve(n.size() + 1 + d.size());
            s.append(n.data(), n.size());
            s += '/';
            s.append(d.data(), d.size());
            return string(s);
        }

        // The value with `places` digits after the point, rounded to the
        // nearest and a half away from zero, as Go's FloatString and the
        // rounding taught at school: 2/3 is "0.667", -1/8 to two places
        // "-0.13", 1/2 to none "1". A negative value keeps its minus when
        // it rounds to zero ("-0.00"), as printf does; no places, no point.
        string to_decimal(size_t places) const;

        // The nearest double, a tie to the even one — rounded once, from the
        // exact value (so not the quotient of the two parts' doubles, which
        // rounds three times); past the largest double an infinity of the
        // sign, below the smallest a zero of the sign. Not noexcept: the
        // division of long parts allocates.
        double to_double() const;

    private:
        struct Reduced {};

        // The parts already in lowest terms, the denominator above zero
        rational(Reduced, big_integer numerator, big_integer denominator) noexcept
        : _numerator(std::move(numerator))
        , _denominator(std::move(denominator)) {
        }

        rational(big_integer whole, Reduced) noexcept
        : _numerator(std::move(whole))
        , _denominator(1) {
        }

        void _reduce() {
            if (_denominator.sign() < 0) {
                _numerator = -_numerator;
                _denominator = -_denominator;
            }
            if (_numerator.sign() == 0) {
                _denominator = 1;
                return;
            }
            if (_denominator == 1) {
                return;
            }
            big_integer g = _numerator.gcd(_denominator);
            if (g != 1) {
                _numerator /= g;
                _denominator /= g;
            }
        }

        // a/b ± c/d, Knuth's way: with g = gcd(b, d), the numerator
        // t = a·(d/g) ± c·(b/g) and the denominator (b/g)·d, of which only
        // a factor of g can be common with t — so the second gcd is of t
        // and g, not of t and the whole product
        static rational _add(const rational& x, const rational& y, bool subtract) {
            const big_integer& a = x._numerator;
            const big_integer& b = x._denominator;
            const big_integer& d = y._denominator;
            big_integer c = subtract ? -y._numerator : y._numerator;
            if (b == 1 && d == 1) {
                return rational(a + c, Reduced{});
            }
            big_integer g = b.gcd(d);
            if (g == 1) {
                return rational(Reduced{}, a * d + c * b, b * d);
            }
            big_integer bg = b / g;
            big_integer t = a * (d / g) + c * bg;
            if (t.sign() == 0) {
                return rational();
            }
            big_integer h = t.gcd(g);
            if (h == 1) {
                return rational(Reduced{}, std::move(t), bg * d);
            }
            return rational(Reduced{}, t / h, bg * (d / h));
        }

        size_t _hash() const noexcept {
            std::hash<big_integer> h;
            size_t x = h(_numerator);
            return x ^ (h(_denominator) + 0x9e3779b97f4a7c15ull + (x << 6) + (x >> 2));
        }

        big_integer _numerator;
        big_integer _denominator;

        friend struct std::hash<rational>;
    };

    inline rational::rational(double value) {
        if (!std::isfinite(value)) {
            throw domain_error("sgcl::math::rational: a NaN or an infinity is no fraction");
        }
        if (value == 0) {
            _denominator = 1;
            return;
        }
        // value = m · 2^(e - 53) with m a whole number of 53 bits
        int e;
        double m = std::frexp(std::fabs(value), &e);
        auto mantissa = int64_t(std::ldexp(m, 53));
        int shift = e - 53;
        // The twos of the mantissa come off the power of two first
        int twos = std::countr_zero(uint64_t(mantissa));
        mantissa >>= twos;
        shift += twos;
        big_integer n = value < 0 ? -mantissa : mantissa;
        if (shift >= 0) {
            _numerator = n << shift;
            _denominator = 1;
        } else {
            _numerator = std::move(n);
            _denominator = big_integer(1) << -shift;
        }
    }

    inline string rational::to_decimal(size_t places) const {
        // |x|·10^places, rounded half away from zero
        big_integer scale = big_integer(10).pow(int64_t(places));
        auto [q, r] = (_numerator.abs() * scale).div_rem(_denominator);
        if (2 * r >= _denominator) {
            q += 1;
        }
        auto digits = q.to_string();
        std::string s;
        if (_numerator.sign() < 0) {
            s += '-';
        }
        if (!places) {
            s.append(digits.data(), digits.size());
            return string(s);
        }
        if (digits.size() <= places) {
            s += "0.";
            s.append(places - digits.size(), '0');
            s.append(digits.data(), digits.size());
        } else {
            s.append(digits.data(), digits.size() - places);
            s += '.';
            s.append(digits.data() + digits.size() - places, places);
        }
        return string(s);
    }

    // The exponent E of the value (2^E <= |x| < 2^(E+1)) decides how many
    // bits the double keeps: 53, or fewer for a subnormal; the quotient of
    // |n|·2^k by d to that many bits, its remainder against half the
    // divisor for the rounding, and a scaling that is exact
    inline double rational::to_double() const {
        if (_numerator.sign() == 0) {
            return 0.0;
        }
        if (_denominator == 1) {
            return _numerator.to_double();
        }
        bool negative = _numerator.sign() < 0;
        big_integer n = _numerator.abs();
        const big_integer& d = _denominator;
        auto e = int64_t(n.bit_length()) - int64_t(d.bit_length());
        // n/d < 2^e is the one case of the two lengths where E = e - 1
        if (e >= 0 ? n < (d << e) : (n << -e) < d) {
            --e;
        }
        auto signed_zero = [negative] {
            return negative ? -0.0 : 0.0;
        };
        auto infinity = [negative] {
            return negative ? -HUGE_VAL : HUGE_VAL;
        };
        if (e > 1023) {
            return infinity();
        }
        int64_t precision = e >= -1022 ? 53 : e + 1075;
        if (precision < 0) {
            return signed_zero();   // below half the smallest subnormal
        }
        if (precision == 0) {
            // [2^-1075, 2^-1074): the smallest subnormal, but for the tie
            // at 2^-1075 itself, which goes to the even zero
            bool tie = (n << 1075) == d;
            return tie ? signed_zero() : (negative ? -0x1p-1074 : 0x1p-1074);
        }
        int64_t shift = precision - 1 - e;
        big_integer q;
        big_integer r;
        big_integer divisor = d;
        if (shift >= 0) {
            std::tie(q, r) = (n << shift).div_rem(d);
        } else {
            divisor = d << -shift;
            std::tie(q, r) = n.div_rem(divisor);
        }
        auto c = (r << 1) <=> divisor;
        auto m = *q.to_uint64();
        if (c > 0 || (c == 0 && (m & 1))) {
            ++m;
            if (m == uint64_t(1) << precision) {
                m >>= 1;
                ++e;
                if (e > 1023) {
                    return infinity();
                }
            }
        }
        double v = std::ldexp(double(m), int(e - precision + 1));
        return negative ? -v : v;
    }

    inline expected<rational, parse_error> rational::parse(const string& text) {
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
        auto is_digit = [](char c) {
            return c >= '0' && c <= '9';
        };
        size_t whole_first = at;
        while (at < size && is_digit(p[at])) {
            ++at;
        }
        size_t whole_last = at;
        // A fraction: digits, a slash, digits
        if (at < size && p[at] == '/') {
            if (whole_first == whole_last) {
                return fail(Reason::no_digits, at);
            }
            size_t denominator_first = ++at;
            while (at < size && is_digit(p[at])) {
                ++at;
            }
            if (at < size) {
                return fail(Reason::invalid_digit, at);
            }
            if (denominator_first == size) {
                return fail(Reason::no_digits, size);
            }
            std::string numerator(p + whole_first, whole_last - whole_first);
            std::string denominator(p + denominator_first, size - denominator_first);
            auto n = big_integer::parse(string(numerator));
            auto d = big_integer::parse(string(denominator));
            if (d->sign() == 0) {
                return fail(Reason::zero_denominator, denominator_first);
            }
            return rational(negative ? -*n : *n, *d);
        }
        // A decimal: digits, a point and digits (either side may be empty,
        // not both), an exponent
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
        if (at < size && (p[at] == 'e' || p[at] == 'E')) {
            size_t exponent_at = ++at;
            bool exponent_negative = false;
            if (at < size && (p[at] == '+' || p[at] == '-')) {
                exponent_negative = p[at] == '-';
                ++at;
            }
            size_t digits_first = at;
            constexpr int64_t Limit = 1'000'000;
            bool past = false;
            while (at < size && is_digit(p[at])) {
                exponent = exponent * 10 + (p[at] - '0');
                if (exponent > Limit) {
                    past = true;
                    exponent = Limit + 1;   // held, so that it cannot overflow
                }
                ++at;
            }
            if (at < size) {
                return fail(Reason::invalid_digit, at);
            }
            if (digits_first == at) {
                return fail(Reason::no_digits, at);
            }
            if (past) {
                return fail(Reason::exponent_out_of_range, exponent_at);
            }
            if (exponent_negative) {
                exponent = -exponent;
            }
        }
        if (at < size) {
            return fail(Reason::invalid_digit, at);
        }
        // The digits of both sides as one whole number, over 10 to the
        // length of the fraction less the exponent
        std::string digits(p + whole_first, whole_last - whole_first);
        digits.append(p + fraction_first, fraction_last - fraction_first);
        auto value = big_integer::parse(string(digits));
        big_integer n = negative ? -*value : *value;
        int64_t scale = int64_t(fraction_last - fraction_first) - exponent;
        if (scale <= 0) {
            return rational(n * big_integer(10).pow(-scale), Reduced{});
        }
        return rational(std::move(n), big_integer(10).pow(scale));
    }

    // txt::format: {} is to_string ("3/4"), {:f} and {:.Nf} (and {:.N})
    // to_decimal with N places, six when none is given, as for a double;
    // + and a space for the sign of a value not negative, the width, the
    // fill and the alignment (right by default, as for any number), the
    // zeros of {:010.3f} after the sign
    inline void format_value(txt::format_sink& out, const rational& v, const txt::format_spec& spec) {
        string text = spec.type == 'f' || spec.precision >= 0
                          ? v.abs().to_decimal(spec.precision >= 0 ? size_t(spec.precision) : 6)
                          : v.abs().to_string();
        char head[1];
        size_t head_size = 0;
        if (v.numerator().sign() < 0) {
            head[head_size++] = '-';
        } else if (spec.sign == '+' || spec.sign == ' ') {
            head[head_size++] = spec.sign;
        }
        txt::detail::put_padded(out, {text.data(), text.size()}, spec, {head, head_size}, '>');
    }

    inline std::ostream& operator<<(std::ostream& os, const rational& v) {
        auto s = v.to_string();
        return os << std::string_view(s.data(), s.size());
    }
}

// Which specifications a rational takes, for the pattern checked where it
// is compiled; the writing is format_value's, above
template<>
struct sgcl::txt::formatter<sgcl::math::rational> {
    static constexpr bool takes(char type) noexcept {
        return !type || type == 'f';
    }

    static constexpr bool takes_precision() noexcept {
        return true;
    }
};

template<>
struct std::hash<sgcl::math::rational> {
    size_t operator()(const sgcl::math::rational& v) const noexcept {
        return v._hash();
    }
};

//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "expected.h"
#include "string.h"

#include <chrono>
#include <compare>
#include <concepts>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <ratio>
#include <type_traits>

namespace sgcl {
    // Why a text is not a duration: a sentence and the byte of the text
    // the reading stopped on. The sentence is a literal, so the error is
    // two words and costs nothing to make or to copy.
    class duration_error {
    public:
        constexpr duration_error(const char* reason, size_t offset) noexcept
        : _reason(reason)
        , _offset(offset) {
        }

        string message() const {
            return string(_reason);
        }

        size_t offset() const noexcept {
            return _offset;
        }

    private:
        const char* _reason;
        size_t _offset;
    };

    // A span of time, what Go's time.Duration is: a signed count of
    // nanoseconds in 64 bits, about 292 years either way. The methods
    // read it in the units a person thinks in (seconds() is 1.5, not
    // 1500000000), to_string() and parse() are Go's text ("1h30m",
    // "1.5µs", "-2m0.5s"), and the constants nanosecond ... hour make
    // one (90 * second).
    //
    // It is the library's one duration: every std::chrono::duration of
    // an integral count in a whole number of nanoseconds converts into it
    // implicitly, exactly (1h, 30min, 500ms), and it converts implicitly
    // into std::chrono::nanoseconds, so a function that takes that type
    // takes a duration as it is. A duration of a floating count converts
    // only explicitly (truncated toward zero, as duration_cast does); one
    // of a unit finer than a nanosecond does not convert at all (a
    // duration_cast to nanoseconds first). A function template of the
    // standard that deduces its duration (duration_cast, floor,
    // condition_variable::wait_for, hh_mm_ss, std::format) does not see
    // through a class: it takes std::chrono::nanoseconds(d). The mixed
    // operators a class needs to sit among the standard's types are
    // hidden friends: a time_point plus or minus a duration, a duration
    // compared with, added to or subtracted from a std::chrono one.
    //
    // Arithmetic saturates at the ends of the range instead of wrapping:
    // a sum, a product, a negation or a conversion that would not fit is
    // the largest or the smallest duration (Go wraps silently). A
    // division by zero is a division by zero, as for an int.
    class duration {
    public:
        constexpr duration() noexcept = default;

        // From a std::chrono::duration of an integral count whose unit is
        // a whole number of nanoseconds: 1h, 30min, 500ms, 7ns
        template<class Rep, class Period>
        requires std::is_integral_v<Rep> && (std::ratio_divide<Period, std::nano>::den == 1)
        constexpr duration(std::chrono::duration<Rep, Period> d) noexcept
        : _ns(_scaled(d.count(), std::ratio_divide<Period, std::nano>::num)) {
        }

        // From a std::chrono::duration of a floating count, which may
        // carry a part of a nanosecond: truncated toward zero, a NaN zero
        template<class Rep, class Period>
        requires std::is_floating_point_v<Rep>
        explicit constexpr duration(std::chrono::duration<Rep, Period> d) noexcept
        : _ns(_truncated(std::chrono::duration<long double, std::nano>(d).count())) {
        }

        // Into the standard's nanoseconds: what a std::chrono function
        // taking a duration of that type receives without a cast
        constexpr operator std::chrono::nanoseconds() const noexcept {
            return std::chrono::nanoseconds(_ns);
        }

        // The zero duration, as std::chrono has it (the same as duration())
        static constexpr duration zero() noexcept {
            return duration();
        }

        // The largest and the smallest duration, some 292 years either
        // way: "never" for a timer (a point this far ahead saturates at
        // its clock's max(), which the timers of async never fire)
        static constexpr duration max() noexcept {
            return _max();
        }

        static constexpr duration min() noexcept {
            return _min();
        }

        // Go's text: a sign, then one or more numbers each with a unit,
        // the number a decimal with an optional fraction ("1.5h", ".5s",
        // "1.s"), the unit one of ns, us, µs (U+00B5 or U+03BC), ms, s,
        // m, h; "0" alone is zero. "1h30m", "-1.5h", "300ms", "2h45m0.5s".
        // A fraction is taken exactly and truncated to the nanosecond.
        // Nothing else is accepted: no spaces, no days (a day of a
        // calendar is 23, 24 or 25 hours: that is a date's add_days).
        static expected<duration, duration_error> parse(const string& text);

        // The whole nanoseconds, microseconds and milliseconds, the last
        // two truncated toward zero
        constexpr int64_t nanoseconds() const noexcept {
            return _ns;
        }

        constexpr int64_t microseconds() const noexcept {
            return _ns / 1000;
        }

        constexpr int64_t milliseconds() const noexcept {
            return _ns / 1000000;
        }

        // The seconds, minutes and hours with a fraction: 1.5, 0.025. The
        // whole units and the rest are converted apart, so that a long
        // duration keeps its nanoseconds as far as a double can
        constexpr double seconds() const noexcept {
            return _in(Second);
        }

        constexpr double minutes() const noexcept {
            return _in(Minute);
        }

        constexpr double hours() const noexcept {
            return _in(Hour);
        }

        // The absolute value; the smallest duration, which has no
        // positive counterpart, gives the largest
        constexpr duration abs() const noexcept {
            return _ns < 0 ? -*this : *this;
        }

        // Toward zero to a multiple of step; a step of zero or less
        // leaves the duration as it is
        constexpr duration truncate(duration step) const noexcept {
            if (step._ns <= 0) {
                return *this;
            }
            return _raw(_ns - _ns % step._ns);
        }

        // To the nearest multiple of step, a half away from zero, the
        // result saturated at the ends of the range; a step of zero or
        // less leaves the duration as it is
        constexpr duration round(duration step) const noexcept {
            if (step._ns <= 0) {
                return *this;
            }
            uint64_t m = static_cast<uint64_t>(step._ns);
            uint64_t u = _magnitude(_ns);
            uint64_t r = u % m;
            uint64_t down = u - r;
            if (r < m - r) {
                return _signed(down, _ns < 0);
            }
            return _signed(down + m, _ns < 0);   // below 2^64 (u is at most 2^63, m below it), saturated by _signed
        }

        // Go's text: "1h30m0.5s", "1.5µs", "300ms", "0s", "-2m". The
        // units down from the hour, the leading ones that are zero left
        // out, the seconds with their fraction; a duration under a second
        // in the largest unit that makes its first digit non-zero
        string to_string() const;

        // The arithmetic, saturated at the ends of the range
        friend constexpr duration operator+(duration a, duration b) noexcept {
            int64_t r = static_cast<int64_t>(static_cast<uint64_t>(a._ns) + static_cast<uint64_t>(b._ns));
            if ((a._ns >= 0) == (b._ns >= 0) && (r >= 0) != (a._ns >= 0)) {
                return a._ns >= 0 ? _max() : _min();
            }
            return _raw(r);
        }

        friend constexpr duration operator-(duration a, duration b) noexcept {
            int64_t r = static_cast<int64_t>(static_cast<uint64_t>(a._ns) - static_cast<uint64_t>(b._ns));
            if ((a._ns >= 0) != (b._ns >= 0) && (r >= 0) != (a._ns >= 0)) {
                return a._ns >= 0 ? _max() : _min();
            }
            return _raw(r);
        }

        friend constexpr duration operator-(duration a) noexcept {
            return a._ns == std::numeric_limits<int64_t>::min() ? _max() : _raw(-a._ns);
        }

        friend constexpr duration operator+(duration a) noexcept {
            return a;
        }

        template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        friend constexpr duration operator*(duration d, I n) noexcept {
            return _raw(_scaled(n, d._ns));
        }

        template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        friend constexpr duration operator*(I n, duration d) noexcept {
            return _raw(_scaled(n, d._ns));
        }

        // Toward zero, as an int's division
        template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        friend constexpr duration operator/(duration d, I n) noexcept {
            bool negative = (d._ns < 0) != _negative(n);
            return _signed(_magnitude(d._ns) / _magnitude(n), negative);
        }

        // How many whole times b fits in a, toward zero
        friend constexpr int64_t operator/(duration a, duration b) noexcept {
            if (b._ns == -1 && a._ns == std::numeric_limits<int64_t>::min()) {
                return std::numeric_limits<int64_t>::max();
            }
            return a._ns / b._ns;
        }

        // What is left of a after the whole bs, with the sign of a
        friend constexpr duration operator%(duration a, duration b) noexcept {
            return b._ns == -1 ? duration() : _raw(a._ns % b._ns);
        }

        constexpr duration& operator+=(duration d) noexcept {
            return *this = *this + d;
        }

        constexpr duration& operator-=(duration d) noexcept {
            return *this = *this - d;
        }

        template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        constexpr duration& operator*=(I n) noexcept {
            return *this = *this * n;
        }

        template<std::integral I>
        requires (!std::is_same_v<I, bool>)
        constexpr duration& operator/=(I n) noexcept {
            return *this = *this / n;
        }

        constexpr duration& operator%=(duration d) noexcept {
            return *this = *this % d;
        }

        friend constexpr bool operator==(const duration&, const duration&) noexcept = default;
        friend constexpr std::strong_ordering operator<=>(const duration&, const duration&) noexcept = default;

        // A point of any clock moved by a duration: steady_clock::now() + d,
        // a deadline minus a margin. The point is of the clock's type as
        // chrono makes it (the finer of the two units), and saturated at
        // that type's max() and min() as the duration's arithmetic is:
        // now() + duration::max() is time_point::max(), which the timers
        // of async read as never, where chrono's own + would overflow
        // into the past and fire at once
        template<class Clock, class D>
        friend constexpr auto operator+(const std::chrono::time_point<Clock, D>& t, duration d) {
            return _moved(t, d, false);
        }

        template<class Clock, class D>
        friend constexpr auto operator+(duration d, const std::chrono::time_point<Clock, D>& t) {
            return _moved(t, d, false);
        }

        template<class Clock, class D>
        friend constexpr auto operator-(const std::chrono::time_point<Clock, D>& t, duration d) {
            return _moved(t, d, true);
        }

        // Written as to_string() writes it
        template<class CharT, class Traits>
        friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, duration d) {
            return os << d.to_string();
        }

    private:
        static constexpr int64_t Microsecond = 1000;
        static constexpr int64_t Millisecond = 1000 * Microsecond;
        static constexpr int64_t Second = 1000 * Millisecond;
        static constexpr int64_t Minute = 60 * Second;
        static constexpr int64_t Hour = 60 * Minute;

        static constexpr duration _raw(int64_t ns) noexcept {
            duration d;
            d._ns = ns;
            return d;
        }

        static constexpr duration _max() noexcept {
            return _raw(std::numeric_limits<int64_t>::max());
        }

        static constexpr duration _min() noexcept {
            return _raw(std::numeric_limits<int64_t>::min());
        }

        template<std::integral I>
        static constexpr bool _negative(I v) noexcept {
            if constexpr (std::is_signed_v<I>) {
                return v < 0;
            } else {
                return false;
            }
        }

        // |v| in 64 bits: exact for every value of every integral type up
        // to 64 bits, the smallest int64 included
        template<std::integral I>
        static constexpr uint64_t _magnitude(I v) noexcept {
            if constexpr (std::is_signed_v<I>) {
                return v < 0 ? 0 - static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
            } else {
                return static_cast<uint64_t>(v);
            }
        }

        // A magnitude with a sign as an int64, saturated: up to 2^63 - 1
        // positive, up to 2^63 negative
        static constexpr duration _signed(uint64_t u, bool negative) noexcept {
            constexpr uint64_t Limit = uint64_t(1) << 63;
            if (negative) {
                return u >= Limit ? _min() : _raw(-static_cast<int64_t>(u));
            }
            return u >= Limit ? _max() : _raw(static_cast<int64_t>(u));
        }

        // a * b as an int64, saturated
        template<std::integral A, std::integral B>
        static constexpr int64_t _scaled(A a, B b) noexcept {
            uint64_t ua = _magnitude(a);
            uint64_t ub = _magnitude(b);
            bool negative = _negative(a) != _negative(b);
            if (ua != 0 && ub > std::numeric_limits<uint64_t>::max() / ua) {
                return (negative ? _min() : _max())._ns;
            }
            return _signed(ua * ub, negative)._ns;
        }

        // t + d or t - d in the finer of the two units, saturated at the
        // ends of that unit's 64-bit count; a point of a floating or an
        // unsigned count is moved by chrono's own arithmetic
        template<class Clock, class D>
        static constexpr auto _moved(const std::chrono::time_point<Clock, D>& t, duration d, bool subtract) {
            using Common = std::common_type_t<D, std::chrono::nanoseconds>;
            using Rep = typename Common::rep;
            using Result = std::chrono::time_point<Clock, Common>;
            if constexpr (std::is_integral_v<Rep> && std::is_signed_v<Rep> && sizeof(Rep) == sizeof(int64_t) && std::is_integral_v<typename D::rep>) {
                constexpr auto point_scale = std::ratio_divide<typename D::period, typename Common::period>::num;   // the common unit divides both
                constexpr auto span_scale = std::ratio_divide<std::nano, typename Common::period>::num;
                duration a = _raw(_scaled(t.time_since_epoch().count(), point_scale));
                duration b = _raw(_scaled(d._ns, span_scale));
                return Result(Common(static_cast<Rep>((subtract ? a - b : a + b)._ns)));
            } else {
                return subtract ? Result(t) - Common(std::chrono::nanoseconds(d._ns)) : Result(t) + Common(std::chrono::nanoseconds(d._ns));
            }
        }

        static constexpr int64_t _truncated(long double ns) noexcept {
            if (ns != ns) {
                return 0;
            }
            if (ns >= 0x1p63L) {
                return std::numeric_limits<int64_t>::max();
            }
            if (ns <= -0x1p63L) {
                return std::numeric_limits<int64_t>::min();
            }
            return static_cast<int64_t>(ns);
        }

        constexpr double _in(int64_t unit) const noexcept {
            return static_cast<double>(_ns / unit) + static_cast<double>(_ns % unit) / static_cast<double>(unit);
        }

        int64_t _ns = 0;
    };

    inline constexpr duration nanosecond = std::chrono::nanoseconds(1);
    inline constexpr duration microsecond = std::chrono::microseconds(1);
    inline constexpr duration millisecond = std::chrono::milliseconds(1);
    inline constexpr duration second = std::chrono::seconds(1);
    inline constexpr duration minute = std::chrono::minutes(1);
    inline constexpr duration hour = std::chrono::hours(1);

    namespace detail {
        // The number of nanoseconds in a unit of Go's text, or 0 for a
        // text that is not one
        inline uint64_t duration_unit(const char* s, size_t n) noexcept {
            auto is = [&](const char* unit, size_t length) {
                return n == length && std::char_traits<char>::compare(s, unit, length) == 0;
            };
            if (is("ns", 2)) {
                return 1;
            }
            if (is("us", 2) || is("\xC2\xB5s", 3) || is("\xCE\xBCs", 3)) {   // us, µs (micro sign), μs (Greek mu)
                return 1000;
            }
            if (is("ms", 2)) {
                return 1000000;
            }
            if (is("s", 1)) {
                return 1000000000;
            }
            if (is("m", 1)) {
                return 60000000000;
            }
            if (is("h", 1)) {
                return 3600000000000;
            }
            return 0;
        }

        // Writes u / 10^digits with its fraction of `digits` places, the
        // trailing zeros of the fraction dropped (and the point with them
        // when nothing is left of it); returns the end
        inline char* write_decimal(char* out, uint64_t u, int digits) noexcept {
            uint64_t scale = 1;
            for (int i = 0; i < digits; ++i) {
                scale *= 10;
            }
            uint64_t whole = u / scale;
            uint64_t fraction = u % scale;
            char buffer[20];
            int n = 0;
            do {
                buffer[n++] = static_cast<char>('0' + whole % 10);
                whole /= 10;
            } while (whole != 0);
            while (n != 0) {
                *out++ = buffer[--n];
            }
            if (fraction != 0) {
                while (fraction % 10 == 0) {
                    fraction /= 10;
                    --digits;
                }
                *out++ = '.';
                for (int i = digits - 1; i >= 0; --i) {
                    out[i] = static_cast<char>('0' + fraction % 10);
                    fraction /= 10;
                }
                out += digits;
            }
            return out;
        }
    }

    inline string duration::to_string() const {
        if (_ns == 0) {
            return "0s";
        }
        char text[32];   // the longest, the smallest duration: "-2562047h47m16.854775808s", 25 bytes
        char* out = text;
        if (_ns < 0) {
            *out++ = '-';
        }
        uint64_t u = _magnitude(_ns);
        if (u < 1000) {
            out = detail::write_decimal(out, u, 0);
            *out++ = 'n';
        } else if (u < 1000000) {
            out = detail::write_decimal(out, u, 3);
            *out++ = '\xC2';   // µ, the micro sign, as Go writes it
            *out++ = '\xB5';
        } else if (u < 1000000000) {
            out = detail::write_decimal(out, u, 6);
            *out++ = 'm';
        } else {
            uint64_t seconds = u / 1000000000;
            uint64_t hours = seconds / 3600;
            uint64_t minutes = seconds / 60 % 60;
            if (hours != 0) {
                out = detail::write_decimal(out, hours, 0);
                *out++ = 'h';
            }
            if (hours != 0 || minutes != 0) {
                out = detail::write_decimal(out, minutes, 0);
                *out++ = 'm';
            }
            out = detail::write_decimal(out, (seconds % 60) * 1000000000 + u % 1000000000, 9);
        }
        *out++ = 's';
        return string(text, static_cast<size_t>(out - text));
    }

    inline expected<duration, duration_error> duration::parse(const string& text) {
        constexpr uint64_t Limit = uint64_t(1) << 63;   // the magnitude of the smallest duration
        const char* s = text.data();
        size_t n = text.size();
        size_t i = 0;
        bool negative = false;
        if (i < n && (s[i] == '-' || s[i] == '+')) {
            negative = s[i] == '-';
            ++i;
        }
        if (i == n) {
            return unexpected(duration_error("a number expected", i));
        }
        if (n - i == 1 && s[i] == '0') {
            return duration();
        }
        auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
        uint64_t total = 0;
        while (i < n) {
            size_t start = i;
            uint64_t whole = 0;
            bool digits = false;
            while (i < n && is_digit(s[i])) {
                uint64_t d = static_cast<uint64_t>(s[i] - '0');
                if (whole > (Limit - d) / 10) {
                    return unexpected(duration_error("out of range: a duration is at most about 292 years", start));
                }
                whole = whole * 10 + d;
                digits = true;
                ++i;
            }
            size_t fraction_begin = i;
            size_t fraction_end = i;
            if (i < n && s[i] == '.') {
                fraction_begin = ++i;
                while (i < n && is_digit(s[i])) {
                    ++i;
                }
                fraction_end = i;
            }
            if (!digits && fraction_begin == fraction_end) {
                return unexpected(duration_error("a number expected", start));
            }
            size_t unit_begin = i;
            while (i < n && s[i] != '.' && !is_digit(s[i])) {
                ++i;
            }
            if (unit_begin == i) {
                return unexpected(duration_error("a unit expected: ns, us, ms, s, m or h", i));
            }
            uint64_t unit = detail::duration_unit(s + unit_begin, i - unit_begin);
            if (unit == 0) {
                return unexpected(duration_error("an unknown unit: ns, us, ms, s, m or h expected", unit_begin));
            }
            if (whole > Limit / unit) {
                return unexpected(duration_error("out of range: a duration is at most about 292 years", start));
            }
            // The fraction exactly, from its last digit to its first: each
            // step is the floor of (digit * unit + what came after) / 10,
            // and a floor of a sum with a floor inside is the floor of the
            // exact sum, so the result is 0.ddd... * unit truncated,
            // whatever the number of digits
            uint64_t part = 0;
            for (size_t j = fraction_end; j != fraction_begin; --j) {
                part = (static_cast<uint64_t>(s[j - 1] - '0') * unit + part) / 10;
            }
            uint64_t value = whole * unit + part;   // at most 2^63 + unit: no wrap
            if (value > Limit || total > Limit - value) {
                return unexpected(duration_error("out of range: a duration is at most about 292 years", start));
            }
            total += value;
        }
        if (!negative && total == Limit) {
            return unexpected(duration_error("out of range: a duration is at most about 292 years", 0));
        }
        return _signed(total, negative);
    }
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/rounding.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "currency.h"
#include "detail/cldr.h"
#include "detail/cldr_numbers.h"
#include "locale.h"
#include "plural.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

// Numbers as a locale writes them (LDML Part 3): its digits, its decimal
// separator and grouping ("1 234 567,891" in Polish, "12,34,567.5" in
// Hindi), percent, scientific notation, the compact forms ("1,2 mln",
// "1.2 million") and amounts of a currency ("12,50 zł", "PLN 12.50",
// "(€3.46)"). The data is CLDR's, every locale's, resolved when the tables
// were generated (detail/cldr_numbers.h).
//
// A number is formatted from its decimal digits: an integer exactly, a
// double from its shortest digits (2.675 is 2.675 and rounds to 2.68 at two
// digits, as ICU rounds it, where its binary value would give 2.67), a
// decimal text exactly to 200 significant digits. Rounding is half to even
// unless asked otherwise.
namespace sgcl::txt {
    enum class number_style : uint8_t {
        decimal,            // 1,234.568
        percent,            // 12%: the value times 100
        permille,           // 12‰: the value times 1000
        scientific,         // 1.234E3
        compact,            // 1.2K, 1,2 tys.
        compact_long,       // 1.2 thousand, 1,2 tysiąca
        currency,           // $1,234.57
        accounting,         // ($1,234.57) for a negative amount where the locale writes it so
        currency_compact,   // $1.2K
    };

    enum class currency_display : uint8_t {
        symbol,           // $, US$, zł
        narrow_symbol,    // $
        code,             // USD
        name,             // 12,50 złotego: the currency's name in the number's plural form, from the display
                          // names of the locale (sgcl/txt/names/<locale>.h); the code where they are not included
    };

    enum class sign_display : uint8_t {
        automatic,     // -1, 0, 1
        always,        // -1, +0, +1
        never,         // 1, 0, 1
        except_zero,   // -1, 0, +1
        negative,      // -1, 0, 1, and no minus on a value that rounds to zero
    };

    // How a number is written; every field has the locale's default
    struct number_options {
        number_style style = number_style::decimal;
        int min_integer = 1;           // zeros in front up to this many integer digits
        int min_fraction = -1;         // -1: the style's (0; a currency's digits)
        int max_fraction = -1;         // -1: the style's (3; percent 0; a currency's digits)
        int min_significant = 0;       // with max_significant: significant digits instead of fraction digits
        int max_significant = 0;       // 0: fraction digits
        bool grouping = true;          // the locale's separators of thousands
        sign_display sign = sign_display::automatic;
        sgcl::rounding mode = sgcl::rounding::half_even;   // unnecessary: the digits as they are, none dropped
        txt::currency currency;        // none: the currency of the locale's region
        currency_display display = currency_display::symbol;
        bool cash = false;             // the currency's cash digits and rounding step
    };

    namespace detail::cldr {
        // A number as its decimal digits: d[0..n) the significant digits,
        // the most significant first, no zero at either end; the value is
        // 0.d * 10^point. Zero is n == 0.
        struct Decimal {
            static constexpr int Capacity = 200;
            char d[Capacity];
            int n = 0;
            int point = 0;
            bool negative = false;
            bool infinite = false;
            bool nan = false;
            bool sticky = false;   // digits past the capacity that were not zero

            SGCL_INLINE_HOT bool zero() const noexcept {
                return n == 0;
            }

            void trim() noexcept {
                while (n > 0 && d[n - 1] == '0') {
                    --n;
                }
                int lead = 0;
                while (lead < n && d[lead] == '0') {
                    ++lead;
                }
                if (lead) {
                    for (int k = lead; k < n; ++k) {
                        d[k - lead] = d[k];
                    }
                    n -= lead;
                    point -= lead;
                }
                if (n == 0) {
                    point = 0;
                }
            }
        };

        template<class T>
        inline Decimal decimal_of_integer(T v) noexcept {
            Decimal x;
            uint64_t a;
            if constexpr (std::is_signed_v<T>) {
                x.negative = v < 0;
                a = v < 0 ? uint64_t(0) - uint64_t(v) : uint64_t(v);
            } else {
                a = uint64_t(v);
            }
            char tmp[24];
            int k = 0;
            while (a) {
                tmp[k++] = char('0' + a % 10);
                a /= 10;
            }
            for (int i = 0; i < k; ++i) {
                x.d[i] = tmp[k - 1 - i];
            }
            x.n = k;
            x.point = k;
            x.trim();
            return x;
        }

        inline Decimal decimal_of_double(double v) noexcept {
            Decimal x;
            x.negative = std::signbit(v);
            if (std::isnan(v)) {
                x.nan = true;
                x.negative = false;
                return x;
            }
            if (std::isinf(v)) {
                x.infinite = true;
                return x;
            }
            if (v == 0) {
                return x;
            }
            char text[40];
            auto r = std::to_chars(text, text + sizeof text, std::fabs(v), std::chars_format::scientific);
            // d.ddddde[+-]xx
            const char* p = text;
            const char* end = r.ptr;
            while (p < end && *p != 'e') {
                if (*p != '.') {
                    x.d[x.n++] = *p;
                }
                ++p;
            }
            int exp = 0;
            if (p < end) {
                ++p;
                bool neg = *p == '-';
                if (*p == '-' || *p == '+') {
                    ++p;
                }
                for (; p < end; ++p) {
                    exp = exp * 10 + (*p - '0');
                }
                exp = neg ? -exp : exp;
            }
            x.point = exp + 1;
            x.trim();
            return x;
        }

        // A decimal text: [-+]digits[.digits][e[-+]digits]; false when it
        // is not one
        inline bool decimal_of_text(std::string_view s, Decimal& x) noexcept {
            x = Decimal();
            size_t p = 0;
            if (p < s.size() && (s[p] == '-' || s[p] == '+')) {
                x.negative = s[p] == '-';
                ++p;
            }
            bool any = false, point_seen = false;
            int point = 0;
            char extra = 0;
            for (; p < s.size(); ++p) {
                char c = s[p];
                if (c >= '0' && c <= '9') {
                    any = true;
                    if (x.n == 0 && c == '0') {
                        if (point_seen) {
                            --point;
                        }
                        continue;   // a leading zero
                    }
                    if (x.n < Decimal::Capacity) {
                        x.d[x.n++] = c;
                    } else if (!extra) {
                        extra = c;   // the first digit past the capacity decides the rounding there
                    } else if (c != '0') {
                        x.sticky = true;
                    }
                    if (!point_seen) {
                        ++point;
                    }
                } else if (c == '.' && !point_seen) {
                    point_seen = true;
                } else {
                    break;
                }
            }
            if (!any) {
                return false;
            }
            if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
                ++p;
                bool neg = false;
                if (p < s.size() && (s[p] == '-' || s[p] == '+')) {
                    neg = s[p] == '-';
                    ++p;
                }
                if (p >= s.size()) {
                    return false;
                }
                long exp = 0;
                for (; p < s.size() && s[p] >= '0' && s[p] <= '9'; ++p) {
                    exp = exp < 100000000 ? exp * 10 + (s[p] - '0') : exp;
                }
                point += int(neg ? -exp : exp);
            }
            if (p != s.size()) {
                return false;
            }
            // a hundred thousand digits before or after the point at most:
            // past that the text is not a number anyone writes out, and
            // writing it would be the length of a book
            if (x.n > 0 && (point > 100000 || point < -100000)) {
                return false;
            }
            x.point = point;
            if (extra) {
                // past 200 significant digits: rounded there, half to even
                bool up = extra > '5' || (extra == '5' && (x.sticky || ((x.d[x.n - 1] - '0') & 1)));
                x.sticky = false;
                if (up) {
                    int k = x.n - 1;
                    while (k >= 0 && x.d[k] == '9') {
                        x.d[k] = '0';
                        --k;
                    }
                    if (k < 0) {
                        x.d[0] = '1';
                        x.n = 1;
                        ++x.point;
                    } else {
                        ++x.d[k];
                    }
                }
            }
            x.trim();
            if (x.zero()) {
                x.sticky = false;
            }
            return true;
        }

        // Rounds to keep significant digits (keep <= 0: the rounding
        // position is above the leading digit) by mode; the unit of the
        // last kept digit is 10^(point - keep)
        inline void round_digits(Decimal& x, int keep, rounding mode) noexcept {
            if (x.zero() || keep >= x.n) {
                if (!x.sticky || keep < x.n) {
                    return;
                }
                // only the digits past the capacity are dropped
            }
            int unit = x.point - keep;   // the exponent of the last kept digit's place
            // what is dropped: compared with half a unit
            int cmp;           // -1 below half, 0 exactly half, 1 above
            bool nonzero;
            if (keep < 0) {
                cmp = -1;
                nonzero = true;
            } else if (keep >= x.n) {
                cmp = -1;
                nonzero = x.sticky;
            } else {
                char first = x.d[keep];
                bool rest = x.sticky;
                for (int k = keep + 1; k < x.n && !rest; ++k) {
                    rest = x.d[k] != '0';
                }
                cmp = first > '5' ? 1 : first < '5' ? -1 : (rest ? 1 : 0);
                nonzero = first != '0' || rest;
            }
            if (mode == rounding::unnecessary) {
                return;   // exact: formatting has nothing to report, so nothing is dropped
            }
            bool odd = keep > 0 && keep <= x.n && ((x.d[keep - 1] - '0') & 1);
            bool up = false;
            switch (mode) {
                case rounding::half_even: up = cmp > 0 || (cmp == 0 && odd); break;
                case rounding::half_up: up = cmp >= 0; break;
                case rounding::half_down: up = cmp > 0; break;
                case rounding::up: up = nonzero; break;
                case rounding::down: up = false; break;
                case rounding::ceiling: up = nonzero && !x.negative; break;
                case rounding::floor: up = nonzero && x.negative; break;
                case rounding::unnecessary: break;
            }
            x.sticky = false;
            if (keep <= 0) {
                if (up) {
                    x.d[0] = '1';
                    x.n = 1;
                    x.point = unit + 1;
                } else {
                    x.n = 0;
                    x.point = 0;
                }
                return;
            }
            if (keep < x.n) {
                x.n = keep;
            }
            if (up) {
                int k = x.n - 1;
                while (k >= 0 && x.d[k] == '9') {
                    x.d[k] = '0';
                    --k;
                }
                if (k < 0) {
                    x.d[0] = '1';
                    x.n = 1;
                    ++x.point;
                } else {
                    ++x.d[k];
                }
            }
            x.trim();
        }

        // Rounds to a multiple of step units of 10^-fraction (cash: 5 for
        // 0.05): to the nearest by mode, in units of the step
        inline void round_increment(Decimal& x, int fraction, int step, rounding mode) noexcept {
            if (step <= 1 || x.zero()) {
                round_digits(x, x.point + fraction, mode);
                return;
            }
            // the value in units of 10^-fraction, as a decimal again with
            // the step divided out: x / step rounded, then times step
            Decimal q = x;
            // divide by step (small): long division of the digits
            int needed = x.point + fraction + 4;   // the integer part of the units and a little more
            if (needed > Decimal::Capacity) {
                needed = Decimal::Capacity;
            }
            char out[Decimal::Capacity];
            int rem = 0, m = 0;
            for (int k = 0; k < needed; ++k) {
                int digit = k < x.n ? x.d[k] - '0' : 0;
                int cur = rem * 10 + digit;
                out[m++] = char('0' + cur / step);
                rem = cur % step;
            }
            for (int k = 0; k < m; ++k) {
                q.d[k] = out[k];
            }
            q.n = m;
            q.sticky = rem != 0 || x.n > needed;
            q.trim();
            round_digits(q, q.point + fraction, mode);
            // times step
            if (q.zero()) {
                x = q;
                x.negative = q.negative;
                return;
            }
            char prod[Decimal::Capacity + 4];
            int carry = 0;
            int len = q.n;
            for (int k = len - 1; k >= 0; --k) {
                int v = (q.d[k] - '0') * step + carry;
                prod[k + 2] = char('0' + v % 10);
                carry = v / 10;
            }
            prod[1] = char('0' + carry % 10);
            prod[0] = char('0' + carry / 10);
            x.n = 0;
            for (int k = 0; k < len + 2 && x.n < Decimal::Capacity; ++k) {
                x.d[x.n++] = prod[k];
            }
            x.point = q.point + 2;
            x.sticky = false;
            x.trim();
        }

        // Where a number is written: what fits, and a count of the whole
        struct Sink {
            char* at;
            char* end;
            size_t size = 0;

            SGCL_INLINE_HOT void put(char c) noexcept {
                if (at < end) {
                    *at++ = c;
                }
                ++size;
            }

            SGCL_INLINE_HOT void put(std::string_view s) noexcept {
                size_t room = size_t(end - at);
                size_t n = s.size() < room ? s.size() : room;
                for (size_t i = 0; i < n; ++i) {
                    at[i] = s[i];
                }
                at += n;
                size += s.size();
            }
        };

        SGCL_INLINE_HOT std::string_view number_text(uint32_t i) noexcept {
            return NumberTexts[i];
        }

        enum : int {
            SysDigits, SysDecimal, SysGroup, SysPercent, SysPlus, SysMinus, SysExponential, SysPerMille,
            SysInfinity, SysNaN, SysCurrencyDecimal, SysCurrencyGroup, SysApproximately,
            SysPatDecimal, SysPatPercent, SysPatScientific, SysPatCurrency, SysPatAccounting,
            SysCompactShort, SysCompactLong, SysCompactCurrency,
        };

        enum : int {
            PatPrefix, PatSuffix, PatNegPrefix, PatNegSuffix, PatMinInt, PatMaxInt, PatMinFrac, PatMaxFrac,
            PatGroup1, PatGroup2, PatExponent,
        };
    }

    namespace detail::cldr {
        struct NumberAccess;
    }

    // A locale's way of writing numbers and a choice of options, resolved
    // once: what to format many numbers with. A plain value: it holds
    // pointers into constant tables and lives anywhere.
    class number_format {
    public:
        number_format() noexcept
        : number_format(locale()) {
        }

        explicit number_format(const locale& l, const number_options& o = {}) noexcept
        : _locale(l)
        , _options(o) {
            using namespace detail::cldr;
            _index = locale_index(l);
            uint32_t row = NumberLocale[_index] * NumberLocalesWidth;
            uint32_t system = NumberLocales[row + (l.latin_digits() ? 1 : 0)];
            for (uint32_t k = 0; k < NumberSystemsWidth; ++k) {
                _system[k] = uint16_t(NumberSystems[system * NumberSystemsWidth + k]);
            }
            _min_grouping = uint8_t(NumberLocales[row + 2]);
            int pattern = SysPatDecimal;
            // a currency by its name is written in the decimal pattern, the
            // name after it by the locale's unit pattern
            bool by_name = o.display == currency_display::name && o.style != number_style::currency_compact;
            switch (by_name ? number_style::decimal : o.style) {
                case number_style::percent:
                case number_style::permille: pattern = SysPatPercent; break;
                case number_style::scientific: pattern = SysPatScientific; break;
                case number_style::currency:
                case number_style::currency_compact: pattern = SysPatCurrency; break;
                case number_style::accounting: pattern = SysPatAccounting; break;
                default: break;
            }
            for (uint32_t k = 0; k < NumberPatternsWidth; ++k) {
                _pattern[k] = uint16_t(NumberPatterns[_system[pattern] * NumberPatternsWidth + k]);
            }
            if (_is_currency()) {
                _currency = o.currency ? o.currency : currency::of(l);
                uint16_t info = detail::cldr::CurrencyAccess::info(_currency);
                _currency_digits = uint8_t(o.cash ? (info >> 4) & 15 : info & 15);
                _increment = uint8_t(o.cash ? info >> 8 : 0);
                if (o.display == currency_display::code) {
                    _symbol_spacing = 3;
                } else {
                    CurrencyText t = currency_text(_currency, _index, o.display == currency_display::narrow_symbol);
                    _symbol = t.text;
                    _symbol_spacing = t.spacing;
                }
            }
        }

        txt::locale where() const noexcept {
            return _locale;
        }

        const number_options& options() const noexcept {
            return _options;
        }

        template<class T>
        requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
        string format(T value) const {
            detail::cldr::Decimal x = _decimal(value);
            return _string(x);
        }

        // A number written as decimal text, "-1234.5678" or "1e30", as a
        // math::decimal or a database gives it: exact to 200 significant
        // digits; nullopt when the text is not one, or its point stands
        // more than a hundred thousand digits from its first digit
        optional<string> format(const string& decimal) const {
            detail::cldr::Decimal x;
            if (!detail::cldr::decimal_of_text(decimal.view(), x)) {
                return nullopt;
            }
            return _string(x);
        }

        // The text into memory the caller lends, nothing allocated: what
        // fits is written and the whole size comes back
        template<class T>
        requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
        size_t format_to(const slice<char>& buffer, T value) const noexcept {
            detail::cldr::Decimal x = _decimal(value);
            detail::cldr::Sink out{buffer.data(), buffer.data() + buffer.size()};
            _write(x, out);
            return out.size;
        }

    private:
        template<class T>
        SGCL_INLINE_HOT static detail::cldr::Decimal _decimal(T value) noexcept {
            if constexpr (std::is_integral_v<T>) {
                return detail::cldr::decimal_of_integer(value);
            } else {
                return detail::cldr::decimal_of_double(double(value));
            }
        }

        string _string(detail::cldr::Decimal& x) const {
            char room[160];
            detail::cldr::Sink out{room, room + sizeof room};
            detail::cldr::Decimal copy = x;
            _write(copy, out);
            if (out.size <= sizeof room) {
                return string(std::string_view(room, out.size));
            }
            size_t n = out.size;
            return sgcl::detail::StringAccess::bounded<string>(n, [&](char* chars) {
                detail::cldr::Sink again{chars, chars + n};
                detail::cldr::Decimal second = x;
                _write(second, again);
                return again.size < n ? again.size : n;
            });
        }

        bool _is_currency() const noexcept {
            return _options.style == number_style::currency || _options.style == number_style::accounting
                || _options.style == number_style::currency_compact;
        }

        bool _is_compact() const noexcept {
            return _options.style == number_style::compact || _options.style == number_style::compact_long
                || _options.style == number_style::currency_compact;
        }

        // The fraction digits the options and the style give
        void _fraction_digits(int& min_frac, int& max_frac) const noexcept {
            using namespace detail::cldr;
            if (_is_currency()) {
                min_frac = max_frac = _currency_digits;
            } else if (_options.style == number_style::scientific) {
                min_frac = 0;
                max_frac = 3;
            } else {
                min_frac = _pattern[PatMinFrac];
                max_frac = _pattern[PatMaxFrac];
            }
            if (_options.min_fraction >= 0) {
                min_frac = _options.min_fraction;
                if (max_frac < min_frac) {
                    max_frac = min_frac;
                }
            }
            if (_options.max_fraction >= 0) {
                max_frac = _options.max_fraction;
                if (min_frac > max_frac) {
                    min_frac = max_frac;
                }
            }
            // a thousand fraction digits at most: more is a mistake, and a
            // string of them all zeros
            if (max_frac > 1000) {
                max_frac = 1000;
            }
            if (min_frac > max_frac) {
                min_frac = max_frac;
            }
        }

        // Rounding by the options: significant digits, or fraction digits
        // with the currency's cash step; min_frac comes back for padding
        void _round(detail::cldr::Decimal& x, int& min_frac, int& min_sig) const noexcept {
            min_sig = 0;
            if (_options.max_significant > 0) {
                int max_sig = _options.max_significant;
                min_sig = _options.min_significant > 0 ? std::min(_options.min_significant, max_sig) : 1;
                detail::cldr::round_digits(x, max_sig, _options.mode);
                min_frac = 0;
                return;
            }
            int max_frac;
            _fraction_digits(min_frac, max_frac);
            if (_increment > 1 && max_frac == _currency_digits) {
                detail::cldr::round_increment(x, max_frac, _increment, _options.mode);
            } else {
                detail::cldr::round_digits(x, x.point + max_frac, _options.mode);
            }
        }

        // The compact form: ICU's choice of the power of ten, the default
        // rounding (an integer, two significant digits at least), and the
        // magnitude chosen again when the rounding carried into the next
        // one (999,999 is 1M, not 1000K). Returns the form's text, empty
        // where the magnitude is not compacted.
        std::string_view _compact(detail::cldr::Decimal& x, int& min_frac, int& min_sig) const noexcept {
            using namespace detail::cldr;
            int which = _options.style == number_style::compact ? SysCompactShort
                      : _options.style == number_style::compact_long ? SysCompactLong : SysCompactCurrency;
            uint16_t row[CompactRowsWidth];
            for (uint32_t k = 0; k < CompactRowsWidth; ++k) {
                row[k] = uint16_t(CompactRows[_system[which] * CompactRowsWidth + k]);
            }
            // past the largest magnitude with a form, that form: 10^21 is
            // 1,000,000,000T
            int largest = 19;
            while (largest >= 3 && !(row[largest - 3] >> 12)) {
                --largest;
            }
            auto clamp = [&](int magnitude) {
                return magnitude > largest ? largest : magnitude;
            };
            auto multiplier = [&](int magnitude) {
                if (magnitude < 3 || largest < 3) {
                    return 0;
                }
                uint16_t e = row[clamp(magnitude) - 3];
                int zeros = e >> 12;
                if (!zeros) {
                    return 0;
                }
                return (zeros - 1) - clamp(magnitude);
            };
            bool user_digits = _options.max_significant > 0 || _options.min_fraction >= 0 || _options.max_fraction >= 0;
            auto apply = [&](Decimal& v) {
                if (user_digits) {
                    _round(v, min_frac, min_sig);
                    return;
                }
                min_frac = 0;
                min_sig = 0;
                if (v.point >= 2) {
                    round_digits(v, v.point, _options.mode);
                } else {
                    round_digits(v, 2, _options.mode);
                }
            };
            int magnitude = x.zero() ? 0 : x.point - 1;
            int m = multiplier(magnitude);
            x.point += m;
            apply(x);
            if (!x.zero() && x.point - 1 != magnitude + m) {
                int m2 = multiplier(magnitude + 1);
                if (m2 != m) {
                    x.point += m2 - m;
                    apply(x);
                    m = m2;
                }
            }
            int final_magnitude = (x.zero() ? 0 : x.point - 1) - m;
            if (m == 0 || final_magnitude < 3) {
                return {};
            }
            uint16_t e = row[clamp(final_magnitude) - 3];
            uint32_t forms = e & 0xFFF;
            // the plural form of the number shown, with its exponent
            char digits[Decimal::Capacity + 8];
            size_t n = 0;
            int ip = x.point > 0 ? x.point : 0;
            int from = 0;
            if (ip > 19) {
                // past 10^19 a rule asks only for the last digits (every
                // modulus divides 10^18) and whether the number is large:
                // a 1 and the last eighteen
                digits[n++] = '1';
                from = ip - 18;
            }
            for (int k = from; k < ip; ++k) {
                digits[n++] = k < x.n ? x.d[k] : '0';
            }
            if (n == 0) {
                digits[n++] = '0';
            }
            int fd = _fraction_shown(x, min_frac, min_sig);
            if (fd > 0) {
                digits[n++] = '.';
                for (int k = 0; k < fd && n < sizeof digits - 1; ++k) {
                    int at = x.point + k;
                    digits[n++] = at >= 0 && at < x.n ? x.d[at] : '0';
                }
            }
            // ICU's operands: the number as shown, the exponent beside it
            // (only the rules that ask for e see the million): "1 milion",
            // not the form of 1000000
            PluralOperands operands = operands_of(std::string_view(digits, n));
            operands.e = uint32_t(-m);
            plural category = detail::cldr::cardinal(operands, _locale);
            uint32_t form = sparse(CompactForms, CompactFormsStart, forms, uint32_t(category));
            if (!form) {
                form = sparse(CompactForms, CompactFormsStart, forms, 5);
            }
            return number_text(form);
        }

        // The fraction digits written: the number's own, padded to the
        // least fraction digits, and to the least significant ones (1.20
        // for three), which run from the leading digit (the zero of a
        // zero) to the last fraction digit
        static int _fraction_shown(const detail::cldr::Decimal& x, int min_frac, int min_sig) noexcept {
            int frac = x.n - x.point;
            if (frac < 0) {
                frac = 0;
            }
            if (frac < min_frac) {
                frac = min_frac;
            }
            if (min_sig > 0) {
                int shown = (x.zero() ? 1 : x.point) + frac;
                if (shown < min_sig) {
                    frac += min_sig - shown;
                }
            }
            return frac;
        }

        void _write_digits(detail::cldr::Sink& out, const detail::cldr::Decimal& x, int min_int, int min_frac,
                           int min_sig, bool grouping, std::string_view decimal, std::string_view group) const noexcept {
            using namespace detail::cldr;
            std::string_view digits = number_text(_system[SysDigits]);
            bool ascii = digits.size() == 10;
            auto digit = [&](int v) {
                if (ascii) {
                    out.put(char('0' + v));
                    return;
                }
                // ten code points of one length each (CLDR's numeric systems)
                size_t w = digits.size() / 10;
                out.put(digits.substr(size_t(v) * w, w));
            };
            int integer = x.point > 0 && !x.zero() ? x.point : 0;
            if (integer < min_int) {
                integer = min_int;
            }
            int frac = _fraction_shown(x, min_frac, min_sig);
            int g1 = _pattern[PatGroup1], g2 = _pattern[PatGroup2];
            // the compact forms group as ICU does there, from two digits
            // before the first separator (1234, 12 345)
            int min_grouping = _min_grouping > 0 ? _min_grouping : 1;
            if (_is_compact() && min_grouping < 2) {
                min_grouping = 2;
            }
            bool group_now = grouping && g1 > 0 && integer >= g1 + min_grouping;
            for (int k = 0; k < integer; ++k) {
                int place = integer - 1 - k;   // digits to the right of this one in the integer part
                int at = x.point - 1 - place;  // its index in d
                digit(at >= 0 && at < x.n ? x.d[at] - '0' : 0);
                if (group_now && place > 0) {
                    if (place == g1 || (place > g1 && g2 > 0 && (place - g1) % g2 == 0)) {
                        out.put(group);
                    }
                }
            }
            if (frac > 0) {
                out.put(decimal);
                for (int k = 0; k < frac; ++k) {
                    int at = x.point + k;
                    digit(at >= 0 && at < x.n ? x.d[at] - '0' : 0);
                }
            }
        }

        void _write_affix(detail::cldr::Sink& out, std::string_view affix, bool prefix) const noexcept {
            using namespace detail::cldr;
            for (size_t i = 0; i < affix.size(); ++i) {
                char c = affix[i];
                switch (c) {
                    case '\x01': out.put(number_text(_system[SysMinus])); break;
                    case '\x02': out.put(number_text(_system[SysPlus])); break;
                    case '\x03':
                        out.put(number_text(_system[_options.style == number_style::permille ? SysPerMille : SysPercent]));
                        break;
                    case '\x04': out.put(number_text(_system[SysPerMille])); break;
                    case '\x05': {
                        bool facing_after = prefix && i + 1 == affix.size();
                        bool facing_before = !prefix && i == 0;
                        if (facing_before && (_symbol_spacing & 2)) {
                            out.put("\xC2\xA0");
                        }
                        if (_options.display == currency_display::code || _symbol.empty()) {
                            char code[3];
                            out.put(std::string_view(code, CurrencyAccess::text(_currency, code)));
                        } else {
                            out.put(_symbol);
                        }
                        if (facing_after && (_symbol_spacing & 1)) {
                            out.put("\xC2\xA0");
                        }
                        break;
                    }
                    default: out.put(c); break;
                }
            }
        }

        void _write(detail::cldr::Decimal& x, detail::cldr::Sink& out) const noexcept {
            if (_options.display == currency_display::name && _is_currency()
                && _options.style != number_style::currency_compact) {
                _write_named(x, out);
                return;
            }
            _write_number(x, out);
        }

        // The amount and the currency's name in its plural form, by the
        // locale's unit pattern ("{0} {1}"): the number in the decimal
        // pattern with the currency's digits, its form from the digits shown
        void _write_named(detail::cldr::Decimal& x, detail::cldr::Sink& out) const noexcept {
            using namespace detail::cldr;
            char room[512];
            Sink inner{room, room + sizeof room};
            Decimal copy = x;
            _write_number(copy, inner);
            std::string_view number(room, inner.size < sizeof room ? inner.size : sizeof room);
            // the plural form of the digits as written
            int min_frac = 0, min_sig = 0;
            Decimal shown = x;
            _round(shown, min_frac, min_sig);
            char digits[Decimal::Capacity + 8];
            size_t n = 0;
            int ip = shown.point > 0 && !shown.zero() ? shown.point : 0;
            int from = ip > 19 ? ip - 18 : 0;
            if (from) {
                digits[n++] = '1';
            }
            for (int k = from; k < ip; ++k) {
                digits[n++] = k < shown.n ? shown.d[k] : '0';
            }
            if (n == 0) {
                digits[n++] = '0';
            }
            int fd = _fraction_shown(shown, min_frac, min_sig);
            if (fd > 0) {
                digits[n++] = '.';
                for (int k = 0; k < fd && n < sizeof digits - 1; ++k) {
                    int at = shown.point + k;
                    digits[n++] = at >= 0 && at < shown.n ? shown.d[at] : '0';
                }
            }
            uint32_t category = uint32_t(cardinal(operands_of(std::string_view(digits, n)), _locale));
            auto chain = detail::names::chain_of(_locale);
            uint16_t code = CurrencyAccess::packed(_currency);
            std::string_view name, pattern;
            for (uint32_t f : {1 + category, 6u, 0u}) {
                name = detail::names::first_of(chain, [&](const detail::names::Table& t) { return detail::names::currency_name(t, code, f); });
                if (!name.empty()) {
                    break;
                }
            }
            for (uint32_t f : {detail::names::FmtUnit + category, detail::names::FmtUnit + 5u}) {
                pattern = detail::names::first_of(chain, [&](const detail::names::Table& t) { return detail::names::format(t, f); });
                if (!pattern.empty()) {
                    break;
                }
            }
            if (pattern.empty()) {
                pattern = "{0} {1}";
            }
            char code_text[3];
            if (name.empty()) {
                name = std::string_view(code_text, CurrencyAccess::text(_currency, code_text));
            }
            for (size_t i = 0; i < pattern.size();) {
                if (pattern.substr(i, 3) == "{0}") {
                    out.put(number);
                    i += 3;
                } else if (pattern.substr(i, 3) == "{1}") {
                    out.put(name);
                    i += 3;
                } else {
                    out.put(pattern[i]);
                    ++i;
                }
            }
        }

        void _write_number(detail::cldr::Decimal& x, detail::cldr::Sink& out) const noexcept {
            using namespace detail::cldr;
            std::string_view pp = number_text(_pattern[PatPrefix]);
            std::string_view ps = number_text(_pattern[PatSuffix]);
            std::string_view np = number_text(_pattern[PatNegPrefix]);
            std::string_view ns = number_text(_pattern[PatNegSuffix]);
            bool negative = x.negative;
            int min_frac = 0, min_sig = 0;
            int exponent = 0;
            bool scientific = _options.style == number_style::scientific;
            if (x.nan) {
                _write_affix(out, pp, true);
                out.put(number_text(_system[SysNaN]));
                _write_affix(out, ps, false);
                return;
            }
            if (x.zero()) {
                x.point = 0;
            } else if (_options.style == number_style::percent) {
                x.point += 2;
            } else if (_options.style == number_style::permille) {
                x.point += 3;
            }
            std::string_view cpre, csuf;
            bool compacted = false;
            bool own_sign = false;
            bool own_plus = false;
            if (!x.infinite) {
                if (_is_compact()) {
                    std::string_view form = _compact(x, min_frac, min_sig);
                    if (!form.empty()) {
                        compacted = true;
                        size_t neg = form.find('\x08');
                        if (neg != std::string_view::npos) {
                            // a negative form of its own carries the sign
                            bool zero = x.zero();
                            bool minus = x.negative && (_options.sign == sign_display::automatic
                                         || _options.sign == sign_display::always
                                         || (!zero && _options.sign != sign_display::never));
                            bool plus = !x.negative && (_options.sign == sign_display::always
                                        || (_options.sign == sign_display::except_zero && !zero));
                            if (minus || plus) {
                                own_sign = true;
                                own_plus = plus;
                                form = form.substr(neg + 1);
                            } else {
                                form = form.substr(0, neg);
                            }
                        }
                        size_t mark = form.find('\x07');
                        if (mark == std::string_view::npos) {
                            // a word that stands for the number: Italian mille
                            cpre = form;
                            csuf = std::string_view();
                        } else {
                            cpre = form.substr(0, mark);
                            csuf = form.substr(mark + 1);
                            if (csuf.data() == nullptr) {
                                csuf = std::string_view("", 0);
                            }
                        }
                    }
                } else if (scientific) {
                    int max_int = _pattern[PatMaxInt] > 0 ? _pattern[PatMaxInt] : 1;
                    int min_int = _pattern[PatMinInt];
                    if (!x.zero()) {
                        exponent = x.point - 1;
                        int lead = 1;
                        if (max_int > 1 && max_int > min_int) {
                            // engineering: the exponent a multiple of max_int
                            int r = ((exponent % max_int) + max_int) % max_int;
                            exponent -= r;
                            lead = r + 1;
                        }
                        x.point = lead;
                        _round(x, min_frac, min_sig);
                        if (x.point > lead) {
                            // 9.9995 rounded is 10.000: the exponent moves on
                            bool engineering = max_int > 1 && max_int > min_int;
                            if (!engineering) {
                                exponent += 1;
                                x.point = 1;
                            } else if (x.point > max_int) {
                                exponent += max_int;
                                x.point -= max_int;
                            }
                        }
                    } else {
                        _round(x, min_frac, min_sig);
                    }
                } else {
                    _round(x, min_frac, min_sig);
                }
            }
            bool is_zero = x.zero() && !x.infinite;
            // the sign by the option
            bool show_minus = negative;
            bool show_plus = false;
            switch (_options.sign) {
                case sign_display::automatic: break;
                case sign_display::always: show_plus = !negative; break;
                case sign_display::never: show_minus = false; break;
                case sign_display::except_zero:
                    show_minus = negative && !is_zero;
                    show_plus = !negative && !is_zero;
                    break;
                case sign_display::negative: show_minus = negative && !is_zero; break;
            }
            std::string_view pre = pp, suf = ps;
            char plus_pre[128], plus_suf[128];
            if (show_minus) {
                pre = np;
                suf = ns;
            } else if (show_plus) {
                // the negative pattern with its minus turned to a plus; a
                // pattern without one (parentheses) gets a plus in front
                if (np.find('\x01') != std::string_view::npos || ns.find('\x01') != std::string_view::npos) {
                    size_t a = 0, b = 0;
                    for (char c : np) {
                        if (a < sizeof plus_pre) {
                            plus_pre[a++] = c == '\x01' ? '\x02' : c;
                        }
                    }
                    for (char c : ns) {
                        if (b < sizeof plus_suf) {
                            plus_suf[b++] = c == '\x01' ? '\x02' : c;
                        }
                    }
                    pre = std::string_view(plus_pre, a);
                    suf = std::string_view(plus_suf, b);
                } else {
                    size_t a = 0;
                    plus_pre[a++] = '\x02';
                    for (char c : pp) {
                        if (a < sizeof plus_pre) {
                            plus_pre[a++] = c;
                        }
                    }
                    pre = std::string_view(plus_pre, a);
                }
            }
            if (compacted) {
                // the compact form's affixes, the sign in front: -1.2K, -$1.2M
                char cp[260];
                size_t a = 0;
                if ((show_minus || show_plus) && !own_sign) {
                    cp[a++] = show_minus ? '\x01' : '\x02';
                }
                bool word = cpre.data() && csuf.data() == nullptr;
                for (char c : cpre) {
                    if (a < sizeof cp) {
                        cp[a++] = own_plus && c == '\x01' ? '\x02' : c;
                    }
                }
                _write_affix(out, std::string_view(cp, a), !word);
                if (!word) {
                    _write_digits(out, x, _options.min_integer, min_frac, min_sig, _options.grouping,
                                  number_text(_system[SysDecimal]), number_text(_system[SysGroup]));
                    char cs[260];
                    size_t b = 0;
                    for (char c : csuf) {
                        if (b < sizeof cs) {
                            cs[b++] = own_plus && c == '\x01' ? '\x02' : c;
                        }
                    }
                    _write_affix(out, std::string_view(cs, b), false);
                }
                return;
            }
            _write_affix(out, pre, true);
            if (x.infinite) {
                out.put(number_text(_system[SysInfinity]));
            } else {
                bool cur = _is_currency();
                std::string_view decimal = number_text(_system[cur && _system[SysCurrencyDecimal] ? SysCurrencyDecimal : SysDecimal]);
                std::string_view group = number_text(_system[cur && _system[SysCurrencyGroup] ? SysCurrencyGroup : SysGroup]);
                _write_digits(out, x, scientific ? 1 : _options.min_integer, min_frac, min_sig,
                              _options.grouping && !scientific, decimal, group);
                if (scientific) {
                    out.put(number_text(_system[SysExponential]));
                    int e = exponent;
                    if (e < 0) {
                        out.put(number_text(_system[SysMinus]));
                        e = -e;
                    } else if (_pattern[PatExponent] & 16) {
                        out.put(number_text(_system[SysPlus]));
                    }
                    char tmp[12];
                    int k = 0;
                    do {
                        tmp[k++] = char('0' + e % 10);
                        e /= 10;
                    } while (e);
                    int min_exp = _pattern[PatExponent] & 15;
                    detail::cldr::Decimal ed;
                    for (int i = k; i < min_exp; ++i) {
                        tmp[k++] = '0';
                    }
                    for (int i = 0; i < k; ++i) {
                        ed.d[i] = tmp[k - 1 - i];
                    }
                    ed.n = k;
                    ed.point = k;
                    _write_digits(out, ed, k, 0, 0, false, {}, {});
                }
            }
            _write_affix(out, suf, false);
        }

        txt::locale _locale;
        number_options _options;
        uint16_t _system[detail::cldr::NumberSystemsWidth] = {};
        uint16_t _pattern[detail::cldr::NumberPatternsWidth] = {};
        std::string_view _symbol;
        txt::currency _currency;
        uint16_t _index = 0;
        uint8_t _min_grouping = 1;
        uint8_t _currency_digits = 2;
        uint8_t _increment = 0;
        uint8_t _symbol_spacing = 3;

        friend struct detail::cldr::NumberAccess;
    };

    namespace detail::cldr {
        // What message_format needs of a format beyond its public face: a
        // number already in decimal digits appended to a buffer, and the
        // locale's symbols for the affixes of a pattern of its own
        struct NumberAccess {
            static void append(const number_format& f, const Decimal& x, std::string& out) {
                char room[160];
                Sink s{room, room + sizeof room};
                Decimal copy = x;
                f._write(copy, s);
                if (s.size <= sizeof room) {
                    out.append(room, s.size);
                    return;
                }
                size_t at = out.size(), n = s.size;
                out.resize(at + n);
                Sink again{out.data() + at, out.data() + at + n};
                Decimal second = x;
                f._write(second, again);
                out.resize(at + (again.size < n ? again.size : n));
            }

            // SysMinus, SysPlus, SysPercent, SysPerMille
            SGCL_INLINE_HOT static std::string_view symbol(const number_format& f, int which) noexcept {
                return number_text(f._system[which]);
            }

            // the symbol of a currency format (its code where the symbol is
            // the code), the no-break space CLDR's currencySpacing puts
            // between it and a digit it faces with a letter
            static void currency_symbol(const number_format& f, std::string& out, bool facing_after,
                                        bool facing_before) {
                if (facing_before && (f._symbol_spacing & 2)) {
                    out.append("\xC2\xA0");
                }
                if (f._options.display == currency_display::code || f._symbol.empty()) {
                    char code[3];
                    out.append(code, CurrencyAccess::text(f._currency, code));
                } else {
                    out.append(f._symbol);
                }
                if (facing_after && (f._symbol_spacing & 1)) {
                    out.append("\xC2\xA0");
                }
            }

            SGCL_INLINE_HOT static int currency_digits(const number_format& f) noexcept {
                return f._currency_digits;
            }

            // the group sizes of a pattern of its own (#,##,##0: 3 then 2),
            // grouping from the first separator a pattern places
            SGCL_INLINE_HOT static void set_grouping(number_format& f, int first, int rest) noexcept {
                f._pattern[PatGroup1] = uint16_t(first);
                f._pattern[PatGroup2] = uint16_t(rest);
                f._min_grouping = 1;
            }
        };
    }

    // A number in a locale, the one-line form: format_number(1234.5,
    // txt::locale("pl")) is "1234,5"
    template<class T>
    requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
    string format_number(T value, const locale& l = {}, const number_options& o = {}) {
        return number_format(l, o).format(value);
    }

    // An amount of a currency: format_currency(12.5, txt::currency("PLN"),
    // txt::locale("pl")) is "12,50 zł"
    template<class T>
    requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
    string format_currency(T amount, const currency& c, const locale& l = {},
                           currency_display d = currency_display::symbol) {
        number_options o;
        o.style = number_style::currency;
        o.currency = c;
        o.display = d;
        return number_format(l, o).format(amount);
    }
}

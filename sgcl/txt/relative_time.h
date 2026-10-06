//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/duration.h"
#include "../core/string.h"
#include "detail/cldr.h"
#include "detail/cldr_relative.h"
#include "locale.h"
#include "number.h"
#include "plural.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <string_view>

// A time relative to now as a locale says it (LDML Part 4, relative time
// fields): "za 3 dni", "3 hours ago", "yesterday", "now". The number is
// written by the locale's decimal format and the phrase chosen by its
// plural form.
namespace sgcl::txt {
    enum class time_unit : uint8_t {
        second,
        minute,
        hour,
        day,
        week,
        month,
        quarter,
        year,
    };

    struct relative_options {
        txt::width width = txt::width::wide;
        bool numeric = false;   // false: "yesterday", "tomorrow", "now" where the language has a word for it
    };

    namespace detail::cldr {
        // The phrase of a value: a word for -2..2 where the language has
        // one and words are wanted (ICU's rule, with its 1% margin: -0.995
        // is "yesterday"), else the plural pattern of the future or the
        // past around the number
        inline string relative(double value, time_unit unit, const locale& l, const relative_options& o) {
            uint16_t index = locale_index(l);
            uint32_t block = RelativeLocales[RelativeLocale[index] * RelativeLocalesWidth
                                             + uint32_t(o.width) * 8 + uint32_t(unit)];
            auto slot = [&](uint32_t k) {
                return RelativeTexts[sparse(RelativeBlocks, RelativeBlocksStart, block, k)];
            };
            if (!o.numeric && value > -2.1 && value < 2.1 && !std::isnan(value)) {
                double x100 = value * 100.0;
                int k = int(x100 < 0 ? x100 - 0.5 : x100 + 0.5);
                if (k % 100 == 0) {
                    std::string_view word = slot(uint32_t(k / 100 + 2));
                    if (!word.empty()) {
                        return string(word);
                    }
                }
            }
            bool past = value < 0 || (value == 0 && std::signbit(value));
            double magnitude = std::fabs(value);
            number_format digits(l);
            char room[96];
            size_t written;
            plural category;
            if (magnitude < 1e15 && magnitude == std::floor(magnitude)) {
                // a whole number, as it nearly always is: its form from the
                // integer
                int64_t whole = int64_t(magnitude);
                written = digits.format_to(slice<char>(room, sizeof room), whole);
                category = plural_of(whole, l);
            } else {
                // the form of the number as written, three fraction digits
                // at most (1.5 is not 1)
                written = digits.format_to(slice<char>(room, sizeof room), magnitude);
                number_options plain;
                plain.grouping = false;
                category = plural_of(number_format(locale(), plain).format(magnitude), l);
            }
            string long_number;
            std::string_view n(room, written < sizeof room ? written : sizeof room);
            if (written > sizeof room) {
                long_number = digits.format(magnitude);   // 1e300 days: written again whole
                n = long_number.view();
            }
            uint32_t base = 5 + (past ? 6 : 0);
            std::string_view pattern = slot(base + uint32_t(category));
            if (pattern.empty()) {
                pattern = slot(base + 5);
            }
            size_t at = pattern.find("{0}");
            if (at == std::string_view::npos) {
                return string(pattern);
            }
            size_t total = pattern.size() - 3 + n.size();
            return sgcl::detail::StringAccess::bounded<string>(total, [&](char* out) {
                size_t k = 0;
                for (size_t i = 0; i < at; ++i) {
                    out[k++] = pattern[i];
                }
                for (char c : n) {
                    out[k++] = c;
                }
                for (size_t i = at + 3; i < pattern.size(); ++i) {
                    out[k++] = pattern[i];
                }
                return k;
            });
        }
    }

    // A value of a unit relative to now: format_relative(-1, time_unit::day,
    // txt::locale("pl")) is "wczoraj", numeric "1 dzień temu"; 3 days is
    // "za 3 dni"; a negative value is the past
    inline string format_relative(double value, time_unit unit, const locale& l = {}, const relative_options& o = {}) {
        return detail::cldr::relative(value, unit, l, o);
    }

    // A span of time relative to now in the unit that fits it, rounded to
    // a whole number of it: under a minute seconds, under an hour minutes,
    // under a day hours, under a week days, under 30 days weeks, under 365
    // days months (of 30.44 days), else years (of 365.25). A negative span
    // is the past: -90 minutes is "2 hours ago".
    inline string format_relative(const duration& d, const locale& l = {}, const relative_options& o = {}) {
        double ns = double(std::chrono::nanoseconds(d).count());
        double a = std::fabs(ns);
        constexpr double S = 1e9, M = 60 * S, H = 60 * M, D = 24 * H;
        time_unit unit;
        double per;
        if (a < M) {
            unit = time_unit::second, per = S;
        } else if (a < H) {
            unit = time_unit::minute, per = M;
        } else if (a < D) {
            unit = time_unit::hour, per = H;
        } else if (a < 7 * D) {
            unit = time_unit::day, per = D;
        } else if (a < 30 * D) {
            unit = time_unit::week, per = 7 * D;
        } else if (a < 365 * D) {
            unit = time_unit::month, per = 30.436875 * D;
        } else {
            unit = time_unit::year, per = 365.2425 * D;
        }
        double v = std::round(a / per);
        return detail::cldr::relative(ns < 0 ? -v : v, unit, l, o);
    }
}

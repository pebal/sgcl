//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The texts and the zones of the time module on any bytes, without an
// oracle. The first byte picks the path; a pattern and a text are split at
// a NUL. What must hold:
//   - what a layout (rfc3339, http, email, iso8601) reads is an instant
//     inside the years 1677 to 2262 whose rfc3339_nano text reads back to
//     the same instant and offset; the layout's own text reads back to the
//     instant to the layout's precision, and writing is stable from then on
//     (format(parse(x)) is the canonical x);
//   - a pattern of '%' reads either an error with its byte inside the
//     text, or an instant that round-trips through rfc3339_nano; a date
//     read by its own parse writes back as a date that reads to itself;
//     any pattern writes without a crash;
//   - a zone from any bytes (from_tzif) or any TZ string (from_posix) is
//     an error or a zone whose offsets stay within 25 hours, whose
//     transitions after an instant come after it and before it before it,
//     and whose walk of transitions ascends and ends; a wall time made in
//     it (date::at) lands within three days of its date: a time a change
//     skipped moves on by the change, which between offsets of up to 25
//     hours either way is up to 50 hours (NZST-24NZDT5 skips 29).
// Built with libFuzzer (tests/fuzz/run.sh tests/time/fuzz/time_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/time/time.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    const time::layout layouts[] = {time::rfc3339, time::http, time::email, time::iso8601, time::rfc3339_nano};

    // The instant read back from its RFC 3339 text: the same, with its offset
    void exact(const time::datetime& t) {
        check(t.year() >= 1677 && t.year() <= 2262);
        string nano = t.format(time::rfc3339_nano);
        auto back = time::datetime::parse(nano, time::rfc3339);
        check(back.has_value());
        check(back->unix_nano() == t.unix_nano());
        check(back->offset() == t.offset());
    }

    void layout_text(uint8_t which, std::string_view in) {
        auto l = layouts[which % 5];
        auto t = time::datetime::parse(string(in), l);
        if (!t) {
            check(t.error().offset() <= in.size());
            return;
        }
        exact(*t);
        // the layout's own text: the instant to its precision, then stable
        string once = t->format(l);
        auto again = time::datetime::parse(once, l);
        check(again.has_value());
        bool seconds_only = l != time::rfc3339_nano && l != time::iso8601;
        int64_t want = t->unix_nano();
        if (seconds_only) {
            want -= ((want % 1000000000) + 1000000000) % 1000000000;
        }
        check(again->unix_nano() == want);
        check(again->format(l) == once);
    }

    void pattern_text(std::string_view pattern, std::string_view text, int32_t offset_minutes) {
        string p(pattern);
        auto z = time::zone::fixed(duration(std::chrono::minutes(offset_minutes % (24 * 60))));
        auto t = time::datetime::parse(string(text), p, z);
        if (t) {
            exact(*t);
            (void)t->format(p);
        } else {
            check(t.error().offset() <= text.size());
        }
        auto d = time::date::parse(string(text), p);
        if (d) {
            (void)d->format(p);
            auto back = time::date::parse(d->to_string());   // ISO 8601 reads a year of five digits, %Y four
            check(back.has_value() && *back == *d);
        }
        auto plain = time::date::parse(string(text));
        if (plain) {
            auto back = time::date::parse(plain->format("%Y-%m-%d"));
            check(back.has_value() && *back == *plain);
        }
        // any pattern writes, for an instant of the input
        int64_t ns = 0;
        std::memcpy(&ns, text.data(), std::min(sizeof(ns), text.size()));
        (void)time::datetime::from_unix_nano(ns, z).format(p);
    }

    void zone_walk(const time::zone& z, std::string_view instants) {
        // a POSIX offset reaches 24 hours, and a DST without its own an hour
        // more (tzcode and Go take both): 25 hours at the most
        const int64_t day = 26 * 3600;
        int64_t starts[] = {-2208988800LL, 0, 1790208000LL, 4102444800LL, -9223372036LL, 9223372035LL};
        for (int64_t s : starts) {
            auto t = time::datetime::from_unix(s, z);
            auto off = z.offset_at(t);
            check(off > duration(std::chrono::seconds(-day)) && off < duration(std::chrono::seconds(day)));
            (void)z.abbreviation_at(t);
            (void)z.is_dst_at(t);
            check(t.in(time::zone::utc()).unix() == s);
            if (auto n = z.next_transition(t)) {
                check(n->unix_nano() > t.unix_nano());
            }
            if (auto p = z.previous_transition(t)) {
                check(p->unix_nano() < t.unix_nano());
            }
        }
        // a walk of the transitions from 1900 on ascends, and ends by 2262
        auto t = time::datetime::from_unix(-2208988800LL, z);
        for (int steps = 0; steps < 3000; ++steps) {
            auto n = z.next_transition(t);
            if (!n) {
                break;
            }
            check(n->unix_nano() > t.unix_nano());
            t = *n;
            auto off = z.offset_at(t);
            check(off > duration(std::chrono::seconds(-day)) && off < duration(std::chrono::seconds(day)));
        }
        // the instants of the input, and a wall time at each date
        for (size_t at = 0; at + 8 <= instants.size() && at < 64; at += 8) {
            int64_t s;
            std::memcpy(&s, instants.data() + at, 8);
            s %= int64_t(9000000000LL);
            auto t2 = time::datetime::from_unix(s, z);
            (void)z.offset_at(t2);
            (void)t2.format(time::rfc3339);
            (void)t2.abbreviation();
            auto d = t2.date();
            auto wall = d.at(int(uint8_t(instants[at]) % 25), int(uint8_t(instants[at + 1]) % 60), z);
            if (wall.year() > 1677 && wall.year() < 2262) {
                auto landed = wall.in(z).date();
                check(landed >= d.add_days(-3) && landed <= d.add_days(3));
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (mode % 4) {
        case 0: layout_text(mode >> 2, rest); break;
        case 1: {
            size_t nul = rest.find('\0');
            std::string_view pattern = rest.substr(0, nul);
            std::string_view text = nul == std::string_view::npos ? std::string_view() : rest.substr(nul + 1);
            pattern_text(pattern, text, int32_t(int8_t(mode)) * 23);
            break;
        }
        case 2: {
            slice<const byte> bytes(reinterpret_cast<const byte*>(rest.data()), rest.size());
            auto z = time::zone::from_tzif(bytes, "fuzz");
            if (z) {
                zone_walk(*z, rest.substr(rest.size() > 64 ? rest.size() - 64 : 0));
            }
            break;
        }
        case 3: {
            size_t nul = rest.find('\0');
            auto z = time::zone::from_posix(string(rest.substr(0, nul)));
            if (z) {
                zone_walk(*z, nul == std::string_view::npos ? std::string_view() : rest.substr(nul + 1));
            }
            break;
        }
    }
    return 0;
}

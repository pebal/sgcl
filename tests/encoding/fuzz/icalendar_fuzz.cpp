//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::icalendar, encoding::vcard and encoding::recurrence on any text.
// The first byte picks the reading: 0 a calendar, 1 an address book, 2 a
// recurrence rule (the next 8 bytes its start, the rest the rule). What must
// hold: a calendar or a card read writes a text that reads back to an equal
// value and writes the same text again; every property's typed readings and
// every component's occurrences over two years are made without a fault, in
// order and in the range; a rule read writes a text that reads back to an
// equal rule, and its instances from a start are in order, start first, at
// most the limit; a text refused is refused at a place within it. Built with
// libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/icalendar_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void touch(const content_line& p) {
        (void)p.text();
        (void)p.list();
        (void)p.components();
        (void)p.as_int();
        (void)p.as_bool();
        (void)p.as_date();
        (void)p.as_datetime();
        (void)p.as_datetime(time::zone::utc());
        (void)p.as_duration();
        (void)p.as_utc_offset();
        (void)p.as_recurrence();
    }

    void walk(const icalendar& root, const icalendar& c, int depth) {
        for (const auto& p : c.properties()) {
            touch(p);
            (void)root.datetime_of(p, time::zone::utc());
        }
        if (depth == 1 && c.property_of("DTSTART")) {
            auto from = time::datetime::from_unix(946684800, time::zone::utc());   // 2000
            auto to = time::datetime::from_unix(1009843200, time::zone::utc());    // 2002
            auto v = root.occurrences(c, from, to, 200, time::zone::utc());
            check(v.size() <= 200);
            for (size_t i = 0; i < v.size(); ++i) {
                check(v[i] >= from && v[i] < to);
                check(i == 0 || v[i - 1] < v[i]);
            }
        }
        for (const auto& child : c.components()) {
            walk(root, child, depth + 1);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 32768) {
        return 0;
    }
    int mode = data[0] % 3;
    if (mode == 2) {
        if (size < 9) {
            return 0;
        }
        int64_t seconds = 0;
        std::memcpy(&seconds, data + 1, 8);
        seconds = seconds % (int64_t(200) * 365 * 86400);   // within 1770 to 2170
        std::string text(reinterpret_cast<const char*>(data + 9), size - 9);
        auto r = recurrence::parse(string(text));
        if (!r) {
            check(r.error().offset() <= text.size());
            return 0;
        }
        auto back = recurrence::parse(r->to_string());
        check(back && *back == *r && back->to_string() == r->to_string());
        auto zone = (data[0] & 8) ? time::zone::load("Europe/Warsaw").value() : time::zone::utc();
        auto start = time::datetime::from_unix(seconds, zone);
        auto to = start + duration(std::chrono::hours(24 * 400));
        auto v = r->occurrences(start, start, to, 300);
        check(v.size() <= 300);
        check(v.empty() || v[0] == start || r->until());
        for (size_t i = 1; i < v.size(); ++i) {
            check(v[i - 1] < v[i] && v[i] < to);
        }
        return 0;
    }
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    if (mode == 0) {
        auto all = icalendar::parse_all(string(text));
        if (!all) {
            check(all.error().offset() <= text.size());
            return 0;
        }
        for (const auto& c : *all) {
            walk(c, c, 0);
            string written;
            try {
                written = c.to_string();
            } catch (const invalid_argument&) {
                check(false);
            }
            auto back = icalendar::parse(written);
            if (!back || !(*back == c)) {
                std::fprintf(stderr, "written:\n%s\n%s\n", written.data(), back ? "differs" : back.error().message().data());
            }
            check(back && *back == c && back->to_string() == written);
        }
        return 0;
    }
    auto all = vcard::parse_all(string(text));
    if (!all) {
        check(all.error().offset() <= text.size());
        return 0;
    }
    for (const auto& c : *all) {
        for (const auto& p : c.properties()) {
            touch(p);
        }
        auto written = c.to_string();
        auto back = vcard::parse(written);
        if (!back || !(*back == c)) {
            std::fprintf(stderr, "written:\n%s\n%s\n", written.data(), back ? "differs" : back.error().message().data());
        }
        // VERSION moved first by the writer
        check(back && back->to_string() == written && back->properties().size() == c.properties().size());
    }
    return 0;
}

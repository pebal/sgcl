//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The time module: what reading the clock, the fields of an instant in a
// zone, a time of the clock made an instant, the text of an instant both
// ways, a duration's text and a zone's loading cost, one case a run
// (benchmarks/go/time has the Go side, the same cases over the same
// instants). Prints one line: ns per call.
//
//   time <case> <sgcl|std>
//
//   now                 time::now() (Go: time.Now(); std: system_clock::now())
//   fields_utc          year, month, day, hour, minute, second of an instant in UTC
//   fields_local        the same in Europe/Warsaw
//   offset_now          the zone's offset at instants of this year
//   offset_random       the zone's offset at instants from 1900 to 2100
//   local_to_instant    date(y, m, d).at(h, mi, s, Europe/Warsaw) (Go: time.Date)
//   format_rfc3339      to a caller's buffer, nothing allocated (Go: AppendFormat;
//                       std: format_to of "{:%FT%T}Z" over sys_seconds)
//   format_http         the same, IMF-fixdate (std: "{:%a, %d %b %Y %T} GMT")
//   format_pattern      "%d.%m.%Y %H:%M" to a buffer (Go: "02.01.2006 15:04")
//   format_string       t.format(time::rfc3339): the string made (Go: Format)
//   parse_rfc3339       datetime::parse(text, rfc3339) (Go: time.Parse(RFC3339))
//   parse_http          datetime::parse(text, http) (Go: http.ParseTime)
//   parse_pattern       datetime::parse(text, "%d.%m.%Y %H:%M") (Go: "02.01.2006 15:04")
//   duration_string     duration::to_string (Go: Duration.String)
//   duration_parse      duration::parse (Go: time.ParseDuration)
//   load_cold           zone::load of every zone of the database, once each
//   load_cached         zone::load of one name again (Go: LoadLocation, which
//                       reads the file every time)
//
// The instants are 4096 drawn once by splitmix64, walked in order; the
// loop runs for about two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/time/time.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <format>
#include <string>
#include <vector>

namespace {
    volatile uint64_t sink;

    uint64_t splitmix(uint64_t& s) {
        s += 0x9e3779b97f4a7c15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    // Instants from `from` to `to` seconds since 1970, with nanoseconds
    std::vector<int64_t> instants(int64_t from, int64_t to) {
        std::vector<int64_t> out(4096);
        uint64_t s = 1;
        for (auto& t : out) {
            t = (from + int64_t(splitmix(s) % uint64_t(to - from))) * 1000000000 + int64_t(splitmix(s) % 1000000000);
        }
        return out;
    }

    template<class F>
    std::pair<uint64_t, double> run_for(F&& f, double seconds) {
        uint64_t calls = 0;
        uint64_t acc = 0;
        auto t0 = bench::Clock::now();
        double wall = 0;
        size_t i = 0;
        do {
            for (int k = 0; k < 1024; ++k) {
                acc += f(i++ & 4095);
            }
            calls += 1024;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }
}

int main(int argc, char** argv) {
    using namespace sgcl;
    if (argc < 3) {
        std::fprintf(stderr, "usage: time <case> <sgcl|std>\n");
        return 2;
    }
    std::string what = argv[1];
    std::string variant = argv[2];
    bool std_ = variant == "std";
    time::zone warsaw = time::zone::load("Europe/Warsaw").value();
    auto recent = instants(1767225600, 1798761600);        // 2026
    auto wide = instants(-2208988800, 4102444800);         // 1900 to 2100
    sgcl::vector<time::datetime> utc_times, local_times;   // a datetime holds its zone's tracked_ptr: a container of the library
    std::vector<std::chrono::sys_seconds> sys_times;
    for (int64_t ns : wide) {
        utc_times.push_back(time::datetime::from_unix_nano(ns, time::zone::utc()));
        local_times.push_back(time::datetime::from_unix_nano(ns, warsaw));
        sys_times.push_back(std::chrono::sys_seconds(std::chrono::seconds(ns / 1000000000)));
    }
    // The texts in a vector of the library: a string holds a tracked
    // pointer, which a std::vector's memory must not
    sgcl::vector<sgcl::string> rfc3339_texts, http_texts, pattern_texts, duration_texts;
    std::vector<time::date> dates;
    std::vector<sgcl::duration> durations;
    for (auto& t : local_times) {
        rfc3339_texts.push_back(t.format(time::rfc3339));
        http_texts.push_back(t.format(time::http));
        pattern_texts.push_back(t.format("%d.%m.%Y %H:%M"));
        dates.push_back(t.date());
        durations.push_back(std::chrono::nanoseconds(t.unix_nano() % 100000000000000));
        duration_texts.push_back(durations.back().to_string());
    }
    char buffer[128];
    auto measure = [&](auto f) {
        run_for(f, 0.25);
        auto [calls, wall] = run_for(f, 2.0);
        std::printf("time %s variant=%s ns/op=%.2f wall=%.2fs\n", what.c_str(), variant.c_str(), wall * 1e9 / double(calls), wall);
    };
    if (what == "now") {
        if (std_) {
            measure([](size_t) { return uint64_t(std::chrono::system_clock::now().time_since_epoch().count()); });
        } else {
            measure([](size_t) { return uint64_t(time::now().unix_nano()); });
        }
    } else if (what == "fields_utc" || what == "fields_local") {
        auto& ts = what == "fields_utc" ? utc_times : local_times;
        if (std_ && what == "fields_utc") {
            measure([&](size_t i) {
                auto days = std::chrono::floor<std::chrono::days>(sys_times[i]);
                std::chrono::year_month_day ymd(days);
                std::chrono::hh_mm_ss hms(sys_times[i] - days);
                return uint64_t(int(ymd.year()) + unsigned(ymd.month()) + unsigned(ymd.day()) + hms.hours().count() + hms.minutes().count() + hms.seconds().count());
            });
        } else {
            measure([&](size_t i) {
                auto& t = ts[i];
                auto d = t.date();
                return uint64_t(d.year() + int(d.month()) + d.day() + t.hour() + t.minute() + t.second());
            });
        }
    } else if (what == "offset_now" || what == "offset_random") {
        sgcl::vector<time::datetime> ts;
        for (int64_t ns : what == "offset_now" ? recent : wide) {
            ts.push_back(time::datetime::from_unix_nano(ns, warsaw));
        }
        measure([&](size_t i) { return uint64_t(warsaw.offset_at(ts[i]).nanoseconds()); });
    } else if (what == "local_to_instant") {
        measure([&](size_t i) { return uint64_t(dates[i].at(int(i % 24), int(i % 60), int(i % 60), warsaw).unix_nano()); });
    } else if (what == "format_rfc3339" || what == "format_http") {
        time::layout l = what == "format_http" ? time::http : time::rfc3339;
        if (std_) {
            if (l == time::http) {
                measure([&](size_t i) { return uint64_t(std::format_to_n(buffer, sizeof buffer, "{:%a, %d %b %Y %T} GMT", sys_times[i]).size); });
            } else {
                measure([&](size_t i) { return uint64_t(std::format_to_n(buffer, sizeof buffer, "{:%FT%T}Z", sys_times[i]).size); });
            }
        } else {
            measure([&](size_t i) {
                txt::format_sink out(buffer, sizeof buffer);
                time::detail::layout_writer::write(out, local_times[i], l);
                return uint64_t(out.size());
            });
        }
    } else if (what == "format_pattern") {
        if (std_) {
            measure([&](size_t i) { return uint64_t(std::format_to_n(buffer, sizeof buffer, "{:%d.%m.%Y %H:%M}", sys_times[i]).size); });
        } else {
            measure([&](size_t i) { return uint64_t(txt::format_to(buffer, "{:%d.%m.%Y %H:%M}", local_times[i])); });
        }
    } else if (what == "format_string") {
        measure([&](size_t i) { return uint64_t(local_times[i].format(time::rfc3339).size()); });
    } else if (what == "parse_rfc3339") {
        measure([&](size_t i) { return uint64_t(time::datetime::parse(rfc3339_texts[i], time::rfc3339)->unix_nano()); });
    } else if (what == "parse_http") {
        measure([&](size_t i) { return uint64_t(time::datetime::parse(http_texts[i], time::http)->unix_nano()); });
    } else if (what == "parse_pattern") {
        measure([&](size_t i) { return uint64_t(time::datetime::parse(pattern_texts[i], "%d.%m.%Y %H:%M")->unix_nano()); });
    } else if (what == "duration_string") {
        measure([&](size_t i) { return uint64_t(durations[i].to_string().size()); });
    } else if (what == "duration_parse") {
        measure([&](size_t i) { return uint64_t(sgcl::duration::parse(duration_texts[i])->nanoseconds()); });
    } else if (what == "load_cold") {
        auto names = time::zone::available();
        auto t0 = bench::Clock::now();
        uint64_t acc = 0;
        for (auto& n : names) {
            acc += uint64_t(time::zone::load(n)->name().size());
        }
        double wall = bench::seconds_since(t0);
        sink = acc;
        std::printf("time %s variant=%s zones=%zu ns/op=%.2f wall=%.2fs\n", what.c_str(), variant.c_str(), names.size(), wall * 1e9 / double(names.size()), wall);
    } else if (what == "load_cached") {
        measure([&](size_t) { return uint64_t(time::zone::load("America/New_York")->name().size()); });
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    return 0;
}

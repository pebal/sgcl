//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the collector's public API, config, clock and duration
// (DESIGN 408): the queries at a null pointer, an address outside the heap,
// one inside an object and one into garbage; the settings at 0 and at
// SIZE_MAX; a stepper asked for the gate it stands at; the constants the
// config page lists; durations at the ends of their range.
#include "tests/types.h"

#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>

namespace {
    struct Pair {
        int first = 1;
        tracked_ptr<Int> second;
    };

    // A managed object nothing holds once the call returns: its address,
    // hidden from the stack scan
    SGCL_NOINLINE uintptr_t garbage() {
        tracked_ptr<Pair> p = make_tracked<Pair>();
        return hide(p.get());
    }
}

// collector

TEST(CollectorBoundaries_Tests, QueriesAtNullAndOutsideTheHeap) {
    int on_stack = 0;
    auto on_heap = std::make_unique<int>(0);
    for (const void* p : {(const void*)nullptr, (const void*)&on_stack, (const void*)on_heap.get(), (const void*)uintptr_t(1),
                          (const void*)UINTPTR_MAX}) {
        {
            auto [guard, rs] = collector::get_referrers(p);
            EXPECT_TRUE(rs.empty());
        }
        {
            auto [guard, path] = collector::get_path_to_root(p);
            EXPECT_TRUE(path.empty());
        }
        auto r = collector::get_retained(p);
        EXPECT_EQ(r.objects, 0u);
        EXPECT_EQ(r.bytes, 0u);
        std::ostringstream out;
        collector::explain(p, out);
        EXPECT_NE(out.str().find("not a live managed object"), std::string::npos) << out.str();
    }
}

TEST(CollectorBoundaries_Tests, QueriesIntoAnObjectAndIntoGarbage) {
    root_ptr<Pair> held = make_tracked<Pair>();
    off_frame([&] { held->second = make_tracked<Int>(2); });
    collector::clear_stack();
    auto whole = collector::get_retained(held.get());
    auto inside = collector::get_retained(&held->first);   // a pointer into the object: the same object
    EXPECT_EQ(whole.objects, inside.objects);
    EXPECT_EQ(whole.bytes, inside.bytes);
    EXPECT_EQ(whole.objects, 2u);
    {
        auto [guard, path] = collector::get_path_to_root(&held->second);
        EXPECT_FALSE(path.empty());
        EXPECT_EQ(path.back().from, collector::referrer::kind::unique);   // the block of cells, a root by its state
    }
    auto dead = garbage();
    collector::clear_stack();
    collector::force_collect(true);
    auto r = collector::get_retained(unhide(dead));
    EXPECT_EQ(r.objects, 0u);
    std::ostringstream out;
    collector::explain(unhide(dead), out);
    EXPECT_NE(out.str().find("not a live managed object"), std::string::npos) << out.str();
}

TEST(CollectorBoundaries_Tests, ClearStackAtItsEnds) {
    collector::clear_stack(0);
    collector::clear_stack(1);
    collector::clear_stack(SIZE_MAX);   // the whole unused stack, never closer than the guard margin
    collector::clear_stack(config::stack_clear_size);
    EXPECT_TRUE(collector::force_collect(false));
    EXPECT_TRUE(collector::force_collect(true));
}

TEST(CollectorBoundaries_Tests, MemoryLimitAtZeroAndAtTheMaximum) {
    const size_t before = collector::get_memory_limit();
    collector::set_memory_limit(0);   // no ceiling
    EXPECT_EQ(collector::get_memory_limit(), 0u);
    auto p = make_tracked<Int>(1);
    EXPECT_EQ(*p, 1);
    collector::set_memory_limit(SIZE_MAX);
    EXPECT_EQ(collector::get_memory_limit(), SIZE_MAX);
    auto q = make_tracked<Int>(2);
    EXPECT_EQ(*q, 2);
    collector::set_memory_limit_percent(100);
    EXPECT_GE(collector::get_memory_limit(), collector::get_committed_memory());
    collector::set_memory_limit(before);
    EXPECT_EQ(collector::get_memory_limit(), before);
}

TEST(CollectorBoundaries_Tests, StatisticsAfterACycle) {
    collector::force_collect(true);
    auto a = collector::get_statistics();
    collector::force_collect(true);
    auto b = collector::get_statistics();
    EXPECT_GT(b.cycles, a.cycles);
    EXPECT_GE(b.full_cycles, a.full_cycles + 1);   // a forced collection is full
    EXPECT_LE(b.full_cycles, b.cycles);
    EXPECT_LE(b.live_bytes, b.committed_bytes);
    for (double ms : b.phases_ms) {
        EXPECT_GE(ms, 0.0);
    }
    for (auto name : collector::phase_names) {
        EXPECT_NE(name, nullptr);
    }
    EXPECT_EQ(collector::get_live_object_count(), collector::get_live_object_count());
}

TEST(CollectorBoundaries_Tests, StepperAtTheGateItStandsAt) {
    collector::force_collect(true);
    collector::stepper s;
    auto at = s.current();
    auto cycles = collector::get_statistics().cycles;
    EXPECT_EQ(s.advance_to(at), at);   // the next gate of that name: in the next cycle
    EXPECT_EQ(s.current(), at);
    EXPECT_EQ(collector::get_statistics().cycles, cycles + 1);
    s.advance_to(collector::stepper::phase::released);
    cycles = collector::get_statistics().cycles;
    s.finish_cycle();   // standing at released: the whole next cycle
    EXPECT_EQ(s.current(), collector::stepper::phase::released);
    EXPECT_EQ(collector::get_statistics().cycles, cycles + 1);
    s.full(false);
    s.helpers(0);
    s.finish_cycle();
    EXPECT_EQ(collector::get_statistics().cycles, cycles + 2);
    s.full(true);
    s.finish_cycle();
}

TEST(CollectorBoundaries_Tests, TwoSteppersOneAfterTheOther) {
    {
        collector::stepper s(false);
        s.finish_cycle();
    }
    {
        collector::stepper s(true);
        s.step();
        s.finish_cycle();
    }   // left at a gate other than released: the collector runs on by itself
    EXPECT_TRUE(collector::force_collect(true));
}

// config: what the page lists

TEST(CollectorBoundaries_Tests, ConfigAsThePageSays) {
    static_assert(config::long_sleep_time == std::chrono::seconds(30));
    static_assert(config::short_sleep_time == std::chrono::seconds(3));
    static_assert(config::pressure_sleep_time == std::chrono::milliseconds(100));
    static_assert(config::page_size == 0x10000);
    static_assert(config::chunk_size == 0x200000);
    static_assert(config::cache_line_size == 128);
#if defined(__APPLE__) && defined(__aarch64__)
    static_assert(config::l1_line_size == 128);
#endif
    static_assert(config::heap_reserve_factor == 4);
    static_assert(config::heap_reserve_minimum == size_t(64) << 30);
    static_assert(config::heap_reserve_floor == size_t(1) << 30);
    static_assert(config::heap_limit_percent == 90);
    static_assert(config::heap_pressure_percent == 75);
    static_assert(config::max_types_number == 4096);
    static_assert(config::stack_clear_size == 0x10000);
    static_assert(config::stack_guard_margin == 0x8000);
    static_assert(config::stack_scan_threshold == size_t(4) << 20);
    static_assert(config::stack_scan_segment == size_t(256) << 10);
    static_assert(config::sweep_page_threshold == 256);
    static_assert(config::young_cycles_max == 8);
    static_assert(config::full_cycle_growth_percent == 100);
    static_assert(config::io_buffer_size == 0x2000);
    static_assert(config::io_copy_buffer_size == 0x8000);
    // The defaults of the settings a build may change (-D), as the page gives them
    static_assert(config::heap_free_chunk_reserve == 32);
    static_assert(config::sweep_threads_max == 0);
    static_assert(config::mark_object_threshold == 1024 * 1024);
    static_assert(config::mark_prefetch_window == 8);
    static_assert(config::helpers_growth_threshold == size_t(16) << 20);
    static_assert(config::generational);
#if defined(__aarch64__)
    static_assert(config::backoff_max == 4096);
#elif defined(__x86_64__)
    static_assert(config::backoff_max == 1024);
#endif
    static_assert(config::workers == 0);
    static_assert(config::worker_spin_microseconds == 20);
    static_assert(config::blocking_threads == 0);
    static_assert(config::blocking_idle_milliseconds == 10000);
    SUCCEED();
}

// clock

TEST(CollectorBoundaries_Tests, ClockAtTheEndsOfADuration) {
    static_assert(clock::is_steady);
    auto a = clock::now();
    auto b = clock::now();
    EXPECT_LE(a, b);
    auto far = a + duration::max();   // saturated, not wrapped
    EXPECT_GT(far, a);
    auto early = a - duration::max();
    EXPECT_LT(early, a);
    EXPECT_EQ(a + duration::zero(), a);
    EXPECT_GE(far + duration::max(), far);
    EXPECT_LE(early - duration::max(), early);
    EXPECT_EQ(duration(b - a) >= duration::zero(), true);
}

// duration

TEST(CollectorBoundaries_Tests, DurationAtTheEndsOfItsRange) {
    const duration mx = duration::max(), mn = duration::min(), one = nanosecond;
    EXPECT_EQ(duration().nanoseconds(), 0);
    EXPECT_EQ(mx + one, mx);
    EXPECT_EQ(mn - one, mn);
    EXPECT_EQ(mn + mn, mn);
    EXPECT_EQ(mx - mn, mx);
    EXPECT_EQ(mn - mx, mn);
    EXPECT_EQ(-mn, mx);
    EXPECT_EQ(-mx, mn + one);
    EXPECT_EQ(mn * -1, mx);
    EXPECT_EQ(mn * int64_t(-1), mx);
    EXPECT_EQ(mx * 2, mx);
    EXPECT_EQ(mx * -2, mn);
    EXPECT_EQ(mx * uint64_t(UINT64_MAX), mx);
    EXPECT_EQ(one * INT64_MIN, mn);
    EXPECT_EQ(mn / -1, mx);
    EXPECT_EQ(mn / int64_t(-1), mx);
    EXPECT_EQ(mn / uint64_t(UINT64_MAX), duration());   // toward zero
    EXPECT_EQ(mn / mn, 1);
    EXPECT_EQ(mn / -one, INT64_MAX);
    EXPECT_EQ(mn % -one, duration());
    EXPECT_EQ(mn % mx, -one);
    EXPECT_EQ(mn % mn, duration());
    EXPECT_EQ(mn.abs(), mx);
    EXPECT_EQ(mx.abs(), mx);
    // truncate and round at extreme steps
    EXPECT_EQ(mn.truncate(mx), -mx);
    EXPECT_EQ(mx.truncate(mx), mx);
    EXPECT_EQ(mx.truncate(mn), mx);   // a step below zero leaves it
    EXPECT_EQ(mx.truncate(duration()), mx);
    EXPECT_EQ(mn.round(mx), -mx);
    EXPECT_EQ(mx.round(mx), mx);
    EXPECT_EQ(mx.round(one), mx);
    EXPECT_EQ(mn.round(one), mn);
    EXPECT_EQ(mx.round(second), mx);   // the next multiple is past the range: saturated
    EXPECT_EQ(mn.round(second), mn);
    EXPECT_EQ(mn.truncate(second).nanoseconds() % 1000000000, 0);
    EXPECT_EQ((-one).round(2 * one), -2 * one);   // a half away from zero
    // the compound forms at the ends
    duration d = mx;
    d += mx;
    EXPECT_EQ(d, mx);
    d -= mn;
    EXPECT_EQ(d, mx);
    d *= 3;
    EXPECT_EQ(d, mx);
    d /= -1;
    EXPECT_EQ(d, -mx);
    d %= second;
    EXPECT_GT(d, -second);
    // the conversions at the ends
    EXPECT_EQ(duration(std::chrono::hours(INT64_MAX)), mx);
    EXPECT_EQ(duration(std::chrono::hours(INT64_MIN)), mn);
    EXPECT_EQ(duration(std::chrono::duration<double>(1e300)), mx);
    EXPECT_EQ(duration(std::chrono::duration<double>(-1e300)), mn);
    EXPECT_EQ(duration(std::chrono::duration<double>(std::numeric_limits<double>::quiet_NaN())), duration());
    EXPECT_EQ(duration(std::chrono::duration<double>(std::numeric_limits<double>::infinity())), mx);
    EXPECT_EQ(std::chrono::nanoseconds(mn).count(), INT64_MIN);
    // the units of the ends
    EXPECT_EQ(mn.microseconds(), INT64_MIN / 1000);
    EXPECT_EQ(mn.milliseconds(), INT64_MIN / 1000000);
    EXPECT_LT(mn.hours(), -2562047.0);
    EXPECT_GT(mx.seconds(), 9223372036.0);
}

TEST(CollectorBoundaries_Tests, DurationTextAtItsEdges) {
    EXPECT_EQ(duration().to_string(), "0s");
    EXPECT_EQ(nanosecond.to_string(), "1ns");
    EXPECT_EQ((-nanosecond).to_string(), "-1ns");
    for (auto text : {"0", "-0", "+0", "0s", "-0s", "+0ns", "0.0s", ".0s", "0.s"}) {
        auto d = duration::parse(text);
        ASSERT_TRUE(d.has_value()) << text;
        EXPECT_EQ(*d, duration()) << text;
    }
    // A fraction of many digits: exact, cut to the nanosecond
    std::string tiny = "0." + std::string(1000, '0') + "1s";
    EXPECT_EQ(*duration::parse(string(tiny.c_str())), duration());
    std::string nines = "0." + std::string(1000, '9') + "s";
    EXPECT_EQ(duration::parse(string(nines.c_str()))->nanoseconds(), 999999999);
    std::string zeros = std::string(1000, '0') + "1s";   // leading zeros of the whole part
    EXPECT_EQ(*duration::parse(string(zeros.c_str())), second);
    // The sum at the ends, the sign once
    EXPECT_EQ(*duration::parse("2562047h47m16.854775807s"), duration::max());
    EXPECT_EQ(*duration::parse("-2562047h47m16.854775808s"), duration::min());
    EXPECT_FALSE(duration::parse("2562047h47m16.854775808s"));
    EXPECT_FALSE(duration::parse("--1s"));
    EXPECT_FALSE(duration::parse("1s-1s"));
    EXPECT_FALSE(duration::parse("+"));
    EXPECT_FALSE(duration::parse("."));
    EXPECT_FALSE(duration::parse("s"));
    // Bytes that are not text: a NUL inside, malformed UTF-8 in the unit
    auto nul = duration::parse(string("1s\0", 3));
    ASSERT_FALSE(nul);
    EXPECT_EQ(nul.error().offset(), 1u);   // the unit "s\0", as "1d" is refused at its unit
    auto bad = duration::parse("1\xC2s");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().offset(), 1u);
    auto cut = duration::parse("1\xC2");
    ASSERT_FALSE(cut);
    EXPECT_THROW(duration(string("1 s")), bad_expected_access<duration_error>);
    EXPECT_EQ(duration(string("1h")), hour);
}

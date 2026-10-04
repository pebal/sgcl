//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "os.h"

#include <atomic>
#include <chrono>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#include <x86intrin.h>
#endif
#endif

// The processor's own counter, for the spins that wait a few microseconds
// (the scheduler's workers: config::worker_spin_microseconds): read in a
// few cycles, where the system's clock is a call (mach_continuous_time on
// macOS, about a hundred nanoseconds; clock_gettime elsewhere). For a
// deadline of a spin only: not a time of day, not comparable between
// machines, and nothing the program's timeouts are measured on (those are
// sgcl::clock's).
//
//   arm64: the virtual counter, cntvct_el0, at cntfrq_el0 ticks a second
//     (24 MHz on Apple silicon); constant and monotonic by the
//     architecture;
//   x86-64: rdtsc when the time stamp counter is invariant (cpuid
//     0x80000007, bit 8 of EDX: constant rate, runs in every C-state),
//     its rate measured once, at the first use, against the steady clock
//     over about a millisecond;
//   elsewhere, and on an x86 whose counter is not invariant: the steady
//     clock's nanoseconds, a tick a nanosecond.
//
// No flag of the build is needed: every instruction here is in the base of
// its architecture, and the invariance is asked of the processor at run
// time (the CPU baseline of the library).
namespace sgcl::detail {
    namespace ticks_detail {
        SGCL_INLINE_HOT uint64_t steady_nanoseconds() noexcept {
            return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }

#if defined(__x86_64__) || defined(_M_X64)
        inline bool invariant_tsc() noexcept {
#if defined(_MSC_VER)
            int r[4];
            __cpuid(r, int(0x80000000));
            if (unsigned(r[0]) < 0x80000007u) {
                return false;
            }
            __cpuid(r, int(0x80000007));
            return (unsigned(r[3]) >> 8) & 1;
#else
            unsigned a = 0, b = 0, c = 0, d = 0;
            if (!__get_cpuid(0x80000000u, &a, &b, &c, &d) || a < 0x80000007u) {
                return false;
            }
            __get_cpuid(0x80000007u, &a, &b, &c, &d);
            return (d >> 8) & 1;
#endif
        }

        // The counter's rate, or 0 when it is not to be used: measured
        // once, against the steady clock over about a millisecond
        inline uint64_t tsc_per_second() noexcept {
            static const uint64_t rate = [] {
                if (!invariant_tsc()) {
                    return uint64_t(0);
                }
                const uint64_t t0 = steady_nanoseconds();
                const uint64_t c0 = __rdtsc();
                uint64_t t1 = t0;
                while (t1 - t0 < 1000000) {
                    t1 = steady_nanoseconds();
                }
                const uint64_t c1 = __rdtsc();
                return uint64_t((long double)(c1 - c0) * 1e9L / (long double)(t1 - t0));
            }();
            return rate;
        }
#endif
    }

    // The counter's ticks a second
    inline uint64_t ticks_per_second() noexcept {
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        static const uint64_t rate = [] {
            uint64_t f;
            __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
            return f ? f : uint64_t(1000000000);
        }();
        return rate;
#elif defined(__x86_64__) || defined(_M_X64)
        const uint64_t r = ticks_detail::tsc_per_second();
        return r ? r : uint64_t(1000000000);
#else
        return 1000000000;
#endif
    }

    // The counter now
    inline uint64_t cpu_ticks() noexcept {
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        uint64_t t;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(t));
        return t;
#elif defined(__x86_64__) || defined(_M_X64)
        if (ticks_detail::tsc_per_second()) [[likely]] {
            return __rdtsc();
        }
        return ticks_detail::steady_nanoseconds();
#else
        return ticks_detail::steady_nanoseconds();
#endif
    }

    SGCL_INLINE_HOT double ticks_per_microsecond() noexcept {
        return double(ticks_per_second()) / 1e6;
    }

    // The ticks of `us` microseconds, without rounding a fractional rate
    SGCL_INLINE_HOT uint64_t ticks_of_microseconds(uint64_t us) noexcept {
        const uint64_t rate = ticks_per_second();
        return rate / 1000000 * us + rate % 1000000 * us / 1000000;
    }
}

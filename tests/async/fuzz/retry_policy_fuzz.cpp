//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async::retry's waits for any policy: the fields read from the input as
// they come (negative, zero and huge durations, a multiplier of any double
// including NaN, the infinities and the subnormals, every jitter, any count
// of attempts), the waits drawn the retry's way (multiplied as it goes) for
// up to 300 failures, the formula itself at three draws each time:
// every wait in [0, max(0, max_delay)], no wait from a policy whose cap is
// zero, a decorrelated wait never below the first (when the cap allows it)
// nor above three times the one before, the waits of no jitter never
// shorter than the one before and equal to the formula's (retry_delay,
// the pow), the attempts counted as the policy says; and UBSan on every
// conversion between a double and a count of nanoseconds. Then a whole
// retry on this thread, of a function that fails k times, whose waits the
// policy's first bytes keep under a microsecond.
//
// The input: the policy in 34 bytes, then a byte for the whole retry's
// count of failures.
//
// Built with libFuzzer (tests/fuzz/run.sh tests/async/fuzz/retry_policy_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/async/retry.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <source_location>

namespace {
    using namespace sgcl;

    void check(bool ok, std::source_location at = std::source_location::current()) {
        if (!ok) {
            std::fprintf(stderr, "retry_policy_fuzz: check failed at line %u\n", (unsigned)at.line());
            __builtin_trap();
        }
    }

    template<class T>
    T take(const uint8_t*& p) {
        T v;
        std::memcpy(&v, p, sizeof v);
        p += sizeof v;
        return v;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 34) {
        return 0;
    }
    const uint8_t* p = data;
    async::retry_policy policy;
    policy.attempts = take<uint8_t>(p) % 64;
    policy.max_elapsed = duration(std::chrono::nanoseconds(take<int64_t>(p) >> (data[1] & 63)));
    policy.initial = duration(std::chrono::nanoseconds(take<int64_t>(p) >> (data[2] & 63)));
    policy.max_delay = duration(std::chrono::nanoseconds(take<int64_t>(p) >> (data[3] & 63)));
    policy.multiplier = take<double>(p);
    policy.jitter = async::jitter(take<uint8_t>(p) % 4);
    const double cap = policy.max_delay > duration::zero() ? (double)policy.max_delay.nanoseconds() : 0;
    const int64_t first = policy.initial > duration::zero() ? policy.initial.nanoseconds() : 0;

    async::detail::RetryState st(policy);
    duration before = duration::zero();
    for (int n = 1; n <= 300; ++n, ++st.n) {
        auto d = st.next(policy);
        if (policy.attempts != 0 && (size_t)n >= policy.attempts) {
            check(!d);
            break;
        }
        if (!d) {   // only max_elapsed refuses here
            check(policy.max_elapsed > duration::zero());
            break;
        }
        check(*d >= duration::zero());
        check((double)d->nanoseconds() <= cap);
        if (cap == 0) {
            check(*d == duration::zero());
        }
        if (policy.jitter == async::jitter::decorrelated) {
            if ((double)first <= cap) {
                check(d->nanoseconds() >= first || (double)d->nanoseconds() >= cap);
            }
            if (n > 1 && (double)first <= 3 * (double)before.nanoseconds()) {
                check((double)d->nanoseconds() <= 3 * (double)before.nanoseconds() + 1);
            }
        }
        if (policy.jitter == async::jitter::none) {
            check(*d >= before || n == 1);
            duration f = async::detail::retry_delay(policy, (size_t)n, before, 0);
            double diff = std::fabs((double)f.nanoseconds() - (double)d->nanoseconds());
            check(diff <= 1e-9 * (double)f.nanoseconds() * n + 2);
        }
        for (double u : {0.0, 0.5, 0.9999999999}) {   // the formula itself at every draw
            duration f = async::detail::retry_delay(policy, (size_t)n, before, u);
            check(f >= duration::zero() && (double)f.nanoseconds() <= cap);
        }
        before = *d;
    }
    if (p < data + size) {   // a whole retry, its waits under a microsecond
        async::retry_policy small = policy;
        small.initial = duration(std::chrono::nanoseconds(policy.initial.nanoseconds() & 255));
        small.max_delay = duration(std::chrono::nanoseconds(policy.max_delay.nanoseconds() & 511));
        small.max_elapsed = duration::zero();
        small.attempts = policy.attempts % 8;
        int fails = *p % 10;
        int calls = 0;
        auto r = async::retry([&]() -> expected<int, int> {
            ++calls;
            if (calls <= fails) {
                return unexpected(calls);
            }
            return 7;
        }, small).wait();
        int allowed = small.attempts == 0 ? fails + 1 : (int)small.attempts;
        check(calls == std::min(fails + 1, allowed));
        check(r.has_value() == (fails < allowed));
    }
    return 0;
}

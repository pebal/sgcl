//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async::rate_limiter on one thread under a manual clock, driven by a
// sequence of operations read from the input, against a model: a token
// bucket counted in ticks of credit (Go's formulation, a float of tokens
// and the time of the last update, here exact: the limits are a whole
// number of nanoseconds a token, so in the limiter's ticks of 2^-10 ns
// every quantity is a whole number). allow and reserve must give what the model gives, the
// tokens must read what the model holds; cancel, set_limit and set_burst
// are checked against their bounds (a cancel gives back between nothing
// and what was reserved, and nothing after the time; a change keeps the
// tokens up to the burst) and the model follows the limiter's reading.
// inf and zero limits come in through set_limit. The waits are the unit
// tests' and TSan's (tests/async/rate_limiter.cpp).
//
// The input: the interval (ns a token, 1..2^20) and the burst (0..255) in
// three bytes, then operations of three bytes (what, two of argument).
//
// Built with libFuzzer (tests/fuzz/run.sh tests/async/fuzz/rate_limiter_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/async/rate_limiter.h"

#include <cmath>
#include <cstdio>
#include <source_location>
#include <cstdint>
#include <optional>

namespace {
    using namespace sgcl;

    void check(bool ok, std::source_location at = std::source_location::current()) {
        if (!ok) {
            std::fprintf(stderr, "rate_limiter_fuzz: check failed at line %u\n", (unsigned)at.line());
            __builtin_trap();
        }
    }

    enum class Mode { finite, unlimited, zero };

    struct Model {
        Mode mode = Mode::finite;
        int64_t interval = 1;     // ns a token
        int64_t burst = 0;
        int64_t credit = 0;       // ticks (2^-10 ns) of tokens at `last`, at most burst * interval * 1024
        int64_t last = 0;         // ns of the clock
        int64_t left = 0;         // zero mode: the tokens left

        int64_t t() const {
            return interval * 1024;
        }

        int64_t at(int64_t now) const {
            int64_t c = credit + (now - last) * 1024;
            return c > burst * t() ? burst * t() : c;
        }

        double tokens(int64_t now) const {
            switch (mode) {
                case Mode::unlimited: return (double)burst;
                case Mode::zero: return (double)left;
                case Mode::finite: break;
            }
            return (double)at(now) / (double)t();
        }

        bool allow(int64_t now, int64_t n) {
            switch (mode) {
                case Mode::unlimited: return true;
                case Mode::zero:
                    if (left < n) {
                        return false;
                    }
                    left -= n;
                    return true;
                case Mode::finite: break;
            }
            if (n > burst) {
                return false;
            }
            int64_t c = at(now);
            if (c < n * t()) {
                return false;
            }
            credit = c - n * t();
            last = now;
            return true;
        }

        // ok, and the ns from now until the act
        std::optional<int64_t> reserve(int64_t now, int64_t n) {
            switch (mode) {
                case Mode::unlimited: return 0;
                case Mode::zero:
                    if (left < n) {
                        return std::nullopt;
                    }
                    left -= n;
                    return 0;
                case Mode::finite: break;
            }
            if (n > burst) {
                return std::nullopt;
            }
            credit = at(now) - n * t();
            last = now;
            return credit < 0 ? (-credit + 1023) / 1024 : 0;
        }

        // The model taken from the limiter's reading
        void follow(int64_t now, double tokens) {
            last = now;
            if (mode == Mode::zero) {
                left = (int64_t)std::floor(tokens);
            } else {
                credit = (int64_t)std::llround(tokens * (double)t());
            }
        }
    };

    struct Held {
        async::rate_limiter::reservation r;
        int64_t n = 0;
        int64_t act = 0;
    };

    struct Slots {   // the reservations hold a tracked word: in a managed object
        Held at[4];
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    async::manual_clock clock;
    clock.install();
    const time_point start = clock::now();
    auto now_ns = [&] { return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start).count(); };

    Model m;
    m.interval = 1 + (((int64_t)data[0] << 8 | data[1]) << (data[2] >> 6 & 3));   // 1..2^18 ns a token
    m.burst = data[2] & 0x3f;
    m.credit = m.burst * m.t();
    async::rate_limiter lim(1e9 / (double)m.interval, (size_t)m.burst);
    {
        // the limiter's interval rounded from the double: the model takes the limiter's
        double per = 1e9 / (double)m.interval;
        check(std::fabs(lim.limit() - per) <= per * 1e-12);
    }
    tracked_ptr<Slots> slots = make_tracked<Slots>();
    Held* held = slots->at;

    for (size_t i = 3; i + 2 < size; i += 3) {
        const uint8_t what = data[i];
        const uint16_t arg = (uint16_t)(data[i + 1] << 8 | data[i + 2]);
        const int64_t now = now_ns();
        switch (what % 9) {
        case 0:
        case 1: {   // allow
            int64_t n = arg % 70;
            check(lim.allow((size_t)n) == m.allow(now, n));
            break;
        }
        case 2: {   // reserve into a slot
            int64_t n = arg % 70;
            Held& h = held[arg >> 8 & 3];
            auto want = m.reserve(now, n);
            h.r = lim.reserve((size_t)n);
            check(h.r.ok() == want.has_value());
            if (want) {
                check(h.r.delay().nanoseconds() == *want);
                h.n = n;
                h.act = now + *want;
            }
            break;
        }
        case 3: {   // cancel a slot: between nothing and n back, nothing at or after the time
            Held& h = held[arg & 3];
            double before = lim.tokens();
            bool was = h.r.ok();
            h.r.cancel();
            check(!h.r.ok());
            double after = lim.tokens();
            check(after >= before - 1e-9);
            if (!was || now >= h.act || m.mode != Mode::finite) {
                check(after == before);
            } else {
                check(after <= before + (double)h.n + 1e-9);
            }
            m.follow(now, after);
            break;
        }
        case 4:
        case 5:   // the clock
            clock.advance(std::chrono::nanoseconds((int64_t)arg << (what >> 4 & 7)));
            break;
        case 6: {   // tokens
            double t = lim.tokens();
            check(std::fabs(t - m.tokens(now)) <= 1e-9 * (1 + std::fabs(t)));
            break;
        }
        case 7: {   // set_limit: another whole interval, inf or zero
            double before = lim.tokens();
            int k = arg % 8;
            if (k == 0) {
                lim.set_limit(async::rate_limiter::inf);
                m.mode = Mode::unlimited;
            } else if (k == 1) {
                lim.set_limit(0.0);
                m.mode = Mode::zero;
            } else {
                Mode was = m.mode;
                m.interval = 1 + ((int64_t)arg >> 3);
                m.mode = Mode::finite;
                lim.set_limit(std::chrono::nanoseconds(m.interval));
                if (was == Mode::unlimited) {
                    before = (double)m.burst;   // full after inf
                }
            }
            double after = lim.tokens();
            if (m.mode == Mode::zero) {
                check(after == std::floor(before) || (before > (double)m.burst && after == (double)m.burst));
            } else if (m.mode == Mode::finite) {
                check(std::fabs(after - before) <= 0.5 / (double)m.t() + 1e-9 * (1 + std::fabs(before)));   // the new epoch's word is whole ticks
            }
            m.follow(now, after);
            for (int k = 0; k < 4; ++k) {   // reservations before a change give nothing back
                held[k].act = 0;
            }
            break;
        }
        case 8: {   // set_burst
            double before = lim.tokens();
            m.burst = arg % 64;
            lim.set_burst((size_t)m.burst);
            double after = lim.tokens();
            if (m.mode == Mode::finite) {
                double want = before > (double)m.burst ? (double)m.burst : before;
                check(std::fabs(after - want) <= 0.5 / (double)m.t() + 1e-9 * (1 + std::fabs(want)));
            }
            check(lim.burst() == (size_t)m.burst);
            m.follow(now, after);
            for (int k = 0; k < 4; ++k) {
                held[k].act = 0;
            }
            break;
        }
        }
    }
    clock.uninstall();
    return 0;
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The async module's coordination of callers: a rate limiter,
// singleflight and retry. One case
// per run; prints one line, ns per operation, for compare.sh
// (CASES=coordination). benchmarks/go/coordination is the Go side (the
// algorithms of golang.org/x/time/rate and golang.org/x/sync/singleflight
// written out, the x modules not being on this machine; a retry loop of
// the same policy, Go having none).
//
//   coordination allow sgcl [n]      allow() with tokens always there (1e9 a second, a burst of 1e9), one thread
//   coordination deny sgcl [n]       allow() of an empty bucket (1 a second, burst 1, taken), one thread
//   coordination allowpar sgcl [n]   allow() from 8 threads on one bucket with tokens always there, per call
//   coordination waitnow sgcl [n]    co_await wait() in a task with the tokens there
//   coordination paced sgcl [n]      co_await wait() in a task at 10000 a second, burst 1: per wait (100 us is exact)
//   coordination sfsolo sgcl [n]     singleflight run(key, f) on a thread, one caller, f returns at once: a call made and ended
//   coordination sftask sgcl [n]     the same, co_await in a task
//   coordination sfpar sgcl [n]      8 threads on 16 keys, f returns at once: per call
//   coordination sfshared sgcl [n]   8 tasks on one key, f 2 us of work: per caller's call (most of them join)
//   coordination retryok sgcl [n]    retry(f) on a thread, f a value at once: per retry
//   coordination retrytask sgcl [n]  the same, co_await in a task
//   coordination retryfail sgcl [n]  retry(f) on a thread, f failing three times then a value, waits of zero: per retry
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {
    namespace async = sgcl::async;

    void report(const char* what, double wall, long ops) {
        std::printf("coordination %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    async::task<long> flights_solo(async::singleflight flights, long n) {
        long sum = 0;
        sgcl::string key("key");
        for (long i = 0; i < n; ++i) {
            sum += co_await flights.run(key, [i] { return i; });
        }
        co_return sum;
    }

    // The work of a shared call: 2 us of the processor, so that the
    // callers that join a call are those of the same 2 us on both sides (a
    // yield's length is the scheduler's, and Go's Gosched is not ours)
    long spin_2us() {
        auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(2);
        while (std::chrono::steady_clock::now() < until) {
        }
        return 1;
    }

    async::task<long> flights_shared(async::singleflight flights, long n) {
        long sum = 0;
        sgcl::string key("key");
        for (long i = 0; i < n; ++i) {
            sum += co_await flights.run(key, [] { return spin_2us(); });
        }
        co_return sum;
    }

    sgcl::expected<long, int> succeed(long i) {
        return i;
    }

    async::task<long> retries_in_task(long n) {
        long sum = 0;
        for (long i = 0; i < n; ++i) {
            sum += *co_await async::retry([i] { return succeed(i); });
        }
        co_return sum;
    }

    async::task<long> waits(async::rate_limiter lim, long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            ok += (bool)co_await lim.acquire();
        }
        co_return ok;
    }
}

int main(int argc, char** argv) {
    std::string what = argc > 1 ? argv[1] : "";
    std::string v = argc > 2 ? argv[2] : "";
    if (!bench::has_variant(v.c_str(), {"sgcl"})) {
        std::fprintf(stderr, "usage: coordination <allow|deny|allowpar|waitnow|paced|sfsolo|sftask|sfpar|sfshared|retryok|retrytask|retryfail> sgcl [n]\n");
        return 2;
    }
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    long sum = 0;
    if (what == "allow") {
        n = n ? n : 20'000'000;
        async::rate_limiter lim(1e9, 1'000'000'000);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sum += lim.allow();
        }
        report("allow", bench::seconds_since(t0), n);
    } else if (what == "deny") {
        n = n ? n : 20'000'000;
        async::rate_limiter lim(1.0, 1);
        sum += lim.allow();
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sum += lim.allow();
        }
        report("deny", bench::seconds_since(t0), n);
    } else if (what == "allowpar") {
        n = n ? n : 4'000'000;
        async::rate_limiter lim(1e9, 1'000'000'000);
        std::atomic<long> total{0};
        auto t0 = bench::Clock::now();
        {
            std::vector<std::thread> ts;
            for (int t = 0; t < 8; ++t) {
                ts.emplace_back([&] {
                    async::rate_limiter mine = lim;   // on this thread's stack: a handle never in a std::thread's closure
                    long k = 0;
                    for (long i = 0; i < n; ++i) {
                        k += mine.allow();
                    }
                    total += k;
                });
            }
            for (auto& t : ts) {
                t.join();
            }
        }
        sum += total;
        report("allowpar", bench::seconds_since(t0), 8 * n);
    } else if (what == "waitnow") {
        n = n ? n : 5'000'000;
        async::rate_limiter lim(1e9, 1'000'000'000);
        auto t0 = bench::Clock::now();
        sum += async::spawn(waits(lim, n)).wait();
        report("waitnow", bench::seconds_since(t0), n);
    } else if (what == "paced") {
        n = n ? n : 10'000;
        async::rate_limiter lim(10'000.0, 1);
        (void)lim.allow();
        auto t0 = bench::Clock::now();
        sum += async::spawn(waits(lim, n)).wait();
        report("paced", bench::seconds_since(t0), n);
    } else if (what == "sfsolo") {
        n = n ? n : 5'000'000;
        async::singleflight flights;
        sgcl::string key("key");
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sum += flights.run(key, [i] { return i; }).wait();
        }
        report("sfsolo", bench::seconds_since(t0), n);
    } else if (what == "sftask") {
        n = n ? n : 5'000'000;
        async::singleflight flights;
        auto t0 = bench::Clock::now();
        sum += async::spawn(flights_solo(flights, n)).wait();
        report("sftask", bench::seconds_since(t0), n);
    } else if (what == "sfpar") {
        n = n ? n : 1'000'000;
        async::singleflight flights;
        std::atomic<long> total{0};
        auto t0 = bench::Clock::now();
        {
            std::vector<std::thread> ts;
            for (int t = 0; t < 8; ++t) {
                ts.emplace_back([&, t] {
                    async::singleflight mine = flights;   // on this thread's stack
                    sgcl::string keys[16];
                    for (int k = 0; k < 16; ++k) {
                        keys[k] = sgcl::to_string(k);
                    }
                    long k = 0;
                    for (long i = 0; i < n; ++i) {
                        k += mine.run(keys[(i + t) & 15], [i] { return i; }).wait();
                    }
                    total += k;
                });
            }
            for (auto& t : ts) {
                t.join();
            }
        }
        sum += total;
        report("sfpar", bench::seconds_since(t0), 8 * n);
    } else if (what == "sfshared") {
        n = n ? n : 500'000;
        async::singleflight flights;
        std::vector<async::task<long>> ts;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < 8; ++t) {
            ts.push_back(async::spawn(flights_shared(flights, n)));
        }
        for (auto& t : ts) {
            sum += t.wait();
        }
        report("sfshared", bench::seconds_since(t0), 8 * n);
    } else if (what == "retryok") {
        n = n ? n : 10'000'000;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sum += *async::retry([i] { return succeed(i); }).wait();
        }
        report("retryok", bench::seconds_since(t0), n);
    } else if (what == "retrytask") {
        n = n ? n : 10'000'000;
        auto t0 = bench::Clock::now();
        sum += async::spawn(retries_in_task(n)).wait();
        report("retrytask", bench::seconds_since(t0), n);
    } else if (what == "retryfail") {
        n = n ? n : 5'000'000;
        async::retry_policy policy;
        policy.initial = sgcl::duration::zero();
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            int left = 3;
            sum += *async::retry([&left, i]() -> sgcl::expected<long, int> {
                if (left-- > 0) {
                    return sgcl::unexpected(left);
                }
                return i;
            }, policy).wait();
        }
        report("retryfail", bench::seconds_since(t0), n);
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    if (sum < 0) {
        std::printf("%ld\n", sum);
    }
}

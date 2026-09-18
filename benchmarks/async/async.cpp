//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The async module: what a hop, a wait and a race cost on the scheduler.
// One case per run, the tasks on the scheduler's workers unless the case
// says; prints one line, ns per operation, for compare.sh (CASES=async).
//
//   async yield sgcl [n]        co_await yield() on a worker
//   async exyield sgcl [n]      co_await yield() on an executor's thread
//   async strand sgcl [n]       on_workers() then on(strand): a round trip
//   async await sgcl [n]        co_await of a task that returns at once, in a chain
//   async spawn sgcl [n]        co_await of a task spawned on the scheduler, one at a time
//   async whenall sgcl [n]      when_all of two tasks that return at once, per task
//   async timeout sgcl [n]      timeout(t, 1h) of a task that returns at once: a race per iteration
//   async select sgcl [n]       async_select of a channel with an element and a timeout case of an hour
//   async cv sgcl [n]           two tasks handing a turn to each other through a condition variable, per hand-off
//   async pingpong sgcl [n]     two tasks over two rendezvous channels, per hop
//   async generator sgcl [n]    an async_generator yielding n values to a task, per value
//   async mutex sgcl [n]        async_scoped_lock and its release, uncontended
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace std::chrono_literals;

namespace {
    sgcl::task<int> leaf(int v) {
        co_return v;
    }

    sgcl::task<> yields(long n) {
        for (long i = 0; i < n; ++i) {
            co_await sgcl::yield();
        }
    }

    sgcl::task<> strand_hops(sgcl::strand& s, long n) {
        for (long i = 0; i < n; ++i) {
            co_await sgcl::on_workers();
            co_await sgcl::on(s);
        }
    }

    sgcl::task<long> await_chain(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            s += co_await leaf(int(i));
        }
        co_return s;
    }

    sgcl::task<long> spawn_chain(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            s += co_await sgcl::spawn(leaf(int(i)));
        }
        co_return s;
    }

    sgcl::task<long> whenall_loop(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            auto [a, b] = co_await sgcl::when_all(leaf(int(i)), leaf(int(i)));
            s += a + b;
        }
        co_return s;
    }

    sgcl::task<long> timeout_loop(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            auto r = co_await sgcl::timeout(leaf(int(i)), 1h);
            s += *r;
        }
        co_return s;
    }

    sgcl::task<long> select_loop(sgcl::channel<int>& ch, long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            ch.try_send(int(i));   // an element is there: the channel's case is served at once
            co_await sgcl::async_select(ch.on_receive([&](int v) { s += v; }), sgcl::timeout(1h, [] {}));
        }
        co_return s;
    }

    sgcl::task<> cv_side(sgcl::mutex& m, sgcl::condition_variable& cv, int& turn, int mine, long n) {
        for (long i = 0; i < n; ++i) {
            auto g = co_await m.async_scoped_lock();
            while (turn % 2 != mine) {
                co_await cv.async_wait(g);
            }
            ++turn;
            cv.notify_one();
        }
    }

    sgcl::task<> ping(sgcl::channel<int>& a, sgcl::channel<int>& b, long n) {
        for (long i = 0; i < n; ++i) {
            co_await a.async_send(int(i));
            co_await b.async_receive();
        }
    }

    sgcl::task<> pong(sgcl::channel<int>& a, sgcl::channel<int>& b, long n) {
        for (long i = 0; i < n; ++i) {
            co_await a.async_receive();
            co_await b.async_send(int(i));
        }
    }

    sgcl::async_generator<int> counting(long n) {
        for (long i = 0; i < n; ++i) {
            co_yield int(i);
        }
    }

    sgcl::task<long> consume(long n) {
        long s = 0;
        auto g = counting(n);
        while (auto v = co_await g.next()) {
            s += *v;
        }
        co_return s;
    }

    sgcl::task<long> mutex_loop(sgcl::mutex& m, long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            auto g = co_await m.async_scoped_lock();
            s += i;
        }
        co_return s;
    }

    void report(const char* what, double wall, long ops) {
        std::printf("async %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    std::string what = argc > 1 ? argv[1] : "";
    std::string v = argc > 2 ? argv[2] : "";
    if (!bench::has_variant(v.c_str(), {"sgcl"})) {
        std::fprintf(stderr, "usage: async <yield|exyield|strand|await|spawn|whenall|timeout|select|cv|pingpong|generator|mutex> sgcl [n]\n");
        return 2;
    }
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    long sum = 0;
    if (what == "yield") {
        n = n ? n : 2'000'000;
        auto t0 = bench::Clock::now();
        sgcl::spawn(yields(n)).join();
        report("yield", bench::seconds_since(t0), n);
    } else if (what == "exyield") {
        n = n ? n : 2'000'000;
        sgcl::executor ex;
        auto t0 = bench::Clock::now();
        ex.run(yields(n));
        report("exyield", bench::seconds_since(t0), n);
    } else if (what == "strand") {
        n = n ? n : 500'000;
        sgcl::strand s;
        auto t0 = bench::Clock::now();
        sgcl::spawn(strand_hops(s, n)).join();
        report("strand", bench::seconds_since(t0), n);
    } else if (what == "await") {
        n = n ? n : 2'000'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::spawn(await_chain(n)).join();
        report("await", bench::seconds_since(t0), n);
    } else if (what == "spawn") {
        n = n ? n : 500'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::spawn(spawn_chain(n)).join();
        report("spawn", bench::seconds_since(t0), n);
    } else if (what == "whenall") {
        n = n ? n : 500'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::spawn(whenall_loop(n)).join();
        report("whenall", bench::seconds_since(t0), 2 * n);
    } else if (what == "timeout") {
        n = n ? n : 200'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::spawn(timeout_loop(n)).join();
        report("timeout", bench::seconds_since(t0), n);
    } else if (what == "select") {
        n = n ? n : 500'000;
        sgcl::channel<int> ch(1);
        auto t0 = bench::Clock::now();
        sum = sgcl::spawn(select_loop(ch, n)).join();
        report("select", bench::seconds_since(t0), n);
    } else if (what == "cv") {
        n = n ? n : 200'000;
        sgcl::mutex m;
        sgcl::condition_variable cv;
        int turn = 0;
        auto t0 = bench::Clock::now();
        auto a = sgcl::spawn(cv_side(m, cv, turn, 0, n));
        auto b = sgcl::spawn(cv_side(m, cv, turn, 1, n));
        a.join();
        b.join();
        report("cv", bench::seconds_since(t0), 2 * n);
    } else if (what == "pingpong") {
        n = n ? n : 500'000;
        sgcl::channel<int> a, b;
        auto t0 = bench::Clock::now();
        auto p = sgcl::spawn(ping(a, b, n));
        auto q = sgcl::spawn(pong(a, b, n));
        p.join();
        q.join();
        report("pingpong", bench::seconds_since(t0), 2 * n);
    } else if (what == "generator") {
        n = n ? n : 2'000'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::spawn(consume(n)).join();
        report("generator", bench::seconds_since(t0), n);
    } else if (what == "mutex") {
        n = n ? n : 2'000'000;
        sgcl::mutex m;
        auto t0 = bench::Clock::now();
        sum = sgcl::spawn(mutex_loop(m, n)).join();
        report("mutex", bench::seconds_since(t0), n);
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    if (sum == -1) {
        std::printf("?");
    }
    sgcl::scheduler::stop();
    return 0;
}

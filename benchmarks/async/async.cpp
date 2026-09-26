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
//   async exhop sgcl [n]        on_workers() then on(ex): a round trip between an executor's thread and a worker
//   async expost sgcl [n]       ex.go() of a task from a foreign thread into a running executor, per task
//   async exmany sgcl [n]       ex.go() from four foreign threads at once into one executor, per task
//   async exmanyhop sgcl [n]    eight tasks hopping on(ex) and back at once: many producers, per hop
//   async strandmany sgcl [n]   eight tasks hopping on(strand) and back at once, per hop
//   async await sgcl [n]        co_await of a task that returns at once, in a chain
//   async spawn sgcl [n]        co_await of a task spawned on the scheduler, one at a time
//   async whenall sgcl [n]      when_all of two tasks that return at once, per task
//   async timeout sgcl [n]      with_timeout(t, 1h) of a task that returns at once: a race per iteration
//   async select sgcl [n]       select of a channel with an element and a timeout case of an hour
//   async cv sgcl [n]           two tasks handing a turn to each other through a condition variable, per hand-off
//   async pingpong sgcl [n]     two tasks over two rendezvous channels, per hop
//   async generator sgcl [n]    an async::generator yielding n values to a task, per value
//   async mutex sgcl [n]        scoped_lock and its release, uncontended
//   async bcast sgcl [n] [k] [gap]  a broadcast of n values, k task subscribers (16), the sender a task waiting for the k per value; per value
//                               (gap: microseconds the sender sleeps before each value, the workers asleep by then; the median round)
//   async bcastth sgcl [n] [k]  a broadcast of n values into a ring of 1024, k task subscribers, the sender the main thread, never waiting
//   async notifyall sgcl [n] [k]  k tasks woken together by condition_variable::notify_all, n rounds; per round
//   async wgclose sgcl [n] [k]  k tasks woken together by the close of a wait_group's round, n rounds; per round
//   async wgcloseth sgcl [n] [k]  the same, the round closed by the main thread
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

using namespace sgcl::async;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {
    sgcl::async::task<int> leaf(int v) {
        co_return v;
    }

    sgcl::async::task<> yields(long n) {
        for (long i = 0; i < n; ++i) {
            co_await sgcl::async::yield();
        }
    }

    sgcl::async::task<> strand_hops(sgcl::async::strand& s, long n) {
        for (long i = 0; i < n; ++i) {
            co_await sgcl::async::on_workers();
            co_await sgcl::async::on(s);
        }
    }

    // The executor's queue: a round trip through it, from the executor's
    // thread to a worker and back
    sgcl::async::task<> ex_hops(sgcl::async::executor& ex, long n) {
        for (long i = 0; i < n; ++i) {
            co_await sgcl::async::on_workers();
            co_await sgcl::async::on(ex);
        }
    }

    // A task posted into an executor: counts itself, and the last stops it
    sgcl::async::task<> ex_posted(sgcl::async::executor& ex, std::atomic<long>& left) {
        if (left.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ex.stop();
        }
        co_return;
    }

    // Tasks posted into a running executor from `threads` foreign threads,
    // n in all; the time from the first post to the run's end
    double ex_post(long n, unsigned threads) {
        sgcl::async::executor ex;
        std::atomic<long> left = {n};
        std::atomic<bool> go = {false};
        std::vector<std::thread> producers;
        for (unsigned k = 0; k < threads; ++k) {
            producers.emplace_back([&, k] {
                while (!go.load(std::memory_order_acquire)) {
                }
                for (long i = k; i < n; i += threads) {
                    ex.go(ex_posted(ex, left));
                }
            });
        }
        auto t0 = bench::Clock::now();
        go.store(true, std::memory_order_release);
        ex.run();
        auto wall = bench::seconds_since(t0);
        for (auto& p : producers) {
            p.join();
        }
        return wall;
    }

    // Many producers into one executor or strand: `tasks` tasks each
    // hopping onto it and back to the workers n / tasks times
    template<class Executor>
    sgcl::async::task<> hops_on(Executor& ex, long n) {
        for (long i = 0; i < n; ++i) {
            co_await sgcl::async::on(ex);
            co_await sgcl::async::on_workers();
        }
    }

    template<class Executor>
    sgcl::async::task<> many_hops(Executor& ex, long n, unsigned tasks) {
        std::vector<sgcl::async::task<>> ts;
        for (unsigned k = 0; k < tasks; ++k) {
            ts.push_back(sgcl::async::spawn(hops_on(ex, n / tasks)));
        }
        for (auto& t : ts) {
            co_await t;
        }
    }

    sgcl::async::task<long> await_chain(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            s += co_await leaf(int(i));
        }
        co_return s;
    }

    sgcl::async::task<long> spawn_chain(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            s += co_await sgcl::async::spawn(leaf(int(i)));
        }
        co_return s;
    }

    sgcl::async::task<long> whenall_loop(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            auto [a, b] = co_await sgcl::async::when_all(leaf(int(i)), leaf(int(i)));
            s += a + b;
        }
        co_return s;
    }

    sgcl::async::task<long> timeout_loop(long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            auto r = co_await sgcl::async::with_timeout(leaf(int(i)), 1h);
            s += *r;
        }
        co_return s;
    }

    sgcl::async::task<long> select_loop(sgcl::async::channel<int>& ch, long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            ch.try_send(int(i));   // an element is there: the channel's case is served at once
            co_await sgcl::async::select(ch.on_receive([&](int v) { s += v; }), sgcl::async::timeout(1h, [] {}));
        }
        co_return s;
    }

    sgcl::async::task<> cv_side(sgcl::async::mutex& m, sgcl::async::condition_variable& cv, int& turn, int mine, long n) {
        for (long i = 0; i < n; ++i) {
            auto g = co_await m.scoped_lock();
            while (turn % 2 != mine) {
                co_await cv.wait(g);
            }
            ++turn;
            cv.notify_one();
        }
    }

    sgcl::async::task<> ping(sgcl::async::channel<int>& a, sgcl::async::channel<int>& b, long n) {
        for (long i = 0; i < n; ++i) {
            co_await a.send(int(i));
            co_await b.receive();
        }
    }

    sgcl::async::task<> pong(sgcl::async::channel<int>& a, sgcl::async::channel<int>& b, long n) {
        for (long i = 0; i < n; ++i) {
            co_await a.receive();
            co_await b.send(int(i));
        }
    }

    sgcl::async::generator<int> counting(long n) {
        for (long i = 0; i < n; ++i) {
            co_yield int(i);
        }
    }

    sgcl::async::task<long> consume(long n) {
        long s = 0;
        auto g = counting(n);
        while (auto v = co_await g.next()) {
            s += *v;
        }
        co_return s;
    }

    sgcl::async::task<long> mutex_loop(sgcl::async::mutex& m, long n) {
        long s = 0;
        for (long i = 0; i < n; ++i) {
            auto g = co_await m.scoped_lock();
            s += i;
        }
        co_return s;
    }

    // Wake-many: one side waking k tasks at once. bcast: k tasks
    // subscribed to a broadcast of a ring of 1024 receive every value a
    // sender task sends, and the sender waits for the k to have it (a
    // wait group, done() by each) before it sends the next: every value
    // wakes the k, one hand-off per round (with a gap, the sender sleeps
    // before each value and the workers with it, and the time is that of
    // a round alone, the median: the wake of sleeping workers). bcastth:
    // the sender the main thread, which never waits (concurrent.cpp's
    // bcast task case), so that a subscriber is woken for a value only
    // when it caught up and waits (received: the share the subscribers
    // got, a lapped one loses some).
    // notifyall: k tasks wait on a condition variable, and a coordinator
    // task, once all k are registered (counted under the mutex, which the
    // wait lets go of after its registration), moves the generation on
    // and notifies all; each woken task takes the mutex back and counts
    // itself in for the next round. wgclose: k tasks wait on a wait
    // group's round; the coordinator closes it with done() once all k
    // have counted themselves in (a task counted and not yet on the
    // round's channel finds it closed and goes on without a wake), and
    // waits for the k to come back through a second group; wgcloseth:
    // the coordinator the main thread.
    sgcl::async::task<long> bcast_reader(sgcl::async::broadcast<long>& b, std::atomic<int>& subscribed, sgcl::async::wait_group* back) {
        auto s = b.subscribe();
        subscribed.fetch_add(1);
        long got = 0;
        while (auto v = co_await s.receive()) {
            got += *v >= 0;
            if (back) {
                back->done();
            }
        }
        co_return got;
    }

    sgcl::async::task<> bcast_sender(sgcl::async::broadcast<long>& b, sgcl::async::wait_group& back, int k, long n, long gap_us, std::vector<double>& rounds) {
        for (long i = 0; i < n; ++i) {
            if (gap_us) {
                co_await sgcl::async::sleep(std::chrono::microseconds(gap_us));
            }
            auto t0 = bench::Clock::now();
            back.add(k);
            b.send(i);
            co_await back;
            if (gap_us) {
                rounds.push_back(bench::seconds_since(t0));
            }
        }
        b.close();
    }

    void run_bcast(bool on_thread, long n, int k, long gap_us) {
        sgcl::async::broadcast<long> b(1024);
        sgcl::async::wait_group back;
        std::atomic<int> subscribed = {0};
        sgcl::async::scheduler::workers();   // started before the clock
        std::vector<sgcl::async::task<long>> rs;
        for (int t = 0; t < k; ++t) {
            rs.push_back(sgcl::async::spawn(bcast_reader(b, subscribed, on_thread ? nullptr : &back)));
        }
        while (subscribed.load() < k) {
            std::this_thread::yield();
        }
        std::vector<double> rounds;
        rounds.reserve(gap_us ? n : 0);
        auto t0 = bench::Clock::now();
        if (on_thread) {
            for (long i = 0; i < n; ++i) {
                b.send(i);
            }
            b.close();
        } else {
            sgcl::async::spawn(bcast_sender(b, back, k, n, gap_us, rounds)).wait();
        }
        long received = 0;
        for (auto& r : rs) {
            received += r.wait();
        }
        double wall = bench::seconds_since(t0);
        double per = wall / n;
        if (gap_us) {   // the median round: a round after a sleep is at the mercy of the cores' idle states, and a mean is its outliers'
            std::sort(rounds.begin(), rounds.end());
            per = rounds[rounds.size() / 2];
        }
        std::printf("async %s subscribers=%d ns/op=%.1f received=%.0f%% wall=%.2fs cpu=%.2fs\n", on_thread ? "bcastth" : "bcast", k, per * 1e9, 100.0 * received / ((double)n * k), wall, bench::cpu_seconds());
    }

    struct NotifyRounds {
        sgcl::async::mutex m;
        sgcl::async::condition_variable cv;
        sgcl::async::condition_variable back;
        long generation = 0;
        long ready = 0;
    };

    sgcl::async::task<> notify_waiter(NotifyRounds& s, int k, long n) {
        for (long r = 1; r <= n; ++r) {
            auto g = co_await s.m.scoped_lock();
            if (++s.ready == k * r) {
                s.back.notify_one();
            }
            while (s.generation < r) {
                co_await s.cv.wait(g);
            }
        }
    }

    sgcl::async::task<> notify_coordinator(NotifyRounds& s, int k, long n) {
        for (long r = 1; r <= n; ++r) {
            auto g = co_await s.m.scoped_lock();
            while (s.ready < k * r) {
                co_await s.back.wait(g);
            }
            ++s.generation;
            s.cv.notify_all();
        }
    }

    void run_notifyall(long n, int k) {
        NotifyRounds s;
        sgcl::async::scheduler::workers();
        auto t0 = bench::Clock::now();
        std::vector<sgcl::async::task<>> ws;
        for (int t = 0; t < k; ++t) {
            ws.push_back(sgcl::async::spawn(notify_waiter(s, k, n)));
        }
        sgcl::async::spawn(notify_coordinator(s, k, n)).wait();
        for (auto& w : ws) {
            w.wait();
        }
        double wall = bench::seconds_since(t0);
        std::printf("async notifyall waiters=%d ns/op=%.1f per-wake=%.1f wall=%.2fs cpu=%.2fs\n", k, wall * 1e9 / n, wall * 1e9 / ((double)n * k), wall, bench::cpu_seconds());
    }

    struct GroupRounds {
        sgcl::async::wait_group gate[2];   // round r waits on gate[r % 2]: the next round's is armed before this one's is closed
        sgcl::async::wait_group back;
        std::atomic<long> arrived = {0};
    };

    sgcl::async::task<> group_waiter(GroupRounds& s, long n) {
        for (long r = 1; r <= n; ++r) {
            s.arrived.fetch_add(1, std::memory_order_acq_rel);
            co_await s.gate[r % 2];
            s.back.done();
        }
    }

    sgcl::async::task<> group_coordinator(GroupRounds& s, int k, long n) {
        for (long r = 1; r <= n; ++r) {
            while (s.arrived.load(std::memory_order_acquire) < k * r) {
                co_await sgcl::async::yield();
            }
            s.gate[(r + 1) % 2].add(1);
            s.back.add(k);
            s.gate[r % 2].done();
            co_await s.back;
        }
    }

    void group_coordinator_thread(GroupRounds& s, int k, long n) {
        for (long r = 1; r <= n; ++r) {
            while (s.arrived.load(std::memory_order_acquire) < k * r) {
                std::this_thread::yield();
            }
            s.gate[(r + 1) % 2].add(1);
            s.back.add(k);
            s.gate[r % 2].done();
            s.back.wait();
        }
    }

    void run_wgclose(bool on_thread, long n, int k) {
        GroupRounds s;
        s.gate[1].add(1);
        sgcl::async::scheduler::workers();
        auto t0 = bench::Clock::now();
        std::vector<sgcl::async::task<>> ws;
        for (int t = 0; t < k; ++t) {
            ws.push_back(sgcl::async::spawn(group_waiter(s, n)));
        }
        if (on_thread) {
            group_coordinator_thread(s, k, n);
        } else {
            sgcl::async::spawn(group_coordinator(s, k, n)).wait();
        }
        for (auto& w : ws) {
            w.wait();
        }
        double wall = bench::seconds_since(t0);
        std::printf("async %s waiters=%d ns/op=%.1f per-wake=%.1f wall=%.2fs cpu=%.2fs\n", on_thread ? "wgcloseth" : "wgclose", k, wall * 1e9 / n, wall * 1e9 / ((double)n * k), wall, bench::cpu_seconds());
    }

    void report(const char* what, double wall, long ops) {
        std::printf("async %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    std::string what = argc > 1 ? argv[1] : "";
    std::string v = argc > 2 ? argv[2] : "";
    if (!bench::has_variant(v.c_str(), {"sgcl"})) {
        std::fprintf(stderr, "usage: async <yield|exyield|strand|exhop|expost|exmany|exmanyhop|strandmany|await|spawn|whenall|timeout|select|cv|pingpong|generator|mutex|bcast|bcastth|notifyall|wgclose|wgcloseth> sgcl [n] [k]\n");
        return 2;
    }
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    long sum = 0;
    if (what == "yield") {
        n = n ? n : 2'000'000;
        auto t0 = bench::Clock::now();
        sgcl::async::spawn(yields(n)).wait();
        report("yield", bench::seconds_since(t0), n);
    } else if (what == "exyield") {
        n = n ? n : 2'000'000;
        sgcl::async::executor ex;
        auto t0 = bench::Clock::now();
        ex.run(yields(n));
        report("exyield", bench::seconds_since(t0), n);
    } else if (what == "strand") {
        n = n ? n : 500'000;
        sgcl::async::strand s;
        auto t0 = bench::Clock::now();
        sgcl::async::spawn(strand_hops(s, n)).wait();
        report("strand", bench::seconds_since(t0), n);
    } else if (what == "exhop") {
        n = n ? n : 500'000;
        sgcl::async::executor ex;
        auto t0 = bench::Clock::now();
        ex.run(ex_hops(ex, n));
        report("exhop", bench::seconds_since(t0), n);
    } else if (what == "expost") {
        n = n ? n : 2'000'000;
        report("expost", ex_post(n, 1), n);
    } else if (what == "exmany") {
        n = n ? n : 2'000'000;
        report("exmany", ex_post(n, 4), n);
    } else if (what == "exmanyhop") {
        n = n ? n : 800'000;
        sgcl::async::executor ex;
        auto t0 = bench::Clock::now();
        ex.run(many_hops(ex, n, 8));
        report("exmanyhop", bench::seconds_since(t0), n);
    } else if (what == "strandmany") {
        n = n ? n : 800'000;
        sgcl::async::strand s;
        auto t0 = bench::Clock::now();
        sgcl::async::spawn(many_hops(s, n, 8)).wait();
        report("strandmany", bench::seconds_since(t0), n);
    } else if (what == "await") {
        n = n ? n : 2'000'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::async::spawn(await_chain(n)).wait();
        report("await", bench::seconds_since(t0), n);
    } else if (what == "spawn") {
        n = n ? n : 500'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::async::spawn(spawn_chain(n)).wait();
        report("spawn", bench::seconds_since(t0), n);
    } else if (what == "whenall") {
        n = n ? n : 500'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::async::spawn(whenall_loop(n)).wait();
        report("whenall", bench::seconds_since(t0), 2 * n);
    } else if (what == "timeout") {
        n = n ? n : 200'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::async::spawn(timeout_loop(n)).wait();
        report("timeout", bench::seconds_since(t0), n);
    } else if (what == "select") {
        n = n ? n : 500'000;
        sgcl::async::channel<int> ch(1);
        auto t0 = bench::Clock::now();
        sum = sgcl::async::spawn(select_loop(ch, n)).wait();
        report("select", bench::seconds_since(t0), n);
    } else if (what == "cv") {
        n = n ? n : 200'000;
        sgcl::async::mutex m;
        sgcl::async::condition_variable cv;
        int turn = 0;
        auto t0 = bench::Clock::now();
        auto a = sgcl::async::spawn(cv_side(m, cv, turn, 0, n));
        auto b = sgcl::async::spawn(cv_side(m, cv, turn, 1, n));
        a.wait();
        b.wait();
        report("cv", bench::seconds_since(t0), 2 * n);
    } else if (what == "pingpong") {
        n = n ? n : 500'000;
        sgcl::async::channel<int> a, b;
        auto t0 = bench::Clock::now();
        auto p = sgcl::async::spawn(ping(a, b, n));
        auto q = sgcl::async::spawn(pong(a, b, n));
        p.wait();
        q.wait();
        report("pingpong", bench::seconds_since(t0), 2 * n);
    } else if (what == "generator") {
        n = n ? n : 2'000'000;
        auto t0 = bench::Clock::now();
        sum = sgcl::async::spawn(consume(n)).wait();
        report("generator", bench::seconds_since(t0), n);
    } else if (what == "mutex") {
        n = n ? n : 2'000'000;
        sgcl::async::mutex m;
        auto t0 = bench::Clock::now();
        sum = sgcl::async::spawn(mutex_loop(m, n)).wait();
        report("mutex", bench::seconds_since(t0), n);
    } else if (what == "bcast" || what == "bcastth") {
        int k = argc > 4 ? std::atoi(argv[4]) : 16;
        long gap = argc > 5 ? std::atol(argv[5]) : 0;
        run_bcast(what == "bcastth", n ? n : (what == "bcast" ? (gap ? 5'000 : 100'000) : 200'000), k, gap);
    } else if (what == "notifyall") {
        int k = argc > 4 ? std::atoi(argv[4]) : 16;
        run_notifyall(n ? n : 100'000, k);
    } else if (what == "wgclose" || what == "wgcloseth") {
        int k = argc > 4 ? std::atoi(argv[4]) : 16;
        run_wgclose(what == "wgcloseth", n ? n : 150'000, k);
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    if (sum == -1) {
        std::printf("?");
    }
    sgcl::async::scheduler::stop();
    return 0;
}

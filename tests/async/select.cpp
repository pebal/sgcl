//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    template<class F>
    bool soon(F&& f) {
        auto until = std::chrono::steady_clock::now() + 5s;
        while (!f()) {
            if (std::chrono::steady_clock::now() > until) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    struct Job {
        int id;
        std::string name;
    };
}

TEST(Select_Test, DataOrStop) {
    sgcl::channel<int> jobs(4);
    sgcl::channel<void> stop;
    std::thread producer([&] {
        for (int i = 0; i < 1000; ++i) {
            jobs.send(i);
        }
        stop.send();
    });
    long sum = 0;
    int got = 0;
    bool running = true;
    while (running) {
        size_t i = sgcl::select(
            jobs.on_receive([&](int j) { sum += j; ++got; }),
            stop.on_receive([&] { running = false; })
        );
        EXPECT_LT(i, 2u);
    }
    producer.join();
    while (auto j = jobs.try_receive()) {   // sent before the stop, still buffered
        sum += *j;
        ++got;
    }
    EXPECT_EQ(got, 1000);
    EXPECT_EQ(sum, 999L * 1000 / 2);
}

TEST(Select_Test, SendCaseAndOtherwise) {
    sgcl::channel<int> out(2);
    int sent = 0, idle = 0;
    for (int i = 0; i < 5; ++i) {
        size_t k = sgcl::select(out.on_send(i, [&] { ++sent; }), sgcl::otherwise([&] { ++idle; }));
        EXPECT_EQ(k, i < 2 ? 0u : 1u);
    }
    EXPECT_EQ(sent, 2);
    EXPECT_EQ(idle, 3);
    EXPECT_EQ(*out.try_receive(), 0);
    // a send case without a body
    EXPECT_EQ(sgcl::select(out.on_send(7), sgcl::otherwise([] {})), 0u);
    EXPECT_EQ(out.size(), 2u);
    // a rendezvous: the send case waits for a receiver
    sgcl::channel<int> r;
    std::thread receiver([&] { EXPECT_EQ(*r.receive(), 9); });
    EXPECT_EQ(sgcl::select(r.on_send(9)), 0u);
    receiver.join();
}

TEST(Select_Test, AClosedChannelIsAlwaysReady) {
    sgcl::channel<int> c(2);
    sgcl::channel<void> never;
    c.send(1);
    c.close();
    int seen = 0, closes = 0;
    // what was sent before the close comes first; the body with optional sees the close
    for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(sgcl::select(c.on_receive([&](sgcl::optional<int> v) { v ? ++seen : ++closes; }), never.on_receive([] {})), 0u);
    }
    EXPECT_EQ(seen, 1);
    EXPECT_EQ(closes, 1);
    // a body taking the value is not called for the close: the index says it
    bool called = false;
    EXPECT_EQ(sgcl::select(c.on_receive([&](int) { called = true; }), never.on_receive([] {})), 0u);
    EXPECT_FALSE(called);
    // a send case on a closed channel: served at once, the body not run
    bool sent = false;
    EXPECT_EQ(sgcl::select(c.on_send(5, [&] { sent = true; }), never.on_receive([] {})), 0u);
    EXPECT_FALSE(sent);
    // a signal channel closed: the body runs (a signal and the close both end the wait)
    sgcl::channel<void> stop;
    stop.close();
    bool stopped = false;
    EXPECT_EQ(sgcl::select(never.on_receive([] {}), stop.on_receive([&] { stopped = true; })), 1u);
    EXPECT_TRUE(stopped);
}

TEST(Select_Test, CloseWakesASelectWaiting) {
    sgcl::channel<int> a, b;
    std::atomic<int> woke = {-1};
    std::thread waiter([&] {
        woke = (int)sgcl::select(a.on_receive([](sgcl::optional<int>) {}), b.on_receive([](sgcl::optional<int>) {}));
    });
    std::this_thread::sleep_for(20ms);
    b.close();
    waiter.join();
    EXPECT_EQ(woke, 1);
}

TEST(Select_Test, FairAmongTheReady) {
    sgcl::channel<int> a(64), b(64);
    for (int i = 0; i < 64; ++i) {
        a.send(i);
        b.send(i);
    }
    int from_a = 0, from_b = 0;
    for (int i = 0; i < 64; ++i) {
        sgcl::select(a.on_receive([&](int) { ++from_a; }), b.on_receive([&](int) { ++from_b; }));
    }
    EXPECT_GT(from_a, 8);   // random: both sides taken, not the first case every time
    EXPECT_GT(from_b, 8);
}

TEST(Select_Test, ManyThreadsOverManyChannels) {
    constexpr int Producers = 4, Consumers = 4, PerProducer = 5000;
    sgcl::channel<int> ch[3] = {sgcl::channel<int>(0), sgcl::channel<int>(1), sgcl::channel<int>(16)};
    sgcl::channel<void> stop;
    std::atomic<long> sum = {0};
    std::atomic<int> got = {0};
    std::vector<std::thread> threads;
    for (int p = 0; p < Producers; ++p) {
        threads.emplace_back([&, p] {
            for (int i = 0; i < PerProducer; ++i) {
                int v = p * PerProducer + i;
                sgcl::select(ch[0].on_send(v), ch[1].on_send(v), ch[2].on_send(v));   // whichever takes it first
            }
        });
    }
    for (int c = 0; c < Consumers; ++c) {
        threads.emplace_back([&] {
            bool running = true;
            while (running) {
                auto take = [&](int v) { sum += v; ++got; };
                sgcl::select(ch[0].on_receive(take), ch[1].on_receive(take), ch[2].on_receive(take), stop.on_receive([&] { running = false; }));
            }
        });
    }
    EXPECT_TRUE(soon([&] { return got == Producers * PerProducer; }));
    stop.close();
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(got, Producers * PerProducer);
    EXPECT_EQ(sum, (long)(Producers * PerProducer - 1) * (Producers * PerProducer) / 2);
}

TEST(Select_Test, CoroutinesSelect) {
    sgcl::channel<sgcl::tracked_ptr<Job>> jobs(2);
    sgcl::channel<int> results(2);
    sgcl::channel<void> stop;
    auto w = [](sgcl::channel<sgcl::tracked_ptr<Job>>& jobs, sgcl::channel<int>& results, sgcl::channel<void>& stop) -> sgcl::task<int> {
        int handled = 0;
        bool running = true;
        while (running) {
            sgcl::tracked_ptr<Job> got;
            co_await sgcl::async_select(
                jobs.on_receive([&](sgcl::tracked_ptr<Job> job) { got = job; }),
                stop.on_receive([&] { running = false; })
            );
            if (got) {
                ++handled;
                co_await results.async_send(got->id * 2);   // a wait after the select, from the task's own frame
            }
        }
        co_return handled;
    };
    std::vector<sgcl::task<int>> workers;
    for (int i = 0; i < 3; ++i) {
        workers.push_back(sgcl::spawn(w(jobs, results, stop)));
    }
    long sum = 0;
    std::thread collector([&] {
        for (int r : results) {
            sum += r;
        }
    });
    for (int i = 0; i < 300; ++i) {
        jobs.send(sgcl::make_tracked<Job>(i, "job"));
    }
    EXPECT_TRUE(soon([&] { return jobs.empty(); }));
    stop.close();
    int handled = 0;
    for (auto& t : workers) {
        handled += t.join();
    }
    results.close();
    collector.join();
    EXPECT_EQ(handled, 300);
    EXPECT_EQ(sum, 2L * 299 * 300 / 2);
    sgcl::scheduler::stop();          // the workers joined and the queue gone: the tests after count live objects from zero
}

TEST(Select_Test, ACoroutineSendCaseAndOtherwise) {
    sgcl::channel<int> out;
    sgcl::channel<int> in(1);
    auto t = sgcl::spawn([](sgcl::channel<int>& out, sgcl::channel<int>& in) -> sgcl::task<int> {
        int idle = 0;
        size_t k = co_await sgcl::async_select(out.on_send(1), sgcl::otherwise([&] { ++idle; }));   // no receiver: otherwise
        if (k != 1 || idle != 1) {
            co_return -1;
        }
        k = co_await sgcl::async_select(out.on_send(2), in.on_receive([](int) {}));   // waits until one is possible
        co_return (int)k;
    }(out, in));
    std::this_thread::sleep_for(20ms);
    in.send(5);
    EXPECT_EQ(t.join(), 1);
    // and the send case, served by a receiver
    auto u = sgcl::spawn([](sgcl::channel<int>& out, sgcl::channel<int>& in) -> sgcl::task<int> {
        co_return (int)co_await sgcl::async_select(out.on_send(3), in.on_receive([](int) {}));
    }(out, in));
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(*out.receive(), 3);
    EXPECT_EQ(u.join(), 0);
    sgcl::scheduler::stop();          // the workers joined and the queue gone: the tests after count live objects from zero
}

TEST(Select_Test, TheElementsOfASelectAreHeldAndReclaimed) {
    sgcl::channel<sgcl::tracked_ptr<Job>> a, b;
    std::atomic<bool> done = {false};
    std::thread t([&] {
        sgcl::select(a.on_send(sgcl::make_tracked<Job>(1, "a")), b.on_send(sgcl::make_tracked<Job>(2, "b")));   // both elements in waiters
        done = true;
    });
    std::this_thread::sleep_for(20ms);
    sgcl::collector::force_collect(true);   // the waiters hold the Jobs
    auto got = a.receive();
    EXPECT_TRUE(got && (*got)->id == 1);
    EXPECT_TRUE(soon([&] { return done.load(); }));
    t.join();
    got = nullptr;
    sgcl::collector::force_collect(true);   // the dead waiter of b and its Job: garbage
    EXPECT_FALSE(b.try_receive());
}

// As for a channel (channel.cpp: TheChannelMayGoOnceTheWaiterIsServed): a
// select that registered looks at its channels, and the channel that
// served it may be gone by then unless the look comes first
namespace {
    SGCL_NOINLINE void scribble() {
        volatile char buf[8192];
        for (auto& c : buf) {
            c = 0x5a;
        }
    }

    sgcl::task<> selects(sgcl::channel<int>& ch, sgcl::channel<int>& never, sgcl::atomic<int>& got) {
        co_await sgcl::async_select(ch.on_receive([&](int v) { got = v; }), never.on_receive([&](int) { got = -1; }));
    }

    SGCL_NOINLINE void serve_a_select(sgcl::atomic<int>& got) {
        sgcl::channel<int> ch, never;                    // on this stack
        sgcl::go(selects(ch, never, got));
        std::this_thread::sleep_for(50us);               // the select registered first, usually
        EXPECT_TRUE(ch.send(3));                         // served: both channels go with the return
    }
}

TEST(Select_Test, TheChannelMayGoOnceTheSelectIsServed) {
    for (int i = 0; i < 300; ++i) {
        sgcl::atomic<int> got = {0};
        std::thread th([&] {
            serve_a_select(got);
            scribble();
        });
        th.join();
        for (int j = 0; j < 5000 && got.load() == 0; ++j) {
            std::this_thread::sleep_for(1ms);
        }
        EXPECT_EQ(got.load(), 3);
    }
    sgcl::scheduler::stop();
}

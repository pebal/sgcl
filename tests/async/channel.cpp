//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    // A coroutine made ready runs on a worker of the scheduler, soon:
    // the test waits for what it expects, with a bound
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
}

TEST(Channel_Test, BufferedSendReceive) {
    sgcl::async::channel<int> ch(4);
    EXPECT_EQ(ch.capacity(), 4u);
    EXPECT_TRUE(ch.empty());
    EXPECT_FALSE(ch.try_receive());
    EXPECT_TRUE(ch.send(1).wait());
    int two = 2;
    EXPECT_TRUE(ch.send(two).wait());
    EXPECT_TRUE(ch.try_send(3));
    EXPECT_TRUE(ch.try_send(4));
    EXPECT_FALSE(ch.try_send(5));   // full
    EXPECT_EQ(ch.size(), 4u);
    EXPECT_FALSE(ch.empty());
    EXPECT_EQ(*ch.receive().wait(), 1);
    EXPECT_EQ(*ch.try_receive(), 2);
    EXPECT_EQ(*ch.receive().wait(), 3);
    EXPECT_EQ(*ch.receive().wait(), 4);
    EXPECT_FALSE(ch.try_receive());
    EXPECT_TRUE(ch.empty());
    EXPECT_FALSE(ch.closed());
}

TEST(Channel_Test, CloseDrainsThenEnds) {
    sgcl::async::channel<std::string> ch(8);
    ch.send("a").wait();
    ch.send("b").wait();
    ch.close();
    EXPECT_TRUE(ch.closed());
    EXPECT_FALSE(ch.send("c").wait());       // closed: refused
    EXPECT_FALSE(ch.try_send("c"));
    EXPECT_EQ(*ch.receive().wait(), "a");    // what was sent is still received
    EXPECT_EQ(*ch.receive().wait(), "b");
    EXPECT_FALSE(ch.receive().wait());       // then nothing, at once
    EXPECT_FALSE(ch.try_receive());
    ch.close();                       // again: nothing happens
    EXPECT_TRUE(ch.empty());
}

TEST(Channel_Test, RangeFor) {
    sgcl::async::channel<int> ch(2);
    std::thread p([&] {
        for (int i = 0; i < 100; ++i) {
            ch.send(i).wait();               // waits whenever the buffer of two is full
        }
        ch.close();
    });
    int sum = 0, count = 0, last = -1;
    for (int v : ch) {
        EXPECT_EQ(v, last + 1);       // the order of the sends
        last = v;
        sum += v;
        ++count;
    }
    p.join();
    EXPECT_EQ(count, 100);
    EXPECT_EQ(sum, 4950);
}

TEST(Channel_Test, RendezvousWaitsForTheReceiver) {
    sgcl::async::channel<int> ch;            // capacity 0
    EXPECT_EQ(ch.capacity(), 0u);
    EXPECT_FALSE(ch.try_send(1));     // no receiver waiting
    sgcl::atomic<bool> sent = {false};
    std::thread s([&] {
        ch.send(7).wait();
        sent = true;
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(sent.load());        // the send waits for the receive
    EXPECT_FALSE(ch.empty());         // a sender holds an element
    EXPECT_EQ(*ch.receive().wait(), 7);
    s.join();
    EXPECT_TRUE(sent.load());

    std::thread r([&] {
        EXPECT_EQ(*ch.receive().wait(), 8);
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(ch.try_send(8));      // a receiver waits: handed over at once
    r.join();
}

TEST(Channel_Test, BackPressure) {
    sgcl::async::channel<int> ch(2);
    sgcl::atomic<int> sent = {0};
    std::thread p([&] {
        for (int i = 0; i < 10; ++i) {
            ch.send(i).wait();
            ++sent;
        }
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(sent.load(), 2);        // two in the buffer, the third send waits
    EXPECT_EQ(*ch.receive().wait(), 0);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(sent.load(), 3);        // room for one more
    for (int i = 1; i < 10; ++i) {
        EXPECT_EQ(*ch.receive().wait(), i);
    }
    p.join();
    EXPECT_EQ(sent.load(), 10);
}

TEST(Channel_Test, CloseWakesWaitingReceivers) {
    sgcl::async::channel<int> ch(1);
    sgcl::atomic<int> got_nothing = {0};
    std::vector<std::thread> receivers;
    for (int i = 0; i < 4; ++i) {
        receivers.emplace_back([&] {
            if (!ch.receive().wait()) {      // waits: nothing to receive
                ++got_nothing;
            }
        });
    }
    std::this_thread::sleep_for(30ms);
    EXPECT_EQ(got_nothing.load(), 0);
    ch.close();
    for (auto& t : receivers) {
        t.join();
    }
    EXPECT_EQ(got_nothing.load(), 4);
    EXPECT_FALSE(ch.receive().wait());
}

TEST(Channel_Test, CloseRefusesWaitingSenders) {
    sgcl::async::channel<int> ch(1);
    ch.send(1).wait();                       // fills the buffer
    sgcl::atomic<int> refused = {0};
    std::vector<std::thread> senders;
    for (int i = 0; i < 3; ++i) {
        senders.emplace_back([&] {
            if (!ch.send(2).wait()) {        // waits: the buffer is full and nobody receives
                ++refused;
            }
        });
    }
    std::this_thread::sleep_for(30ms);
    EXPECT_EQ(refused.load(), 0);
    ch.close();
    for (auto& t : senders) {
        t.join();
    }
    EXPECT_EQ(refused.load(), 3);     // the elements in their hands were not delivered
    EXPECT_EQ(*ch.receive().wait(), 1);      // the buffer is drained
    EXPECT_FALSE(ch.receive().wait());
}

// A send hands its element to a waiting receiver whatever the buffer:
// four receivers waiting, four sends into a channel of capacity one
TEST(Channel_Test, WaitingReceiversAreServedDirectly) {
    sgcl::async::channel<int> ch(1);
    sgcl::atomic<int> served = {0};
    std::vector<std::thread> receivers;
    for (int i = 0; i < 4; ++i) {
        receivers.emplace_back([&] {
            if (ch.receive().wait()) {
                ++served;
            }
        });
    }
    std::this_thread::sleep_for(30ms);
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(ch.send(i).wait());      // none of them waits
    }
    for (auto& t : receivers) {
        t.join();
    }
    EXPECT_EQ(served.load(), 4);
    EXPECT_TRUE(ch.empty());
}

TEST(Channel_Test, ManyProducersManyConsumers) {
    const int producers = 4, consumers = 4, n = 5000;
    sgcl::async::channel<int> ch(16);
    std::vector<sgcl::atomic<int>> seen(size_t(producers * n));
    sgcl::atomic<bool> out_of_order = {false};
    std::vector<std::thread> ws;
    for (int p = 0; p < producers; ++p) {
        ws.emplace_back([&, p] {
            for (int i = 0; i < n; ++i) {
                ch.send(p * n + i).wait();
            }
        });
    }
    std::vector<std::thread> cs;
    for (int c = 0; c < consumers; ++c) {
        cs.emplace_back([&] {
            std::vector<int> last(producers, -1);
            while (auto v = ch.receive().wait()) {
                int p = *v / n;
                if (*v <= last[size_t(p)]) {
                    out_of_order = true;
                }
                last[size_t(p)] = *v;
                ++seen[size_t(*v)];
            }
        });
    }
    for (auto& w : ws) {
        w.join();
    }
    ch.close();
    for (auto& c : cs) {
        c.join();
    }
    for (auto& s : seen) {
        ASSERT_EQ(s.load(), 1);       // every element received once
    }
    EXPECT_FALSE(out_of_order.load());   // and in each producer's order
}

TEST(Channel_Test, ElementsHeldAndReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::async::channel<tracked_ptr<Baz>> ch(100);
    const size_t empty = collector::get_live_object_count();   // the ring and the two waiter queues' first nodes
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            ch.send(make_tracked<Baz>(i)).wait();
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), empty + 100u);   // 100 Baz in the ring, nothing allocated for them
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            auto v = ch.receive().wait();
            ASSERT_TRUE(v);
            EXPECT_EQ((*v)->value, i);
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), empty);
    EXPECT_GE(empty, before + 3u);
}

namespace {
    // A coroutine that receives from one channel and sends to another
    // until the first closes, then closes the second
    sgcl::async::task<> worker(sgcl::async::channel<tracked_ptr<Baz>>& in, sgcl::async::channel<int>& out) {
        while (auto job = co_await in.receive()) {
            if (!co_await out.send((*job)->value * 2)) {
                break;
            }
        }
        out.close();
    }

    sgcl::async::task<int> summer(sgcl::async::channel<int>& in) {
        int sum = 0;
        while (auto v = co_await in.receive()) {
            sum += *v;
        }
        co_return sum;
    }
}

TEST(Channel_Test, CoroutinePipeline) {
    sgcl::async::channel<tracked_ptr<Baz>> jobs(4);
    sgcl::async::channel<int> results(4);
    sgcl::async::task<> w = sgcl::async::spawn(worker(jobs, results));   // on the scheduler: to its first co_await, waiting on jobs
    sgcl::async::task<int> s = sgcl::async::spawn(summer(results));      // waiting on results
    std::thread producer([&] {
        for (int i = 0; i < 100; ++i) {
            jobs.send(make_tracked<Baz>(i)).wait();   // each send makes the worker ready; a worker thread runs it
        }
        jobs.close();
    });
    producer.join();
    w.wait();
    EXPECT_EQ(s.wait(), 9900);
    EXPECT_TRUE(results.closed());
    sgcl::async::scheduler::stop();          // the workers joined and the queue gone: the tests after count live objects from zero
}

TEST(Channel_Test, CoroutineSuspendedOnFullChannelAndOnClose) {
    sgcl::async::channel<int> out(1);
    auto sender = [](sgcl::async::channel<int>& ch, int n, sgcl::atomic<int>& sent) -> sgcl::async::task<> {
        for (int i = 0; i < n; ++i) {
            if (!co_await ch.send(i)) {
                co_return;
            }
            ++sent;
        }
    };
    sgcl::atomic<int> sent = {0};
    sgcl::async::task<> t = sgcl::async::spawn(sender(out, 5, sent));
    EXPECT_TRUE(soon([&] { return sent.load() == 1; }));   // one in the buffer, the second send suspended
    EXPECT_FALSE(t.done());
    EXPECT_EQ(*out.receive().wait(), 0);     // the receive makes the coroutine ready: it sends the next and suspends again
    EXPECT_TRUE(soon([&] { return sent.load() == 2; }));
    EXPECT_EQ(*out.receive().wait(), 1);
    EXPECT_EQ(*out.receive().wait(), 2);
    EXPECT_EQ(*out.receive().wait(), 3);
    EXPECT_TRUE(soon([&] { return sent.load() == 5; }));
    EXPECT_EQ(*out.receive().wait(), 4);
    t.wait();

    sgcl::async::channel<int> in;
    auto receiver = [](sgcl::async::channel<int>& ch, sgcl::atomic<int>& state) -> sgcl::async::task<> {
        auto v = co_await ch.receive();
        state = v ? 1 : 2;
    };
    sgcl::atomic<int> state = {0};
    sgcl::async::task<> r = sgcl::async::spawn(receiver(in, state));
    EXPECT_EQ(state.load(), 0);       // suspended on the empty channel
    in.close();                       // wakes it with nothing
    r.wait();
    EXPECT_EQ(state.load(), 2);

    sgcl::async::channel<int> full(1);
    full.send(1).wait();
    sgcl::atomic<int> sent2 = {0};
    sgcl::async::task<> t2 = sgcl::async::spawn(sender(full, 1, sent2));   // suspended: full
    EXPECT_EQ(sent2.load(), 0);
    full.close();                     // refused
    t2.wait();
    EXPECT_EQ(sent2.load(), 0);
    sgcl::async::scheduler::stop();
}

TEST(Channel_Test, SignalChannel) {
    sgcl::async::channel<void> sig(1);
    EXPECT_FALSE(sig.try_receive());
    EXPECT_TRUE(sig.send().wait());
    EXPECT_TRUE(sig.receive().wait());
    std::thread t([&] {
        std::this_thread::sleep_for(10ms);
        sig.send().wait();
    });
    EXPECT_TRUE(sig.receive().wait());       // waits for it
    t.join();
    sig.close();
    EXPECT_FALSE(sig.receive().wait());
    EXPECT_FALSE(sig.send().wait());

    sgcl::async::channel<void> done;
    auto waiter = [](sgcl::async::channel<void>& ch, sgcl::atomic<int>& got) -> sgcl::async::task<> {
        got = co_await ch.receive() ? 1 : 2;
    };
    sgcl::atomic<int> got = {0};
    sgcl::async::task<> w = sgcl::async::spawn(waiter(done, got));
    EXPECT_EQ(got.load(), 0);
    done.send().wait();                      // a rendezvous with the coroutine
    w.wait();
    EXPECT_EQ(got.load(), 1);
    sgcl::async::scheduler::stop();
}

TEST(Channel_Test, InsideManagedObject) {
    struct Holder {
        sgcl::async::channel<int> ch{2};
    };
    tracked_ptr h = make_tracked<Holder>();
    h->ch.send(1).wait();
    EXPECT_EQ(*h->ch.receive().wait(), 1);
}

// The channel may go as soon as the side it served is done with it: the
// coroutine on the other side may still be inside its suspension, and the
// look it takes at the channel after registering must not read a channel
// that is gone. The channel lives on a thread's stack, in a function that
// serves the coroutine and returns; the thread then calls a function of
// the same depth that writes over that stack: what the old look read
// (under TSan, a race between the look and those writes; in release, a
// pointer of the channel read as whatever the writes left)
namespace {
    SGCL_NOINLINE void scribble() {
        volatile char buf[8192];
        for (auto& c : buf) {
            c = 0x5a;
        }
    }

    sgcl::async::task<> sends(sgcl::async::channel<int>& ch, sgcl::atomic<int>& done) {
        done = co_await ch.send(1) ? 1 : -1;
    }

    sgcl::async::task<> receives(sgcl::async::channel<int>& ch, sgcl::atomic<int>& got) {
        auto v = co_await ch.receive();
        got = v ? *v : -1;
    }

    SGCL_NOINLINE void serve_a_sender(sgcl::atomic<int>& done) {
        sgcl::async::channel<int> ch;                           // a rendezvous on this stack
        sgcl::async::go(sends(ch, done));
        std::this_thread::sleep_for(50us);               // the sender registered first, usually
        EXPECT_EQ(*ch.receive().wait(), 1);                     // served: the channel goes with the return
    }

    SGCL_NOINLINE void serve_a_receiver(sgcl::atomic<int>& got) {
        sgcl::async::channel<int> ch;
        sgcl::async::go(receives(ch, got));
        std::this_thread::sleep_for(50us);
        EXPECT_TRUE(ch.send(2).wait());
    }
}

TEST(Channel_Test, TheChannelMayGoOnceTheWaiterIsServed) {
    for (int i = 0; i < 300; ++i) {
        sgcl::atomic<int> done = {0}, got = {0};
        std::thread th([&] {
            serve_a_sender(done);
            scribble();
            serve_a_receiver(got);
            scribble();
        });
        th.join();
        for (int j = 0; j < 5000 && (done.load() == 0 || got.load() == 0); ++j) {
            std::this_thread::sleep_for(1ms);
        }
        EXPECT_EQ(done.load(), 1);
        EXPECT_EQ(got.load(), 2);
    }
    sgcl::async::scheduler::stop();
}

// A rendezvous sender is released by the receive that takes its element
// and by nothing else: with two senders waiting, one receive releases
// exactly one of them (before, the receive that took the first element
// moved the second's into the ring and released it too, its element
// taken by nobody yet)
TEST(Channel_Test, ARendezvousReleasesASenderOnlyWhenItsElementIsTaken) {
    sgcl::async::channel<int> ch;                                   // capacity 0
    sgcl::atomic<int> sent = {0};
    auto sender = [](sgcl::async::channel<int>& ch, sgcl::atomic<int>& sent, int v) -> sgcl::async::task<> {
        co_await ch.send(v);
        ++sent;
    };
    auto a = sgcl::async::spawn(sender(ch, sent, 1));
    auto b = sgcl::async::spawn(sender(ch, sent, 2));
    std::this_thread::sleep_for(10ms);                       // both waiting
    EXPECT_EQ(sent.load(), 0);
    EXPECT_TRUE(ch.receive().wait().has_value());                   // one taken
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(sent.load(), 1);                               // one released, the other still waiting with its element
    EXPECT_TRUE(ch.receive().wait().has_value());
    a.wait();
    b.wait();
    EXPECT_EQ(sent.load(), 2);
    sgcl::async::scheduler::stop();
}

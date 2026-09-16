//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
}

TEST(Channel_Test, BufferedSendReceive) {
    sgcl::channel<int> ch(4);
    EXPECT_EQ(ch.capacity(), 4u);
    EXPECT_TRUE(ch.empty());
    EXPECT_FALSE(ch.try_receive());
    EXPECT_TRUE(ch.send(1));
    int two = 2;
    EXPECT_TRUE(ch.send(two));
    EXPECT_TRUE(ch.try_send(3));
    EXPECT_TRUE(ch.try_send(4));
    EXPECT_FALSE(ch.try_send(5));   // full
    EXPECT_EQ(ch.size(), 4u);
    EXPECT_FALSE(ch.empty());
    EXPECT_EQ(*ch.receive(), 1);
    EXPECT_EQ(*ch.try_receive(), 2);
    EXPECT_EQ(*ch.receive(), 3);
    EXPECT_EQ(*ch.receive(), 4);
    EXPECT_FALSE(ch.try_receive());
    EXPECT_TRUE(ch.empty());
    EXPECT_FALSE(ch.closed());
}

TEST(Channel_Test, CloseDrainsThenEnds) {
    sgcl::channel<std::string> ch(8);
    ch.send("a");
    ch.send("b");
    ch.close();
    EXPECT_TRUE(ch.closed());
    EXPECT_FALSE(ch.send("c"));       // closed: refused
    EXPECT_FALSE(ch.try_send("c"));
    EXPECT_EQ(*ch.receive(), "a");    // what was sent is still received
    EXPECT_EQ(*ch.receive(), "b");
    EXPECT_FALSE(ch.receive());       // then nothing, at once
    EXPECT_FALSE(ch.try_receive());
    ch.close();                       // again: nothing happens
    EXPECT_TRUE(ch.empty());
}

TEST(Channel_Test, RangeFor) {
    sgcl::channel<int> ch(2);
    std::thread p([&] {
        for (int i = 0; i < 100; ++i) {
            ch.send(i);               // waits whenever the buffer of two is full
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
    sgcl::channel<int> ch;            // capacity 0
    EXPECT_EQ(ch.capacity(), 0u);
    EXPECT_FALSE(ch.try_send(1));     // no receiver waiting
    sgcl::atomic<bool> sent = {false};
    std::thread s([&] {
        ch.send(7);
        sent = true;
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(sent.load());        // the send waits for the receive
    EXPECT_FALSE(ch.empty());         // a sender holds an element
    EXPECT_EQ(*ch.receive(), 7);
    s.join();
    EXPECT_TRUE(sent.load());

    std::thread r([&] {
        EXPECT_EQ(*ch.receive(), 8);
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(ch.try_send(8));      // a receiver waits: handed over at once
    r.join();
}

TEST(Channel_Test, BackPressure) {
    sgcl::channel<int> ch(2);
    sgcl::atomic<int> sent = {0};
    std::thread p([&] {
        for (int i = 0; i < 10; ++i) {
            ch.send(i);
            ++sent;
        }
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(sent.load(), 2);        // two in the buffer, the third send waits
    EXPECT_EQ(*ch.receive(), 0);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(sent.load(), 3);        // room for one more
    for (int i = 1; i < 10; ++i) {
        EXPECT_EQ(*ch.receive(), i);
    }
    p.join();
    EXPECT_EQ(sent.load(), 10);
}

TEST(Channel_Test, CloseWakesWaitingReceivers) {
    sgcl::channel<int> ch(1);
    sgcl::atomic<int> got_nothing = {0};
    std::vector<std::thread> receivers;
    for (int i = 0; i < 4; ++i) {
        receivers.emplace_back([&] {
            if (!ch.receive()) {      // waits: nothing to receive
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
    EXPECT_FALSE(ch.receive());
}

TEST(Channel_Test, CloseRefusesWaitingSenders) {
    sgcl::channel<int> ch(1);
    ch.send(1);                       // fills the buffer
    sgcl::atomic<int> refused = {0};
    std::vector<std::thread> senders;
    for (int i = 0; i < 3; ++i) {
        senders.emplace_back([&] {
            if (!ch.send(2)) {        // waits: the buffer is full and nobody receives
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
    EXPECT_EQ(*ch.receive(), 1);      // the buffer is drained
    EXPECT_FALSE(ch.receive());
}

// A send hands its element to a waiting receiver whatever the buffer:
// four receivers waiting, four sends into a channel of capacity one
TEST(Channel_Test, WaitingReceiversAreServedDirectly) {
    sgcl::channel<int> ch(1);
    sgcl::atomic<int> served = {0};
    std::vector<std::thread> receivers;
    for (int i = 0; i < 4; ++i) {
        receivers.emplace_back([&] {
            if (ch.receive()) {
                ++served;
            }
        });
    }
    std::this_thread::sleep_for(30ms);
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(ch.send(i));      // none of them waits
    }
    for (auto& t : receivers) {
        t.join();
    }
    EXPECT_EQ(served.load(), 4);
    EXPECT_TRUE(ch.empty());
}

TEST(Channel_Test, ManyProducersManyConsumers) {
    const int producers = 4, consumers = 4, n = 5000;
    sgcl::channel<int> ch(16);
    std::vector<sgcl::atomic<int>> seen(size_t(producers * n));
    sgcl::atomic<bool> out_of_order = {false};
    std::vector<std::thread> ws;
    for (int p = 0; p < producers; ++p) {
        ws.emplace_back([&, p] {
            for (int i = 0; i < n; ++i) {
                ch.send(p * n + i);
            }
        });
    }
    std::vector<std::thread> cs;
    for (int c = 0; c < consumers; ++c) {
        cs.emplace_back([&] {
            std::vector<int> last(producers, -1);
            while (auto v = ch.receive()) {
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
    sgcl::channel<tracked_ptr<Baz>> ch(100);
    const size_t empty = collector::get_live_object_count();   // the ring and the two waiter queues' first nodes
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            ch.send(make_tracked<Baz>(i));
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), empty + 100u);   // 100 Baz in the ring, nothing allocated for them
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            auto v = ch.receive();
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
    sgcl::task<> worker(sgcl::channel<tracked_ptr<Baz>>& in, sgcl::channel<int>& out) {
        while (auto job = co_await in.async_receive()) {
            if (!co_await out.async_send((*job)->value * 2)) {
                break;
            }
        }
        out.close();
    }

    sgcl::task<int> summer(sgcl::channel<int>& in) {
        int sum = 0;
        while (auto v = co_await in.async_receive()) {
            sum += *v;
        }
        co_return sum;
    }
}

TEST(Channel_Test, CoroutinePipeline) {
    sgcl::channel<tracked_ptr<Baz>> jobs(4);
    sgcl::channel<int> results(4);
    sgcl::task<> w = worker(jobs, results);
    sgcl::task<int> s = summer(results);
    w.resume();                       // to its first co_await: waiting on jobs
    s.resume();                       // waiting on results
    EXPECT_FALSE(w.done());
    std::thread producer([&] {
        for (int i = 0; i < 100; ++i) {
            jobs.send(make_tracked<Baz>(i));   // each send resumes the worker on this thread
        }
        jobs.close();
    });
    producer.join();
    EXPECT_TRUE(w.done());
    EXPECT_TRUE(s.done());
    EXPECT_EQ(s.result(), 9900);
    EXPECT_TRUE(results.closed());
}

TEST(Channel_Test, CoroutineSuspendedOnFullChannelAndOnClose) {
    sgcl::channel<int> out(1);
    auto sender = [](sgcl::channel<int>& ch, int n, sgcl::atomic<int>& sent) -> sgcl::task<> {
        for (int i = 0; i < n; ++i) {
            if (!co_await ch.async_send(i)) {
                co_return;
            }
            ++sent;
        }
    };
    sgcl::atomic<int> sent = {0};
    sgcl::task<> t = sender(out, 5, sent);
    t.resume();
    EXPECT_EQ(sent.load(), 1);        // one in the buffer, the second send suspended
    EXPECT_FALSE(t.done());
    EXPECT_EQ(*out.receive(), 0);     // the receive resumes the coroutine: it sends the next and suspends again
    EXPECT_EQ(sent.load(), 2);
    EXPECT_EQ(*out.receive(), 1);
    EXPECT_EQ(*out.receive(), 2);
    EXPECT_EQ(*out.receive(), 3);
    EXPECT_EQ(sent.load(), 5);
    EXPECT_EQ(*out.receive(), 4);
    EXPECT_TRUE(t.done());

    sgcl::channel<int> in;
    auto receiver = [](sgcl::channel<int>& ch, sgcl::atomic<int>& state) -> sgcl::task<> {
        auto v = co_await ch.async_receive();
        state = v ? 1 : 2;
    };
    sgcl::atomic<int> state = {0};
    sgcl::task<> r = receiver(in, state);
    r.resume();
    EXPECT_EQ(state.load(), 0);       // suspended on the empty channel
    in.close();                       // wakes it with nothing
    EXPECT_EQ(state.load(), 2);
    EXPECT_TRUE(r.done());

    sgcl::channel<int> full(1);
    full.send(1);
    sgcl::atomic<int> sent2 = {0};
    sgcl::task<> t2 = sender(full, 1, sent2);
    t2.resume();                      // suspended: full
    EXPECT_EQ(sent2.load(), 0);
    full.close();                     // refused
    EXPECT_TRUE(t2.done());
    EXPECT_EQ(sent2.load(), 0);
}

TEST(Channel_Test, SignalChannel) {
    sgcl::channel<void> sig(1);
    EXPECT_FALSE(sig.try_receive());
    EXPECT_TRUE(sig.send());
    EXPECT_TRUE(sig.receive());
    std::thread t([&] {
        std::this_thread::sleep_for(10ms);
        sig.send();
    });
    EXPECT_TRUE(sig.receive());       // waits for it
    t.join();
    sig.close();
    EXPECT_FALSE(sig.receive());
    EXPECT_FALSE(sig.send());

    sgcl::channel<void> done;
    auto waiter = [](sgcl::channel<void>& ch, sgcl::atomic<int>& got) -> sgcl::task<> {
        got = co_await ch.async_receive() ? 1 : 2;
    };
    sgcl::atomic<int> got = {0};
    sgcl::task<> w = waiter(done, got);
    w.resume();
    EXPECT_EQ(got.load(), 0);
    done.send();                      // a rendezvous with the coroutine
    EXPECT_EQ(got.load(), 1);
}

TEST(Channel_Test, InsideManagedObject) {
    struct Holder {
        sgcl::channel<int> ch{2};
    };
    tracked_ptr h = make_tracked<Holder>();
    h->ch.send(1);
    EXPECT_EQ(*h->ch.receive(), 1);
}

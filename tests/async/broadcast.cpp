//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Item {
        explicit Item(int v)
        : value(v) {
            ++alive;
        }

        ~Item() {
            value = -1;
            --alive;
        }

        int value;
        inline static std::atomic<int> alive = {0};
    };

    void settle() {
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // A subscriber task: every value until the close, in order, the lag summed
    struct Seen {
        std::vector<int> values;
        size_t lagged = 0;
    };

    sgcl::async::task<Seen> drain(sgcl::async::broadcast<int>::subscription s) {
        Seen seen;
        while (auto v = co_await s.receive()) {
            seen.lagged += s.lagged();
            seen.values.push_back(*v);
        }
        co_return seen;
    }

    // The wake of many (scheduler.h: WakeBatch): a subscriber that
    // acknowledges every value through a wait group, and a sender that
    // sends the next value only when every subscriber has this one, so
    // that every value finds every subscriber waiting and one walk of the
    // sender wakes them all. A subscriber counts what is wrong: a value
    // out of order or lost, a run of a strand's task beside another one
    // of the strand, a task found on a worker when it started on an
    // executor's thread or the other way round, and a subscriber that
    // did not see every value. A subscription's first value is the first
    // sent after it is made, so the subscriptions are made before the
    // tasks.
    struct Acks {
        sgcl::async::wait_group back;
        sgcl::atomic<int> inside = {0};   // the strand's subscribers running now
    };

    sgcl::async::task<int> acknowledging(sgcl::async::broadcast<int>::subscription s, Acks& acks, int rounds, sgcl::async::strand* st) {
        if (st) {
            co_await sgcl::async::on(*st);
        }
        const bool on_worker = sgcl::async::scheduler::on_worker();   // an executor's task stays on the executor's thread
        int expected = 0, wrong = 0;
        while (auto v = co_await s.receive()) {
            if (*v != expected++ || s.lagged() || sgcl::async::scheduler::on_worker() != on_worker) {
                ++wrong;
            }
            if (st) {
                if (acks.inside.fetch_add(1) != 0) {
                    ++wrong;
                }
                for (int i = 0; i < 200; ++i) {   // a while on the strand, for another run of it to overlap if one could
                    async::detail::os::spin_pause();
                }
                acks.inside.fetch_sub(1);
            }
            acks.back.done();
        }
        co_return expected == rounds ? wrong : wrong + 1;
    }

    sgcl::async::task<> acknowledged(sgcl::async::broadcast<int>& b, Acks& acks, int subscribers, int rounds) {
        for (int r = 0; r < rounds; ++r) {
            acks.back.add(subscribers);
            b.send(r);
            co_await acks.back;
        }
        b.close();
    }

    // k subscribers on the workers, `on_strand` of them moved to one
    // strand, and one run by an executor on a thread of its own; the
    // values sent by a task (the walk on a worker, to its ring) or by
    // this thread (to the global queue)
    void every_subscriber_every_round(int k, int on_strand, bool with_executor, bool from_task, int rounds) {
        sgcl::async::broadcast<int> b(16);
        Acks acks;
        sgcl::async::strand st;
        std::vector<sgcl::async::task<int>> subscribers;
        for (int i = 0; i < k; ++i) {
            subscribers.push_back(sgcl::async::spawn(acknowledging(b.subscribe(), acks, rounds, i < on_strand ? &st : nullptr)));
        }
        sgcl::async::executor ex;
        std::atomic<int> executor_wrong = {-1};
        std::thread executor_thread;
        sgcl::async::broadcast<int>::subscription executor_subscription;   // made here, on a stack (a tracked_ptr lives on a stack or in a managed object, not in the thread's closure)
        if (with_executor) {
            executor_subscription = b.subscribe();
            executor_thread = std::thread([&] {
                executor_wrong = ex.run(acknowledging(std::move(executor_subscription), acks, rounds, nullptr));
            });
        }
        int all = k + (with_executor ? 1 : 0);
        if (from_task) {
            sgcl::async::spawn(acknowledged(b, acks, all, rounds)).wait();
        } else {
            for (int r = 0; r < rounds; ++r) {
                acks.back.add(all);
                b.send(r);
                acks.back.wait();
            }
            b.close();
        }
        for (auto& t : subscribers) {
            EXPECT_EQ(t.wait(), 0);
        }
        if (with_executor) {
            executor_thread.join();
            EXPECT_EQ(executor_wrong.load(), 0);
        }
        sgcl::async::scheduler::stop();
    }
}

TEST(Broadcast_Test, ThreeSubscribersReceiveEveryValueInOrder) {
    sgcl::async::broadcast<int> b(1024);
    EXPECT_EQ(b.capacity(), 1024u);
    std::vector<sgcl::async::task<Seen>> subscribers;
    for (int i = 0; i < 3; ++i) {
        subscribers.push_back(sgcl::async::spawn(drain(b.subscribe())));
    }
    EXPECT_EQ(b.subscribers(), 3u);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(b.send(i));
    }
    b.close();
    EXPECT_FALSE(b.send(1000));
    for (auto& t : subscribers) {
        Seen& seen = t.wait();
        ASSERT_EQ(seen.values.size(), 1000u);
        for (int i = 0; i < 1000; ++i) {
            EXPECT_EQ(seen.values[i], i);
        }
        EXPECT_EQ(seen.lagged, 0u);
    }
    subscribers.clear();                   // the tasks' subscriptions go with their frames
    EXPECT_EQ(b.subscribers(), 0u);
    sgcl::async::scheduler::stop();
}

TEST(Broadcast_Test, ASlowSubscriberIsLapped) {
    sgcl::async::broadcast<int> b(8);
    auto slow = b.subscribe();
    auto quick = b.subscribe();
    for (int i = 0; i < 20; ++i) {
        b.send(i);
        EXPECT_EQ(*quick.receive().wait(), i);    // keeps up: never lapped
    }
    EXPECT_EQ(quick.lagged(), 0u);
    // the ring holds the last eight: 12..19; the twelve before are lost
    auto v = slow.receive().wait();
    ASSERT_TRUE(v);
    EXPECT_EQ(*v, 12);
    EXPECT_EQ(slow.lagged(), 12u);
    for (int i = 13; i < 20; ++i) {
        EXPECT_EQ(*slow.receive().wait(), i);
        EXPECT_EQ(slow.lagged(), 0u);      // the lag is reported with the first value after it
    }
    EXPECT_FALSE(slow.try_receive());
    // lapped twice over: only the last lap counts
    for (int i = 20; i < 60; ++i) {
        b.send(i);
    }
    EXPECT_EQ(*slow.try_receive(), 52);
    EXPECT_EQ(slow.lagged(), 32u);
}

TEST(Broadcast_Test, ALateSubscriberStartsAtItsSubscription) {
    sgcl::async::broadcast<int> b(16);
    EXPECT_TRUE(b.send(0));                // nobody: dropped
    auto first = b.subscribe();
    for (int i = 1; i <= 5; ++i) {
        b.send(i);
    }
    auto late = b.subscribe();
    for (int i = 6; i <= 8; ++i) {
        b.send(i);
    }
    std::vector<int> seen;
    while (auto v = first.try_receive()) {
        seen.push_back(*v);
    }
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}));
    seen.clear();
    while (auto v = late.try_receive()) {
        seen.push_back(*v);
    }
    EXPECT_EQ(seen, (std::vector<int>{6, 7, 8}));
    EXPECT_EQ(late.lagged(), 0u);
}

TEST(Broadcast_Test, CloseEndsEverySubscription) {
    sgcl::async::broadcast<int> b(4);
    std::vector<sgcl::async::task<Seen>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(sgcl::async::spawn(drain(b.subscribe())));
    }
    auto s = b.subscribe();
    std::atomic<int> got = {-1};
    std::thread th([&] {
        auto v = s.receive().wait();              // blocks until the close
        got = v ? *v : -2;
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(got, -1);
    b.send(7);
    b.close();
    EXPECT_TRUE(b.closed());
    th.join();
    EXPECT_EQ(got, 7);                     // sent before the close: still received
    EXPECT_FALSE(s.receive().wait());             // then nothing
    EXPECT_TRUE(s.closed());
    for (auto& t : tasks) {
        Seen& seen = t.wait();
        EXPECT_EQ(seen.values, std::vector<int>{7});
    }
    auto after = b.subscribe();            // a subscription made after the close: nothing
    EXPECT_FALSE(after.receive().wait());
    EXPECT_FALSE(after.try_receive());
    sgcl::async::scheduler::stop();
}

TEST(Broadcast_Test, ASelectCase) {
    sgcl::async::broadcast<int> b(4);
    auto s = b.subscribe();
    int got = 0;
    // nothing there: the otherwise
    EXPECT_EQ(sgcl::async::select(s.on_receive([&](int v) { got = v; }), sgcl::async::otherwise([] {})).wait(), 1u);
    b.send(5);
    EXPECT_EQ(sgcl::async::select(s.on_receive([&](int v) { got = v; }), sgcl::async::otherwise([] {})).wait(), 0u);
    EXPECT_EQ(got, 5);
    // a task waits in a select until the value comes
    auto t = sgcl::async::spawn([](sgcl::async::broadcast<int>::subscription& s) -> sgcl::async::task<int> {
        int got = 0;
        size_t i = co_await sgcl::async::select(s.on_receive([&](int v) { got = v; }));
        co_return i == 0 ? got : -1;
    }(s));
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(t.done());
    b.send(6);
    EXPECT_EQ(t.wait(), 6);
    // the closed broadcast serves the case with nothing
    b.close();
    bool nothing = false;
    EXPECT_EQ(sgcl::async::select(s.on_receive([&](sgcl::optional<int> v) { nothing = !v; })).wait(), 0u);
    EXPECT_TRUE(nothing);
    sgcl::async::scheduler::stop();
}

TEST(Broadcast_Test, ValuesHeldUntilEverySubscriberPassedThem) {
    settle();
    const int before = Item::alive.load();
    sgcl::async::broadcast<tracked_ptr<Item>> b(16);
    auto first = b.subscribe();
    auto second = b.subscribe();
    off_frame([&] {
        for (int i = 0; i < 4; ++i) {
            b.send(make_tracked<Item>(i));
        }
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 4);   // held by the ring
    off_frame([&] {
        for (int i = 0; i < 4; ++i) {
            auto v = first.receive().wait();
            ASSERT_TRUE(v);
            EXPECT_EQ((*v)->value, i);
        }
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 4);   // the second has not passed them
    off_frame([&] {
        EXPECT_EQ((*second.receive().wait())->value, 0);
        EXPECT_EQ((*second.receive().wait())->value, 1);
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 2);   // the two both passed: gone, though their slots are not overwritten
    off_frame([&] {
        auto dropped = std::move(second);        // a subscription dropped counts itself off what it has not passed
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before);
    EXPECT_EQ(b.subscribers(), 1u);
    // lapped values go when their slots are overwritten
    off_frame([&] {
        for (int i = 0; i < 40; ++i) {
            b.send(make_tracked<Item>(i));
        }
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 16);  // the ring's sixteen
    off_frame([&] {
        auto v = first.receive().wait();
        EXPECT_EQ((*v)->value, 24);
        EXPECT_EQ(first.lagged(), 24u);
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 15);
}

TEST(Broadcast_Test, EightSendersEightSubscribers) {
    sgcl::async::broadcast<int> b(256);
    constexpr int Senders = 8, PerSender = 2000;
    // a subscriber counts what it got and what it lost, and checks the
    // order of each sender's values (a value is sender * PerSender + k)
    auto subscriber = [](sgcl::async::broadcast<int>::subscription s) -> sgcl::async::task<Seen> {
        Seen seen;
        std::vector<int> last(Senders, -1);
        while (auto v = co_await s.receive()) {
            seen.lagged += s.lagged();
            int sender = *v / PerSender, k = *v % PerSender;
            if (k <= last[sender]) {
                seen.values.push_back(-1);   // out of order: a failure
            }
            last[sender] = k;
            seen.values.push_back(*v);
        }
        co_return seen;
    };
    std::vector<sgcl::async::task<Seen>> subscribers;
    for (int i = 0; i < 8; ++i) {
        subscribers.push_back(sgcl::async::spawn(subscriber(b.subscribe())));
    }
    std::vector<std::thread> senders;
    for (int s = 0; s < Senders; ++s) {
        senders.emplace_back([&, s] {
            for (int k = 0; k < PerSender; ++k) {
                b.send(s * PerSender + k);
            }
        });
    }
    for (auto& t : senders) {
        t.join();
    }
    b.close();
    for (auto& t : subscribers) {
        Seen& seen = t.wait();
        EXPECT_EQ(seen.values.size() + seen.lagged, (size_t)Senders * PerSender);   // every value received or counted lost
        for (int v : seen.values) {
            EXPECT_NE(v, -1);
        }
    }
    sgcl::async::scheduler::stop();
}

TEST(Broadcast_Test, InsideAManagedObjectWithTaskSubscribers) {
    struct Bus {
        sgcl::async::broadcast<tracked_ptr<Item>> events{16};
    };
    settle();
    const int before = Item::alive.load();
    tracked_ptr bus = make_tracked<Bus>();
    std::vector<sgcl::async::task<int>> listeners;
    for (int i = 0; i < 4; ++i) {
        listeners.push_back(sgcl::async::spawn([](tracked_ptr<Bus> bus) -> sgcl::async::task<int> {
            auto s = bus->events.subscribe();   // a subscription in a frame: a root
            int sum = 0;
            while (auto v = co_await s.receive()) {
                sum += (*v)->value;
            }
            co_return sum;
        }(bus)));
    }
    while (bus->events.subscribers() < 4) {
        std::this_thread::sleep_for(1ms);
    }
    off_frame([&] {
        for (int i = 1; i <= 10; ++i) {
            bus->events.send(make_tracked<Item>(i));
            std::this_thread::sleep_for(1ms);   // the listeners keep up: nothing lapped
        }
    });
    bus->events.close();
    for (auto& t : listeners) {
        EXPECT_EQ(t.wait(), 55);
    }
    settle();
    EXPECT_EQ(Item::alive.load(), before);   // every listener passed every value
    sgcl::async::scheduler::stop();
}

// A ring of two under eight senders: the senders are a whole lap ahead of
// the commit point all the time, and a slot must not be taken for the next
// lap before its position is committed (a sender that overwrote a stored,
// uncommitted position stopped the commits for good: every subscriber
// spinning in the drain after the close, the values after it never seen)
TEST(Broadcast_Test, ASlotIsNotReusedBeforeItsPositionIsCommitted) {
    sgcl::async::broadcast<int> b(2);
    constexpr int Senders = 8, PerSender = 2000;
    auto subscriber = [](sgcl::async::broadcast<int>::subscription s) -> sgcl::async::task<Seen> {
        Seen seen;
        while (auto v = co_await s.receive()) {
            seen.lagged += s.lagged();
            seen.values.push_back(*v);
        }
        co_return seen;
    };
    std::vector<sgcl::async::task<Seen>> subscribers;
    for (int i = 0; i < 8; ++i) {
        subscribers.push_back(sgcl::async::spawn(subscriber(b.subscribe())));
    }
    std::vector<std::thread> senders;
    for (int s = 0; s < Senders; ++s) {
        senders.emplace_back([&, s] {
            for (int k = 0; k < PerSender; ++k) {
                b.send(s * PerSender + k);
            }
        });
    }
    for (auto& t : senders) {
        t.join();
    }
    b.close();
    for (auto& t : subscribers) {
        Seen& seen = t.wait();
        EXPECT_EQ(seen.values.size() + seen.lagged, (size_t)Senders * PerSender);
    }
    sgcl::async::scheduler::stop();
}

// One walk of the sender wakes a hundred tasks, more than a batch holds:
// the batch is handed over and gathers again in the middle of the walk,
// the tasks handed over first running (and registering for the next
// value) while the walk goes on over the rest
TEST(Broadcast_Test, AHundredTaskSubscribersWokenByOneWalkFromATask) {
    every_subscriber_every_round(100, 0, false, true, 300);
}

// The same from a thread that is no worker: the frames to the global
// queue as one chain (concurrent::queue: push_range)
TEST(Broadcast_Test, AHundredTaskSubscribersWokenByOneWalkFromAThread) {
    every_subscriber_every_round(100, 0, false, false, 300);
}

// Three hundred woken by one walk on a worker: more than its ring holds
// when nothing is taken from it meanwhile, the rest spilled to the global
// queue
TEST(Broadcast_Test, ThreeHundredTaskSubscribersWokenByOneWalk) {
    every_subscriber_every_round(300, 0, false, true, 50);
}

// A frame whose header names an executor is not gathered: it goes on its
// executor's queue at the wake, a strand's one at a time and in order,
// an executor's to the thread that runs it; beside them the rest are
// gathered
TEST(Broadcast_Test, SubscribersOnAStrandAndAnExecutorAmongTheGathered) {
    every_subscriber_every_round(48, 16, true, true, 200);
    every_subscriber_every_round(48, 16, true, false, 200);
}

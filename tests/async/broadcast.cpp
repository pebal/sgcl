//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

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

    sgcl::task<Seen> drain(sgcl::broadcast<int>::subscription s) {
        Seen seen;
        while (auto v = co_await s.async_receive()) {
            seen.lagged += s.lagged();
            seen.values.push_back(*v);
        }
        co_return seen;
    }
}

TEST(Broadcast_Test, ThreeSubscribersReceiveEveryValueInOrder) {
    sgcl::broadcast<int> b(1024);
    EXPECT_EQ(b.capacity(), 1024u);
    std::vector<sgcl::task<Seen>> subscribers;
    for (int i = 0; i < 3; ++i) {
        subscribers.push_back(sgcl::spawn(drain(b.subscribe())));
    }
    EXPECT_EQ(b.subscribers(), 3u);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(b.send(i));
    }
    b.close();
    EXPECT_FALSE(b.send(1000));
    for (auto& t : subscribers) {
        Seen& seen = t.join();
        ASSERT_EQ(seen.values.size(), 1000u);
        for (int i = 0; i < 1000; ++i) {
            EXPECT_EQ(seen.values[i], i);
        }
        EXPECT_EQ(seen.lagged, 0u);
    }
    subscribers.clear();                   // the tasks' subscriptions go with their frames
    EXPECT_EQ(b.subscribers(), 0u);
    sgcl::scheduler::stop();
}

TEST(Broadcast_Test, ASlowSubscriberIsLapped) {
    sgcl::broadcast<int> b(8);
    auto slow = b.subscribe();
    auto quick = b.subscribe();
    for (int i = 0; i < 20; ++i) {
        b.send(i);
        EXPECT_EQ(*quick.receive(), i);    // keeps up: never lapped
    }
    EXPECT_EQ(quick.lagged(), 0u);
    // the ring holds the last eight: 12..19; the twelve before are lost
    auto v = slow.receive();
    ASSERT_TRUE(v);
    EXPECT_EQ(*v, 12);
    EXPECT_EQ(slow.lagged(), 12u);
    for (int i = 13; i < 20; ++i) {
        EXPECT_EQ(*slow.receive(), i);
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
    sgcl::broadcast<int> b(16);
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
    sgcl::broadcast<int> b(4);
    std::vector<sgcl::task<Seen>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.push_back(sgcl::spawn(drain(b.subscribe())));
    }
    auto s = b.subscribe();
    std::atomic<int> got = {-1};
    std::thread th([&] {
        auto v = s.receive();              // blocks until the close
        got = v ? *v : -2;
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(got, -1);
    b.send(7);
    b.close();
    EXPECT_TRUE(b.closed());
    th.join();
    EXPECT_EQ(got, 7);                     // sent before the close: still received
    EXPECT_FALSE(s.receive());             // then nothing
    EXPECT_TRUE(s.closed());
    for (auto& t : tasks) {
        Seen& seen = t.join();
        EXPECT_EQ(seen.values, std::vector<int>{7});
    }
    auto after = b.subscribe();            // a subscription made after the close: nothing
    EXPECT_FALSE(after.receive());
    EXPECT_FALSE(after.try_receive());
    sgcl::scheduler::stop();
}

TEST(Broadcast_Test, ASelectCase) {
    sgcl::broadcast<int> b(4);
    auto s = b.subscribe();
    int got = 0;
    // nothing there: the otherwise
    EXPECT_EQ(sgcl::select(s.on_receive([&](int v) { got = v; }), sgcl::otherwise([] {})), 1u);
    b.send(5);
    EXPECT_EQ(sgcl::select(s.on_receive([&](int v) { got = v; }), sgcl::otherwise([] {})), 0u);
    EXPECT_EQ(got, 5);
    // a task waits in a select until the value comes
    auto t = sgcl::spawn([](sgcl::broadcast<int>::subscription& s) -> sgcl::task<int> {
        int got = 0;
        size_t i = co_await sgcl::async_select(s.on_receive([&](int v) { got = v; }));
        co_return i == 0 ? got : -1;
    }(s));
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(t.done());
    b.send(6);
    EXPECT_EQ(t.join(), 6);
    // the closed broadcast serves the case with nothing
    b.close();
    bool nothing = false;
    EXPECT_EQ(sgcl::select(s.on_receive([&](sgcl::optional<int> v) { nothing = !v; })), 0u);
    EXPECT_TRUE(nothing);
    sgcl::scheduler::stop();
}

TEST(Broadcast_Test, ValuesHeldUntilEverySubscriberPassedThem) {
    settle();
    const int before = Item::alive.load();
    sgcl::broadcast<tracked_ptr<Item>> b(16);
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
            auto v = first.receive();
            ASSERT_TRUE(v);
            EXPECT_EQ((*v)->value, i);
        }
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 4);   // the second has not passed them
    off_frame([&] {
        EXPECT_EQ((*second.receive())->value, 0);
        EXPECT_EQ((*second.receive())->value, 1);
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
        auto v = first.receive();
        EXPECT_EQ((*v)->value, 24);
        EXPECT_EQ(first.lagged(), 24u);
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 15);
}

TEST(Broadcast_Test, EightSendersEightSubscribers) {
    sgcl::broadcast<int> b(256);
    constexpr int Senders = 8, PerSender = 2000;
    // a subscriber counts what it got and what it lost, and checks the
    // order of each sender's values (a value is sender * PerSender + k)
    auto subscriber = [](sgcl::broadcast<int>::subscription s) -> sgcl::task<Seen> {
        Seen seen;
        std::vector<int> last(Senders, -1);
        while (auto v = co_await s.async_receive()) {
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
    std::vector<sgcl::task<Seen>> subscribers;
    for (int i = 0; i < 8; ++i) {
        subscribers.push_back(sgcl::spawn(subscriber(b.subscribe())));
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
        Seen& seen = t.join();
        EXPECT_EQ(seen.values.size() + seen.lagged, (size_t)Senders * PerSender);   // every value received or counted lost
        for (int v : seen.values) {
            EXPECT_NE(v, -1);
        }
    }
    sgcl::scheduler::stop();
}

TEST(Broadcast_Test, InsideAManagedObjectWithTaskSubscribers) {
    struct Bus {
        sgcl::broadcast<tracked_ptr<Item>> events{16};
    };
    settle();
    const int before = Item::alive.load();
    tracked_ptr bus = make_tracked<Bus>();
    std::vector<sgcl::task<int>> listeners;
    for (int i = 0; i < 4; ++i) {
        listeners.push_back(sgcl::spawn([](tracked_ptr<Bus> bus) -> sgcl::task<int> {
            auto s = bus->events.subscribe();   // a subscription in a frame: a root
            int sum = 0;
            while (auto v = co_await s.async_receive()) {
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
        EXPECT_EQ(t.join(), 55);
    }
    settle();
    EXPECT_EQ(Item::alive.load(), before);   // every listener passed every value
    sgcl::scheduler::stop();
}

// A ring of two under eight senders: the senders are a whole lap ahead of
// the commit point all the time, and a slot must not be taken for the next
// lap before its position is committed (a sender that overwrote a stored,
// uncommitted position stopped the commits for good: every subscriber
// spinning in the drain after the close, the values after it never seen)
TEST(Broadcast_Test, ASlotIsNotReusedBeforeItsPositionIsCommitted) {
    sgcl::broadcast<int> b(2);
    constexpr int Senders = 8, PerSender = 2000;
    auto subscriber = [](sgcl::broadcast<int>::subscription s) -> sgcl::task<Seen> {
        Seen seen;
        while (auto v = co_await s.async_receive()) {
            seen.lagged += s.lagged();
            seen.values.push_back(*v);
        }
        co_return seen;
    };
    std::vector<sgcl::task<Seen>> subscribers;
    for (int i = 0; i < 8; ++i) {
        subscribers.push_back(sgcl::spawn(subscriber(b.subscribe())));
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
        Seen& seen = t.join();
        EXPECT_EQ(seen.values.size() + seen.lagged, (size_t)Senders * PerSender);
    }
    sgcl::scheduler::stop();
}

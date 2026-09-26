//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// promise<T>: a one-shot completion set by any thread or a C callback,
// awaited by a task, blocked on by a thread, a case of a select.
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // A C API with a callback and a void* context, completing on a thread
    // of its own: what a promise adapts to co_await
    void c_api_start(void (*callback)(void*, int), void* context) {
        std::thread([=] {
            std::this_thread::sleep_for(10ms);
            callback(context, 99);
        }).detach();
    }

    struct Context {
        sgcl::root_ptr<sgcl::async::promise<int>> done;   // the promise held from unmanaged memory: a root
    };

    void on_complete(void* context, int result) {
        auto ctx = static_cast<Context*>(context);
        ctx->done->set_value(result);
        delete ctx;
    }

    // A promise inside a managed object, set with an object
    struct Holder {
        sgcl::async::promise<sgcl::tracked_ptr<Node>> node;
    };
}

TEST(Promise_Tests, SetFromAnotherThreadAwaitedByATask) {
    sgcl::async::promise<int> p;
    EXPECT_FALSE(p.done());
    auto t = sgcl::async::spawn([](sgcl::async::promise<int>& p) -> sgcl::async::task<int> {
        co_return co_await p;                              // suspended until the set
    }(p));
    std::thread th([&] {
        std::this_thread::sleep_for(10ms);
        p.set_value(42);
    });
    EXPECT_EQ(t.wait(), 42);
    th.join();
    EXPECT_TRUE(p.done());
    EXPECT_EQ(p.result(), 42);
    sgcl::async::scheduler::stop();
}

TEST(Promise_Tests, SetBeforeTheAwait) {
    sgcl::async::promise<int> p;
    p.set_value(7);
    EXPECT_TRUE(p.done());
    auto t = sgcl::async::spawn([](sgcl::async::promise<int>& p) -> sgcl::async::task<int> {
        co_return co_await p;                              // ready: no suspension
    }(p));
    EXPECT_EQ(t.wait(), 7);
    EXPECT_EQ(p.result(), 7);                                 // a wait after the set does not wait
    sgcl::async::scheduler::stop();
}

TEST(Promise_Tests, SetWithAnException) {
    sgcl::async::promise<int> p;
    auto t = sgcl::async::spawn([](sgcl::async::promise<int>& p) -> sgcl::async::task<int> {
        try {
            co_await p;
        } catch (const std::runtime_error& e) {
            co_return std::string(e.what()) == "failed" ? 1 : 0;
        }
        co_return -1;
    }(p));
    p.set_exception(std::make_exception_ptr(std::runtime_error("failed")));
    EXPECT_EQ(t.wait(), 1);
    EXPECT_TRUE(p.done());
    EXPECT_THROW(p.wait(), std::runtime_error);            // from a thread too
    EXPECT_THROW(p.result(), std::runtime_error);
    sgcl::async::scheduler::stop();
}

TEST(Promise_Tests, GetFromAThread) {
    sgcl::async::promise<std::string> p;
    std::string got;
    std::thread waiter([&] { got = p.result(); });            // blocks until the set
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(p.done());
    p.set_value(std::string("hello"));
    waiter.join();
    EXPECT_EQ(got, "hello");
    // the value stays in the promise for every reader
    EXPECT_EQ(p.result(), "hello");
}

TEST(Promise_Tests, ATrackedPtrValueKeepsItsObject) {
    settle();
    const int before = Node::alive.load();
    sgcl::tracked_ptr holder = sgcl::make_tracked<Holder>();
    off_frame([&] {
        holder->node.set_value(sgcl::make_tracked<Node>(5));   // the only pointer to the node is the promise's value
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    auto t = sgcl::async::spawn([](sgcl::tracked_ptr<Holder> h) -> sgcl::async::task<int> {
        sgcl::tracked_ptr<Node> n = co_await h->node;
        co_return n->value;
    }(holder));
    EXPECT_EQ(t.wait(), 5);
    holder = nullptr;
    t.destroy();
    sgcl::async::scheduler::stop();                               // the workers' rings let go of the frame's word
    settle();
    EXPECT_EQ(Node::alive.load(), before);                 // the promise gone with its holder, the node with the promise
}

TEST(Promise_Tests, ACCallbackWithAVoidContext) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        sgcl::tracked_ptr p = sgcl::make_tracked<sgcl::async::promise<int>>();   // a managed object: the frame holds it here, the context holds it through a root
        c_api_start(on_complete, new Context{p});
        co_return co_await *p;
    }());
    EXPECT_EQ(t.wait(), 99);
    sgcl::async::scheduler::stop();
}

TEST(Promise_Tests, ManyWaitersOneValue) {
    sgcl::async::promise<int> p;
    std::vector<sgcl::async::task<int>> tasks;
    for (int i = 0; i < 20; ++i) {
        tasks.push_back(sgcl::async::spawn([](sgcl::async::promise<int>& p, int i) -> sgcl::async::task<int> {
            co_return co_await p + i;
        }(p, i)));
    }
    std::vector<std::thread> threads;
    std::atomic<int> sum = {0};
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] { sum += p.result(); });
    }
    std::this_thread::sleep_for(10ms);
    p.set_value(100);
    int total = 0;
    for (auto& t : tasks) {
        total += t.wait();
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(total, 20 * 100 + 190);
    EXPECT_EQ(sum, 400);
    sgcl::async::scheduler::stop();
}

TEST(Promise_Tests, ACaseOfASelect) {
    sgcl::async::promise<int> p;
    // a thread's select: the promise against a timeout
    EXPECT_EQ(sgcl::async::select(p.on_done([] {}), sgcl::async::timeout(5ms, [] {})).wait(), 1u);
    // a task's select: served by the set
    auto t = sgcl::async::spawn([](sgcl::async::promise<int>& p) -> sgcl::async::task<int> {
        int seen = 0;
        co_await sgcl::async::select(
            p.on_done([&] { seen = p.result(); }),
            sgcl::async::timeout(1s, [&] { seen = -1; })
        );
        co_return seen;
    }(p));
    std::this_thread::sleep_for(5ms);
    p.set_value(3);
    EXPECT_EQ(t.wait(), 3);
    EXPECT_EQ(sgcl::async::select(p.on_done([] {})).wait(), 0u);    // ready: served at once
    sgcl::async::scheduler::stop();
}

TEST(Promise_Tests, APromiseOfNothing) {
    sgcl::async::promise<> p;
    EXPECT_FALSE(p.done());
    auto t = sgcl::async::spawn([](sgcl::async::promise<>& p) -> sgcl::async::task<int> {
        co_await p;
        co_return 1;
    }(p));
    std::thread th([&] { p.set_value(); });
    EXPECT_EQ(t.wait(), 1);
    th.join();
    EXPECT_TRUE(p.done());
    p.wait();
    // an exception in a promise of nothing
    sgcl::async::promise<> q;
    q.set_exception(std::make_exception_ptr(std::logic_error("no")));
    EXPECT_THROW(q.wait(), std::logic_error);
    sgcl::async::scheduler::stop();
}

TEST(Promise_Tests, TheFirstSetterWins) {
    sgcl::async::promise<int> p;
    p.set_value(1);
#ifdef NDEBUG
    p.set_value(2);                                        // ignored: the first value stands (asserted in debug builds)
    p.set_exception(std::make_exception_ptr(std::runtime_error("late")));
#endif
    EXPECT_EQ(p.result(), 1);
}

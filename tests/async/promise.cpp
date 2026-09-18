//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// promise<T>: a one-shot completion set by any thread or a C callback,
// awaited by a task, blocked on by a thread, a case of a select.
#include "tests/types.h"

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
        sgcl::root_ptr<sgcl::promise<int>> done;   // the promise held from unmanaged memory: a root
    };

    void on_complete(void* context, int result) {
        auto ctx = static_cast<Context*>(context);
        ctx->done->set_value(result);
        delete ctx;
    }

    // A promise inside a managed object, set with an object
    struct Holder {
        sgcl::promise<sgcl::tracked_ptr<Node>> node;
    };
}

TEST(Promise_Tests, SetFromAnotherThreadAwaitedByATask) {
    sgcl::promise<int> p;
    EXPECT_FALSE(p.ready());
    auto t = sgcl::spawn([](sgcl::promise<int>& p) -> sgcl::task<int> {
        co_return co_await p;                              // suspended until the set
    }(p));
    std::thread th([&] {
        std::this_thread::sleep_for(10ms);
        p.set_value(42);
    });
    EXPECT_EQ(t.join(), 42);
    th.join();
    EXPECT_TRUE(p.ready());
    EXPECT_EQ(p.result(), 42);
    sgcl::scheduler::stop();
}

TEST(Promise_Tests, SetBeforeTheAwait) {
    sgcl::promise<int> p;
    p.set_value(7);
    EXPECT_TRUE(p.ready());
    auto t = sgcl::spawn([](sgcl::promise<int>& p) -> sgcl::task<int> {
        co_return co_await p;                              // ready: no suspension
    }(p));
    EXPECT_EQ(t.join(), 7);
    EXPECT_EQ(p.get(), 7);                                 // a wait after the set does not wait
    sgcl::scheduler::stop();
}

TEST(Promise_Tests, SetWithAnException) {
    sgcl::promise<int> p;
    auto t = sgcl::spawn([](sgcl::promise<int>& p) -> sgcl::task<int> {
        try {
            co_await p;
        } catch (const std::runtime_error& e) {
            co_return std::string(e.what()) == "failed" ? 1 : 0;
        }
        co_return -1;
    }(p));
    p.set_exception(std::make_exception_ptr(std::runtime_error("failed")));
    EXPECT_EQ(t.join(), 1);
    EXPECT_TRUE(p.ready());
    EXPECT_THROW(p.get(), std::runtime_error);            // from a thread too
    EXPECT_THROW(p.result(), std::runtime_error);
    sgcl::scheduler::stop();
}

TEST(Promise_Tests, GetFromAThread) {
    sgcl::promise<std::string> p;
    std::string got;
    std::thread waiter([&] { got = p.get(); });            // blocks until the set
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(p.ready());
    p.set_value(std::string("hello"));
    waiter.join();
    EXPECT_EQ(got, "hello");
    // the value stays in the promise for every reader
    EXPECT_EQ(p.get(), "hello");
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
    auto t = sgcl::spawn([](sgcl::tracked_ptr<Holder> h) -> sgcl::task<int> {
        sgcl::tracked_ptr<Node> n = co_await h->node;
        co_return n->value;
    }(holder));
    EXPECT_EQ(t.join(), 5);
    holder = nullptr;
    t.destroy();
    sgcl::scheduler::stop();                               // the workers' rings let go of the frame's word
    settle();
    EXPECT_EQ(Node::alive.load(), before);                 // the promise gone with its holder, the node with the promise
}

TEST(Promise_Tests, ACCallbackWithAVoidContext) {
    auto t = sgcl::spawn([]() -> sgcl::task<int> {
        sgcl::tracked_ptr p = sgcl::make_tracked<sgcl::promise<int>>();   // a managed object: the frame holds it here, the context holds it through a root
        c_api_start(on_complete, new Context{p});
        co_return co_await *p;
    }());
    EXPECT_EQ(t.join(), 99);
    sgcl::scheduler::stop();
}

TEST(Promise_Tests, ManyWaitersOneValue) {
    sgcl::promise<int> p;
    std::vector<sgcl::task<int>> tasks;
    for (int i = 0; i < 20; ++i) {
        tasks.push_back(sgcl::spawn([](sgcl::promise<int>& p, int i) -> sgcl::task<int> {
            co_return co_await p + i;
        }(p, i)));
    }
    std::vector<std::thread> threads;
    std::atomic<int> sum = {0};
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] { sum += p.get(); });
    }
    std::this_thread::sleep_for(10ms);
    p.set_value(100);
    int total = 0;
    for (auto& t : tasks) {
        total += t.join();
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(total, 20 * 100 + 190);
    EXPECT_EQ(sum, 400);
    sgcl::scheduler::stop();
}

TEST(Promise_Tests, ACaseOfASelect) {
    sgcl::promise<int> p;
    // a thread's select: the promise against a timeout
    EXPECT_EQ(sgcl::select(p.on_ready([] {}), sgcl::timeout(5ms, [] {})), 1u);
    // a task's select: served by the set
    auto t = sgcl::spawn([](sgcl::promise<int>& p) -> sgcl::task<int> {
        int seen = 0;
        co_await sgcl::async_select(
            p.on_ready([&] { seen = p.result(); }),
            sgcl::timeout(1s, [&] { seen = -1; })
        );
        co_return seen;
    }(p));
    std::this_thread::sleep_for(5ms);
    p.set_value(3);
    EXPECT_EQ(t.join(), 3);
    EXPECT_EQ(sgcl::select(p.on_ready([] {})), 0u);    // ready: served at once
    sgcl::scheduler::stop();
}

TEST(Promise_Tests, APromiseOfNothing) {
    sgcl::promise<> p;
    EXPECT_FALSE(p.ready());
    auto t = sgcl::spawn([](sgcl::promise<>& p) -> sgcl::task<int> {
        co_await p;
        co_return 1;
    }(p));
    std::thread th([&] { p.set_value(); });
    EXPECT_EQ(t.join(), 1);
    th.join();
    EXPECT_TRUE(p.ready());
    p.get();
    // an exception in a promise of nothing
    sgcl::promise<> q;
    q.set_exception(std::make_exception_ptr(std::logic_error("no")));
    EXPECT_THROW(q.get(), std::logic_error);
    sgcl::scheduler::stop();
}

TEST(Promise_Tests, TheFirstSetterWins) {
    sgcl::promise<int> p;
    p.set_value(1);
#ifdef NDEBUG
    p.set_value(2);                                        // ignored: the first value stands (asserted in debug builds)
    p.set_exception(std::make_exception_ptr(std::runtime_error("late")));
#endif
    EXPECT_EQ(p.get(), 1);
}

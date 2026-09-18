//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Promise<T> and SpawnBlocking: the facades over sgcl::promise and
// sgcl::spawn_blocking
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    struct Item {
        explicit Item(int v) : value(v) {}
        int value;
    };

    // A C API with a callback and a void* context, completing on a thread of its own
    void c_api_start(void (*callback)(void*, int), void* context) {
        std::thread([=] {
            std::this_thread::sleep_for(5ms);
            callback(context, 7);
        }).detach();
    }

    struct Context {
        RootPtr<Promise<int>> done;   // the promise held from unmanaged memory: a root
    };

    void on_complete(void* context, int result) {
        auto ctx = static_cast<Context*>(context);
        ctx->done->SetValue(result);
        delete ctx;
    }
}

TEST(Sgcl_Promise_Tests, SetAwaitedGotSelected) {
    using namespace Sgcl;
    Promise<int> p;
    EXPECT_FALSE(p.IsReady());
    auto t = Spawn([](Promise<int>& p) -> Task<int> {
        co_return co_await p;                              // suspended until the set
    }(p));
    std::thread th([&] {
        std::this_thread::sleep_for(5ms);
        p.SetValue(42);
    });
    EXPECT_EQ(t.Join(), 42);
    th.join();
    EXPECT_TRUE(p.IsReady());
    EXPECT_EQ(p.Get(), 42);                                // a wait after the set does not wait
    EXPECT_EQ(p.Result(), 42);
    EXPECT_EQ(Select(p.OnReady([] {})), 0u);               // a case of a Select, ready
    // an exception, from a task and from a thread
    Promise<int> q;
    auto u = Spawn([](Promise<int>& q) -> Task<int> {
        try {
            co_await q;
        } catch (const std::runtime_error&) {
            co_return 1;
        }
        co_return 0;
    }(q));
    q.SetException(std::make_exception_ptr(std::runtime_error("failed")));
    EXPECT_EQ(u.Join(), 1);
    EXPECT_THROW(q.Get(), std::runtime_error);
    // a promise of nothing
    Promise<> v;
    v.SetValue();
    EXPECT_TRUE(v.IsReady());
    v.Get();
    Scheduler::Stop();
}

TEST(Sgcl_Promise_Tests, APtrValueAndACCallback) {
    using namespace Sgcl;
    Ptr<Promise<Ptr<Item>>> p = Make<Promise<Ptr<Item>>>();   // a promise as a managed object
    auto t = Spawn([](Ptr<Promise<Ptr<Item>>> p) -> Task<int> {
        Ptr<Item> item = co_await *p;
        co_return item->value;
    }(p));
    p->SetValue(Make<Item>(5));
    EXPECT_EQ(t.Join(), 5);
    auto u = Spawn([]() -> Task<int> {
        Ptr<Promise<int>> done = Make<Promise<int>>();
        c_api_start(on_complete, new Context{done});      // the context holds the promise through a root
        co_return co_await *done;
    }());
    EXPECT_EQ(u.Join(), 7);
    Scheduler::Stop();
}

TEST(Sgcl_Blocking_Tests, SpawnBlockingReturnsThrowsAndStops) {
    using namespace Sgcl;
    auto t = Spawn([]() -> Task<int> {
        int r = co_await SpawnBlocking([] {
            EXPECT_FALSE(Scheduler::OnWorker());           // on a thread of the pool, not a worker
            std::this_thread::sleep_for(5ms);
            return 21;
        });
        co_await Blocking([] { std::this_thread::sleep_for(1ms); });   // the alias, and nothing returned
        co_return r * 2;
    }());
    EXPECT_EQ(t.Join(), 42);
    auto u = Spawn([]() -> Task<std::string> {
        try {
            co_await SpawnBlocking([]() -> int { throw std::runtime_error("from the pool"); });
        } catch (const std::runtime_error& e) {
            co_return e.what();
        }
        co_return "nothing";
    }());
    EXPECT_EQ(u.Join(), "from the pool");
    // from a thread, through Join(); the closure may capture a Ptr
    Ptr<Item> item = Make<Item>(3);
    BlockingTask<int> job = SpawnBlocking([item] { return item->value; });
    EXPECT_EQ(job.Join(), 3);
    EXPECT_TRUE(job.IsDone());
    auto st = BlockingPool::GetStatistics();
    EXPECT_GE(st.Threads, 1u);
    EXPECT_LE(st.Threads, BlockingPool::MaxThreads());
    BlockingPool::WaitIdle();
    EXPECT_EQ(BlockingPool::GetStatistics().Queued, 0u);
    BlockingPool::Stop();
    EXPECT_EQ(BlockingPool::GetStatistics().Threads, 0u);
    BlockingPool::SetIdleTime(50ms);
    EXPECT_EQ(BlockingPool::IdleTime(), 50ms);
    EXPECT_EQ(SpawnBlocking([] { return 1; }).Join(), 1);   // started again
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(BlockingPool::GetStatistics().Threads, 0u);   // idled out
    BlockingPool::SetIdleTime(std::chrono::milliseconds(sgcl::config::BlockingIdleMilliseconds));
    Scheduler::Stop();
}

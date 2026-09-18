//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Executor, Strand, On, OnWorkers, TaskLocal: the facades over sgcl::executor,
// sgcl::strand, sgcl::on, sgcl::on_workers and sgcl::task_local
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <chrono>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    TaskLocal<int> RequestId;

    int ReadId() {
        return RequestId.GetOr(-1);
    }

    Task<int> Reader() {
        co_return ReadId();
    }

    Task<std::thread::id> Where() {
        co_return std::this_thread::get_id();
    }

    Task<int> Trips(Executor& ex, std::vector<std::thread::id>& ids) {
        ids.push_back(std::this_thread::get_id());
        co_await Sleep(1ms);
        ids.push_back(std::this_thread::get_id());
        co_await OnWorkers();
        ids.push_back(std::this_thread::get_id());
        co_await On(ex);
        ids.push_back(std::this_thread::get_id());
        co_return 5;
    }

    Task<> Increment(int& counter, int n) {
        for (int i : Range(n)) {
            ++counter;
            if (i % 1000 == 999) {
                co_await Yield();
            }
        }
    }

    Task<int> Family(std::vector<int>& seen) {
        co_await RequestId.Set(7);
        seen.push_back(ReadId());
        seen.push_back(co_await Spawn(Reader()));
        seen.push_back(co_await RequestId.With(9, Reader()));
        seen.push_back(ReadId());
        co_return RequestId.IsSet() ? 1 : 0;
    }
}

TEST(Sgcl_Executor_Tests, RunPollStop) {
    using namespace Sgcl;
    Executor ex;
    std::vector<std::thread::id> ids;
    EXPECT_EQ(ex.Run(Trips(ex, ids)), 5);
    ASSERT_EQ(ids.size(), 4u);
    EXPECT_EQ(ids[0], std::this_thread::get_id());
    EXPECT_EQ(ids[1], std::this_thread::get_id());
    EXPECT_NE(ids[2], std::this_thread::get_id());
    EXPECT_EQ(ids[3], std::this_thread::get_id());
    auto t = ex.Spawn(Where());
    EXPECT_EQ(ex.Poll(), 1u);
    EXPECT_TRUE(t.IsDone());
    EXPECT_EQ(t.Result(), std::this_thread::get_id());
    ex.Stop();
    ex.Run();                            // returns at once
    EXPECT_FALSE(ex.IsRunning());
    auto u = Spawn(Where(), ex);
    ex.RunUntil(u);
    EXPECT_EQ(u.Result(), std::this_thread::get_id());
    Scheduler::Stop();
}

TEST(Sgcl_Executor_Tests, StrandSerializes) {
    using namespace Sgcl;
    Strand s;
    int counter = 0;
    List<Task<>> tasks;
    for (int i : Range(8)) {
        (void)i;
        tasks.Add(s.Spawn(Increment(counter, 100000)));
    }
    for (auto& t : tasks) {
        t.Join();
    }
    EXPECT_EQ(counter, 800000);
    for (int i = 0; i < 1000 && s.IsBusy(); ++i) {   // the last run is counted until the worker's epilogue
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_FALSE(s.IsBusy());
    auto t = [](Strand& s) -> Task<bool> {
        co_await On(s);
        co_return s.IsBusy() && Scheduler::OnWorker();
    };
    EXPECT_TRUE(Spawn(t(s)).Join());
    Scheduler::Stop();
}

TEST(Sgcl_Executor_Tests, TaskLocal) {
    using namespace Sgcl;
    EXPECT_EQ(RequestId.Get(), None);
    std::vector<int> seen;
    EXPECT_EQ(Spawn(Family(seen)).Join(), 1);
    ASSERT_EQ(seen.size(), 4u);
    EXPECT_EQ(seen[0], 7);
    EXPECT_EQ(seen[1], 7);
    EXPECT_EQ(seen[2], 9);
    EXPECT_EQ(seen[3], 7);
    EXPECT_EQ(ReadId(), -1);
    Scheduler::Stop();
}

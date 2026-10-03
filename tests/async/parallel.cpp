//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// parallel_for, parallel_for_each and parallel_reduce (async/parallel.h) at
// their boundaries (DESIGN 408): an empty range, one index, begin past end,
// the ends of 64-bit and narrow integers, a step of zero, the lanes at
// zero, one and past the workers, a grain past the range, a body that
// throws at the first, a middle and the last index, nesting, a task and a
// plain thread as the caller, every worker a caller at once, a reduction
// whose combine does not commute, and every index visited exactly once.
// Each loop runs inside its call and returns when it is done.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    namespace async = sgcl::async;

    // The calls give nothing (the for forms) or the result (reduce), and
    // are noexcept when the body's call is, but for the step's form,
    // which refuses a step of zero
    static_assert(std::is_void_v<decltype(async::parallel_for(1, [](int) {}))>);
    static_assert(std::is_void_v<decltype(async::parallel_for_each(std::declval<std::vector<int>&>(), [](int&) {}))>);
    static_assert(std::is_same_v<decltype(async::parallel_reduce(0, 1, 0LL, [](int i) { return i; })), long long>);
    static_assert(noexcept(async::parallel_for(1, [](int) noexcept {})));
    static_assert(noexcept(async::parallel_for(0, 1, [](int, unsigned) noexcept {})));
    static_assert(!noexcept(async::parallel_for(0, 1, [](int) {})));
    static_assert(!noexcept(async::parallel_for(0, 1, 1, [](int) noexcept {})));
    static_assert(noexcept(async::parallel_for_each(std::declval<std::vector<int>&>(), [](int&) noexcept {})));
    static_assert(!noexcept(async::parallel_for_each(std::declval<std::vector<int>&>(), [](int&) {})));

    // One counter per index, every one expected at exactly one
    struct Hits {
        explicit Hits(size_t n)
        : counts(new std::atomic<int>[n])
        , size(n) {
            for (size_t i = 0; i < n; ++i) {
                counts[i].store(0, std::memory_order_relaxed);
            }
        }

        void hit(size_t i) const {
            counts[i].fetch_add(1, std::memory_order_relaxed);
        }

        // The indices not hit exactly once
        size_t wrong() const {
            size_t bad = 0;
            for (size_t i = 0; i < size; ++i) {
                bad += counts[i].load(std::memory_order_relaxed) != 1;
            }
            return bad;
        }

        size_t total() const {
            size_t sum = 0;
            for (size_t i = 0; i < size; ++i) {
                sum += (size_t)counts[i].load(std::memory_order_relaxed);
            }
            return sum;
        }

        std::unique_ptr<std::atomic<int>[]> counts;
        size_t size;
    };

    // The loop called inside a task, on its worker: every index visited
    // when the call returns, with no co_await between
    async::task<long long> sum_in_task(long long n) {
        std::atomic<long long> sum = 0;
        async::parallel_for(n, [&](long long i) {
            sum.fetch_add(i, std::memory_order_relaxed);
        });
        co_return sum.load();
    }

    async::task<std::string> concat_in_task(int n, async::parallel_options o) {
        co_return async::parallel_reduce(0, n, std::string(), [](int i) { return std::to_string(i) + ","; },
                                         [](std::string a, std::string b) { return a + b; }, o);
    }

    async::task<int> throws_in_task(int at) {
        async::parallel_for(0, 1000, [at](int i) {
            if (i == at) {
                throw std::runtime_error("index " + std::to_string(i));
            }
        }, {.grain = 1});
        co_return 0;
    }

    // Lane 0 is the task's own worker thread: true when every call of
    // lane 0 ran on the thread that made the call
    async::task<bool> lane_zero_is_the_task() {
        const auto here = std::this_thread::get_id();
        std::atomic<int> elsewhere = 0;
        async::parallel_for(10000, [&](int, unsigned lane) {
            elsewhere += lane == 0 && std::this_thread::get_id() != here;
        }, {.grain = 1});
        co_return elsewhere.load() == 0 && std::this_thread::get_id() == here;
    }

    // A loop whose every index runs a loop of its own, in a task
    async::task<long long> nested_in_task(int n) {
        std::atomic<long long> sum = 0;
        async::parallel_for(n, [&](int i) {
            async::parallel_for(n, [&](int j) { sum.fetch_add(i * n + j, std::memory_order_relaxed); }, {.grain = 1});
        }, {.grain = 1});
        co_return sum.load();
    }

    std::string concat_sequential(int begin, int end, std::string init) {
        for (int i = begin; i < end; ++i) {
            init += std::to_string(i) + ",";
        }
        return init;
    }
}

// Nothing to visit: a count of zero or less, begin at end, begin past end
// with a positive step and below it with a negative one; a reduction of
// nothing is its init, untouched by combine
TEST(Parallel_Tests, AnEmptyRangeCallsNothing) {
    std::atomic<int> calls = 0;
    auto f = [&](auto) { ++calls; };
    async::parallel_for(0, f);
    async::parallel_for(-5, f);
    async::parallel_for(7, 7, f);
    async::parallel_for(10, 5, f);  // begin past end: empty, not a descent
    async::parallel_for(size_t(10), size_t(5), f);
    async::parallel_for(5, 10, -1, f);  // a step down from below end: empty
    async::parallel_for(10, 5, 2, f);
    std::vector<int> none;
    async::parallel_for_each(none, [&](int) { ++calls; });
    EXPECT_EQ(calls.load(), 0);
    int combined = 0;
    auto r = async::parallel_reduce(3, 3, 42, [](int i) { return i; }, [&](int a, int b) { ++combined; return a + b; });
    EXPECT_EQ(r, 42);
    EXPECT_EQ(combined, 0);
    EXPECT_EQ(async::parallel_reduce(9, 1, std::string("x"), [](int) { return std::string("y"); }), "x");
}

// One index: called once, on the caller's thread, as lane 0
TEST(Parallel_Tests, OneIndexRunsOnTheCaller) {
    auto caller = std::this_thread::get_id();
    int calls = 0;
    std::thread::id where;
    unsigned seen_lane = 99;
    async::parallel_for(7, 8, [&](int i, unsigned lane) {
        EXPECT_EQ(i, 7);
        ++calls;
        where = std::this_thread::get_id();
        seen_lane = lane;
    });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(where, caller);
    EXPECT_EQ(seen_lane, 0u);
    EXPECT_EQ(async::parallel_reduce(4, 5, 10, [](int i) { return i; }), 14);
}

// A step down: begin and the indices above end; a step up larger than the
// range: begin alone
TEST(Parallel_Tests, StepsUpAndDown) {
    Hits down(5);
    async::parallel_for(10, 5, -1, [&](int i) { down.hit(size_t(i - 6)); }, {.grain = 1});   // 10, 9, 8, 7, 6
    EXPECT_EQ(down.wrong(), 0u);
    Hits odd(50);
    async::parallel_for(1, 100, 2, [&](int i) { odd.hit(size_t(i / 2)); }, {.grain = 1});
    EXPECT_EQ(odd.wrong(), 0u);
    std::atomic<int> calls = 0;
    async::parallel_for(3, 10, 100, [&](int i) { EXPECT_EQ(i, 3); ++calls; });
    EXPECT_EQ(calls.load(), 1);
}

// A step of zero is refused at the call, before anything runs
TEST(Parallel_Tests, AStepOfZeroIsRefused) {
    std::atomic<int> calls = 0;
    EXPECT_THROW(async::parallel_for(0, 10, 0, [&](int) { ++calls; }), std::invalid_argument);
    EXPECT_THROW(async::parallel_for(size_t(0), size_t(10), size_t(0), [&](size_t) { ++calls; }), std::invalid_argument);
    EXPECT_EQ(calls.load(), 0);
}

// The ends of the integers: the last indices below INT64_MAX and SIZE_MAX,
// the first above INT64_MIN, a whole narrow type, steps across the whole
// 64-bit range both ways, all counted without overflow
TEST(Parallel_Tests, TheEndsOfTheIntegers) {
    constexpr int64_t max = std::numeric_limits<int64_t>::max();
    constexpr int64_t min = std::numeric_limits<int64_t>::min();
    Hits top(100);
    async::parallel_for(max - 100, max, [&](int64_t i) { top.hit(size_t(i - (max - 100))); }, {.grain = 1});
    EXPECT_EQ(top.wrong(), 0u);
    Hits bottom(100);
    async::parallel_for(min, min + 100, [&](int64_t i) { bottom.hit(size_t(i - min)); }, {.grain = 3});
    EXPECT_EQ(bottom.wrong(), 0u);
    constexpr size_t smax = std::numeric_limits<size_t>::max();
    Hits unsigned_top(50);
    async::parallel_for(smax - 50, smax, [&](size_t i) { unsigned_top.hit(i - (smax - 50)); }, {.grain = 1});
    EXPECT_EQ(unsigned_top.wrong(), 0u);
    Hits narrow(255);
    async::parallel_for(int8_t(-128), int8_t(127), [&](int8_t i) { narrow.hit(size_t(i + 128)); }, {.grain = 1});
    EXPECT_EQ(narrow.wrong(), 0u);

    // the whole range of int64_t by a quarter of it: min, -2^62, 0, 2^62
    std::vector<int64_t> up(4), down(4);
    constexpr int64_t quarter = int64_t(1) << 62;
    async::parallel_for(min, max, quarter, [&](int64_t i) { up[size_t((uint64_t(i) - uint64_t(min)) / uint64_t(quarter))] = i; }, {.grain = 1});
    EXPECT_EQ(up, (std::vector<int64_t>{min, -quarter, 0, quarter}));
    // and down from max: max, max - 2^62, ..., the last above min
    async::parallel_for(max, min, -quarter, [&](int64_t i) { down[size_t((uint64_t(max) - uint64_t(i)) / uint64_t(quarter))] = i; }, {.grain = 1});
    EXPECT_EQ(down, (std::vector<int64_t>{max, max - quarter, max - 2 * quarter, max - 3 * quarter}));
    // the step of INT64_MIN itself: max, then max - 2^63 = -1, the last above min
    std::vector<int64_t> halves(2);
    async::parallel_for(max, min, min, [&](int64_t i) { halves[i == max ? 0 : 1] = i; }, {.grain = 1});
    EXPECT_EQ(halves, (std::vector<int64_t>{max, -1}));
    std::atomic<int> calls = 0;
    async::parallel_for(min, max, min, [&](int64_t) { ++calls; });   // a step down from below end: empty
    EXPECT_EQ(calls.load(), 0);
    // SIZE_MAX indices by a third of them
    std::atomic<int> thirds = 0;
    async::parallel_for(size_t(0), smax, smax / 3 + 1, [&](size_t) { ++thirds; });
    EXPECT_EQ(thirds.load(), 3);
    // a reduction of 2^62 chunks: more partials than a dynamic_array holds,
    // refused before map is called
    std::atomic<int> maps = 0;
    EXPECT_THROW(async::parallel_reduce(min, max, 0LL, [&](int64_t) { ++maps; return 0LL; }, {.lanes = 2, .grain = 1}),
                 std::length_error);
    EXPECT_EQ(maps.load(), 0);
}

// The whole 64-bit range: 2^64 - 1 indices, cut into chunks without
// overflow; a body that throws from its first call ends it at once
TEST(Parallel_Tests, TheWholeRangeEndsAtTheFirstThrow) {
    constexpr int64_t max = std::numeric_limits<int64_t>::max();
    constexpr int64_t min = std::numeric_limits<int64_t>::min();
    std::atomic<int> calls = 0;
    EXPECT_THROW(async::parallel_for(min, max, [&](int64_t) {
        ++calls;
        throw std::runtime_error("stop");
    }), std::runtime_error);
    EXPECT_GE(calls.load(), 1);
    EXPECT_LE(calls.load(), (int)async::scheduler::workers());   // one call per lane at most: every lane's first index throws
    calls = 0;
    EXPECT_THROW(async::parallel_for(size_t(0), std::numeric_limits<size_t>::max(), [&](size_t) {
        ++calls;
        throw std::runtime_error("stop");
    }, {.grain = 1}), std::runtime_error);
    EXPECT_GE(calls.load(), 1);
}

// Lanes: one runs everything on the caller as lane 0; the default is the
// workers; more lanes than workers still give every index once and every
// lane number below the lanes asked
TEST(Parallel_Tests, TheNumberOfLanes) {
    auto caller = std::this_thread::get_id();
    std::atomic<int> elsewhere = 0;
    std::atomic<unsigned> top_lane = 0;
    Hits one(1000);
    async::parallel_for(1000, [&](int i, unsigned lane) {
        one.hit(size_t(i));
        elsewhere += std::this_thread::get_id() != caller;
        top_lane = std::max(top_lane.load(), lane);
    }, {.lanes = 1, .grain = 1});
    EXPECT_EQ(one.wrong(), 0u);
    EXPECT_EQ(elsewhere.load(), 0);
    EXPECT_EQ(top_lane.load(), 0u);

    const unsigned workers = async::scheduler::workers();
    std::atomic<unsigned> max_lane = 0;
    Hits defaults(100000);
    async::parallel_for(100000, [&](int i, unsigned lane) {
        defaults.hit(size_t(i));
        unsigned seen = max_lane.load();
        while (lane > seen && !max_lane.compare_exchange_weak(seen, lane)) {
        }
    });
    EXPECT_EQ(defaults.wrong(), 0u);
    EXPECT_LT(max_lane.load(), workers);

    const unsigned many = workers * 3 + 1;
    std::vector<std::atomic<int>> per_lane(many);
    Hits more(20000);
    async::parallel_for(20000, [&](int i, unsigned lane) {
        more.hit(size_t(i));
        ASSERT_LT(lane, many);
        per_lane[lane].fetch_add(1, std::memory_order_relaxed);
    }, {.lanes = many, .grain = 1});
    EXPECT_EQ(more.wrong(), 0u);
    int sum = 0;
    for (auto& c : per_lane) {
        sum += c.load();
    }
    EXPECT_EQ(sum, 20000);
}

// A grain past the range is one chunk: the caller runs it alone
TEST(Parallel_Tests, AGrainPastTheRangeIsOneChunk) {
    auto caller = std::this_thread::get_id();
    std::atomic<int> elsewhere = 0;
    Hits hits(500);
    async::parallel_for(500, [&](int i, unsigned lane) {
        hits.hit(size_t(i));
        elsewhere += std::this_thread::get_id() != caller || lane != 0;
    }, {.lanes = 8, .grain = 1000});
    EXPECT_EQ(hits.wrong(), 0u);
    EXPECT_EQ(elsewhere.load(), 0);
    Hits largest(10);
    async::parallel_for(10, [&](int i) { largest.hit(size_t(i)); }, {.grain = std::numeric_limits<size_t>::max()});
    EXPECT_EQ(largest.wrong(), 0u);
}

// A throw at the first, a middle and the last index: rethrown in the
// caller once every started call has returned (none runs after), and the
// indices not claimed yet are not started
TEST(Parallel_Tests, AThrowStopsTheLoopAndIsRethrown) {
    for (int at : {0, 500000, 999999}) {
        std::atomic<int> running = 0;
        std::atomic<int> calls = 0;
        try {
            async::parallel_for(1000000, [&](int i) {
                ++running;
                ++calls;
                if (i == at) {
                    --running;
                    throw std::runtime_error("index " + std::to_string(i));
                }
                --running;
            }, {.grain = 1});
            ADD_FAILURE() << "no exception for " << at;
        } catch (const std::runtime_error& e) {
            EXPECT_EQ(std::string(e.what()), "index " + std::to_string(at));
        }
        EXPECT_EQ(running.load(), 0);
        int after = calls.load();
        std::this_thread::sleep_for(20ms);
        EXPECT_EQ(calls.load(), after) << "a call after the rethrow, at " << at;
        if (at == 0) {
            EXPECT_LT(after, 1000000);
        }
    }
}

// Every index throws: one exception comes back, the others are dropped
TEST(Parallel_Tests, OneOfManyExceptionsIsRethrown) {
    EXPECT_THROW(async::parallel_for(10000, [](int) { throw std::logic_error("each"); }, {.grain = 1}), std::logic_error);
    EXPECT_THROW(async::parallel_reduce(0, 10000, 0, [](int i) -> int { if (i % 3 == 0) { throw std::logic_error("map"); } return i; }), std::logic_error);
    EXPECT_THROW(async::parallel_for_each(std::vector<int>(5000, 1), [](int) { throw std::logic_error("each"); }), std::logic_error);
}

// A loop inside the body of another, called on the worker that runs the
// outer index (or on the caller), its end waited for there: every pair
// once, no deadlock, three levels deep and with more lanes than workers at
// each
TEST(Parallel_Tests, NestedLoopsRunToTheEnd) {
    Hits hits(64 * 64);
    async::parallel_for(64, [&](int i) {
        async::parallel_for(64, [&](int j) { hits.hit(size_t(i * 64 + j)); }, {.grain = 1});
    }, {.grain = 1});
    EXPECT_EQ(hits.wrong(), 0u);

    const unsigned many = async::scheduler::workers() * 2;
    Hits deep(16 * 16 * 16);
    async::parallel_for(16, [&](int i) {
        async::parallel_for(16, [&](int j) {
            async::parallel_for(16, [&](int k) { deep.hit(size_t((i * 16 + j) * 16 + k)); }, {.lanes = many, .grain = 1});
        }, {.lanes = many, .grain = 1});
    }, {.lanes = many, .grain = 1});
    EXPECT_EQ(deep.wrong(), 0u);

    // a throw inside the inner loop comes out of the outer one
    EXPECT_THROW(async::parallel_for(32, [&](int i) {
        async::parallel_for(32, [&](int j) {
            if (i == 7 && j == 9) {
                throw std::runtime_error("inner");
            }
        });
    }, {.grain = 1}), std::runtime_error);
}

// The caller a task: its worker runs lane 0 and the call returns there once
// the other lanes' chunks have ended; a throw comes out of the call
TEST(Parallel_Tests, CalledFromATask) {
    EXPECT_EQ(async::spawn(sum_in_task(100000)).wait(), 100000LL * 99999 / 2);
    EXPECT_EQ(async::spawn(sum_in_task(0)).wait(), 0);
    EXPECT_EQ(async::spawn(sum_in_task(1)).wait(), 0);
    async::parallel_options o{.lanes = 4, .grain = 7};
    EXPECT_EQ(async::spawn(concat_in_task(1000, o)).wait(), concat_sequential(0, 1000, ""));
    for (int at : {0, 500, 999}) {
        EXPECT_THROW(async::spawn(throws_in_task(at)).wait(), std::runtime_error);
    }
    // many tasks at once, each with its loop: the workers are shared
    std::vector<async::task<long long>> tasks;
    for (int t = 0; t < 32; ++t) {
        tasks.push_back(async::spawn(sum_in_task(5000)));
    }
    for (auto& t : tasks) {
        EXPECT_EQ(t.wait(), 5000LL * 4999 / 2);
    }
    for (int round = 0; round < 20; ++round) {
        EXPECT_TRUE(async::spawn(lane_zero_is_the_task()).wait());
    }
}

// The caller a thread of the program's own, not the main one
TEST(Parallel_Tests, CalledFromAPlainThread) {
    Hits hits(10000);
    long long reduced = 0;
    std::thread t([&] {
        async::parallel_for(10000, [&](int i) { hits.hit(size_t(i)); });
        reduced = async::parallel_reduce(0LL, 10000LL, 0LL, [](long long i) { return i; });
    });
    t.join();
    EXPECT_EQ(hits.wrong(), 0u);
    EXPECT_EQ(reduced, 10000LL * 9999 / 2);
}

// A combine that does not commute (concatenation): the result is the left
// fold in the order of the indices, whatever lane ran which chunk, init on
// the left once
TEST(Parallel_Tests, ReduceKeepsTheOrderOfTheIndices) {
    auto map = [](int i) { return std::to_string(i) + ","; };
    auto concat = [](std::string a, const std::string& b) { return a + b; };
    for (size_t grain : {size_t(0), size_t(1), size_t(7), size_t(100000)}) {
        for (unsigned lanes : {0u, 1u, 3u, 64u}) {
            auto r = async::parallel_reduce(-50, 2000, std::string("init:"), map, concat, {.lanes = lanes, .grain = grain});
            EXPECT_EQ(r, concat_sequential(-50, 2000, "init:")) << grain << " " << lanes;
        }
    }
    // a managed result, its partials kept in a managed array meanwhile
    auto s = async::parallel_reduce(0, 300, sgcl::string(), [](int i) { return sgcl::string(std::to_string(i % 10)); },
                                    [](sgcl::string a, sgcl::string b) { return a + b; }, {.grain = 1});
    std::string expected;
    for (int i = 0; i < 300; ++i) {
        expected += std::to_string(i % 10);
    }
    EXPECT_EQ(std::string(s.data(), s.size()), expected);
    // the default combine adds
    EXPECT_EQ(async::parallel_reduce(int64_t(0), int64_t(1000000), int64_t(0), [](int64_t i) { return i; }),
              int64_t(1000000) * 999999 / 2);
}

// Every index exactly once, a million of them, at the default grain and at
// one index a claim
TEST(Parallel_Tests, EveryIndexOnce) {
    for (size_t grain : {size_t(0), size_t(1), size_t(13)}) {
        Hits hits(1000000);
        async::parallel_for(1000000, [&](int i) { hits.hit(size_t(i)); }, {.grain = grain});
        EXPECT_EQ(hits.wrong(), 0u) << grain;
    }
}

// The lane as an index of scratch space: no two calls at once share a lane
TEST(Parallel_Tests, LanesHaveTheirOwnScratch) {
    const unsigned lanes = async::scheduler::workers();
    std::vector<long long> sums(lanes, 0);
    std::vector<std::atomic<int>> inside(lanes);
    std::atomic<int> shared = 0;
    async::parallel_for(200000, [&](int i, unsigned lane) {
        if (inside[lane].fetch_add(1) != 0) {
            ++shared;
        }
        sums[lane] += i;
        inside[lane].fetch_sub(1);
    });
    EXPECT_EQ(shared.load(), 0);
    long long total = 0;
    for (auto s : sums) {
        total += s;
    }
    EXPECT_EQ(total, 200000LL * 199999 / 2);
}

// for_each: the elements of a std and a managed vector written in place,
// an rvalue range living until the call returns, the lane given when asked
TEST(Parallel_Tests, ForEachOverRanges) {
    std::vector<int> v(10000, 1);
    async::parallel_for_each(v, [](int& x) { x *= 3; });
    EXPECT_EQ(std::count(v.begin(), v.end(), 3), 10000);
    sgcl::vector<int> m(5000, 2);
    async::parallel_for_each(m, [](int& x, unsigned lane) { x += int(lane) * 0 + 1; }, {.grain = 1});
    EXPECT_EQ(std::count(m.begin(), m.end(), 3), 5000);
    std::atomic<long long> sum = 0;
    async::parallel_for_each(std::vector<int>(1000, 5), [&](int x) { sum += x; }, {.grain = 1});
    EXPECT_EQ(sum.load(), 5000);
    std::atomic<int> one = 0;
    int single[1] = {9};
    async::parallel_for_each(single, [&](int x) { one += x; });
    EXPECT_EQ(one.load(), 9);
}

// Every worker a caller at once: four tasks per worker, each a loop whose
// every index runs a nested loop, so that callers wait at their ends on
// workers while every worker is a caller; none deadlocks, every sum whole
TEST(Parallel_Tests, EveryWorkerACaller) {
    const unsigned workers = async::scheduler::workers();
    constexpr int n = 24;
    const long long whole = (long long)(n * n) * (n * n - 1) / 2;
    std::vector<async::task<long long>> tasks;
    for (unsigned t = 0; t < workers * 4; ++t) {
        tasks.push_back(async::spawn(nested_in_task(n)));
    }
    for (auto& t : tasks) {
        EXPECT_EQ(t.wait(), whole);
    }
}

// Many small calls in a row, more lanes than indices and a grain of one:
// lanes that start after their call has returned find nothing to claim and
// touch nothing of the caller (TSan and ASan watch the closure on the stack)
TEST(Parallel_Tests, LateLanesTouchNothingOfAReturnedCall) {
    long long total = 0;
    for (int round = 0; round < 2000; ++round) {
        std::atomic<int> local = 0;
        async::parallel_for(3, [&](int i) { local += i + 1; }, {.lanes = 8, .grain = 1});
        total += local.load();
    }
    EXPECT_EQ(total, 2000LL * 6);
}

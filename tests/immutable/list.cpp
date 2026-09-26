//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <algorithm>
#include <forward_list>
#include <list>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {
    // A list of n-1 .. 0 read from the front: n pushed in front of one another
    sgcl::immutable::list<int> counted(int n) {
        sgcl::immutable::list<int> l;
        for (int i = 0; i < n; ++i) {
            l = l.push_front(i);
        }
        return l;
    }

    template<class L, class O>
    bool same(const L& l, const O& o) {
        return l.size() == o.size() && std::equal(l.begin(), l.end(), o.begin());
    }

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(ImList_Tests, Empty) {
    sgcl::immutable::list<int> l;
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);
    EXPECT_EQ(l.begin(), l.end());
    EXPECT_EQ(l, sgcl::immutable::list<int>());
    static_assert(sizeof(sgcl::immutable::list<int>) == 2 * sizeof(void*));
}

TEST(ImList_Tests, PushFrontPopFrontFront) {
    sgcl::immutable::list<int> a;
    auto b = a.push_front(1);
    auto c = b.push_front(2);
    auto d = c.emplace_front(3);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(b.front(), 1);
    EXPECT_EQ(c.front(), 2);
    EXPECT_EQ(d.front(), 3);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d.pop_front(), c);        // the rest of the chain: the same cells
    EXPECT_EQ(d.pop_front().pop_front(), b);
    EXPECT_EQ(b.pop_front(), a);
    EXPECT_EQ(std::vector<int>(d.begin(), d.end()), (std::vector<int>{3, 2, 1}));
}

TEST(ImList_Tests, RangeAndListConstructorsKeepTheOrder) {
    sgcl::immutable::list<int> il = {1, 2, 3};
    EXPECT_EQ(std::vector<int>(il.begin(), il.end()), (std::vector<int>{1, 2, 3}));
    std::vector<int> v = {4, 5, 6};
    sgcl::immutable::list<int> from_bidi(v.begin(), v.end());
    EXPECT_EQ(std::vector<int>(from_bidi.begin(), from_bidi.end()), v);
    std::forward_list<int> f = {7, 8, 9};          // forward only: through a buffer
    sgcl::immutable::list<int> from_fwd(f.begin(), f.end());
    EXPECT_EQ(std::vector<int>(from_fwd.begin(), from_fwd.end()), (std::vector<int>{7, 8, 9}));
    sgcl::immutable::list<int> from_range(v);              // a range of ints
    EXPECT_EQ(from_range, from_bidi);
    sgcl::immutable::list copied(from_range);              // a copy: the same chain
    EXPECT_EQ(copied, from_range);
    sgcl::immutable::list<std::string> words(std::vector<const char*>{"a", "b"});   // a range of what the elements are made of
    EXPECT_EQ(words.front(), "a");
    sgcl::immutable::list deduced(v.begin(), v.end());
    static_assert(std::is_same_v<decltype(deduced), sgcl::immutable::list<int>>);
}

TEST(ImList_Tests, OldVersionUnchanged) {
    auto l = counted(5);   // 4 3 2 1 0
    auto m = l.push_front(9);
    auto n = l.pop_front();
    EXPECT_EQ(std::vector<int>(l.begin(), l.end()), (std::vector<int>{4, 3, 2, 1, 0}));
    EXPECT_EQ(std::vector<int>(m.begin(), m.end()), (std::vector<int>{9, 4, 3, 2, 1, 0}));
    EXPECT_EQ(std::vector<int>(n.begin(), n.end()), (std::vector<int>{3, 2, 1, 0}));
    EXPECT_EQ(m.pop_front(), l);
    EXPECT_EQ(l.pop_front(), n);
}

TEST(ImList_Tests, Reverse) {
    sgcl::immutable::list<int> l = {1, 2, 3, 4};
    auto r = l.reverse();
    EXPECT_EQ(std::vector<int>(r.begin(), r.end()), (std::vector<int>{4, 3, 2, 1}));
    EXPECT_EQ(r.reverse(), l);
    EXPECT_TRUE(sgcl::immutable::list<int>().reverse().empty());
}

TEST(ImList_Tests, ComparisonByElementsAndByChain) {
    sgcl::immutable::list<int> a = {1, 2, 3};
    sgcl::immutable::list<int> b = {1, 2, 3};
    EXPECT_EQ(a, b);                   // different chains, equal elements
    EXPECT_NE(a, a.push_front(0));
    EXPECT_NE(a, a.pop_front());
    auto c = a;
    EXPECT_EQ(c, a);                   // the same chain
}

TEST(ImList_Tests, Sharing) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::list<int> l, m, n;
    off_frame([&] {
        l = counted(1000);
    });
    EXPECT_EQ(collector::get_live_object_count() - before, 1000u);   // a cell per element, nothing else
    off_frame([&] {
        m = l.push_front(-1);
        n = l.pop_front();
    });
    EXPECT_EQ(collector::get_live_object_count() - before, 1001u);   // one cell for the push, none for the pop
    l = {};
    m = {};
    n = {};
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ImList_Tests, ElementsDestroyedWithTheirCells) {
    settle();
    EXPECT_EQ(Int::counter, 0u);
    sgcl::immutable::list<Int> l, m;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            l = l.push_front(Int(i));
        }
        m = l.pop_front().pop_front();   // shares 98 cells with l
    });
    settle();
    EXPECT_EQ(Int::counter, 100u);
    off_frame([&] { l = {}; });
    settle();
    EXPECT_EQ(Int::counter, 98u);        // the two cells only l reached
    off_frame([&] { m = {}; });
    settle();
    EXPECT_EQ(Int::counter, 0u);
}

TEST(ImList_Tests, TrackedElements) {
    settle();
    sgcl::immutable::list<tracked_ptr<Int>> l;
    off_frame([&] {
        for (int i = 0; i < 50; ++i) {
            l = l.push_front(make_tracked<Int>(i));
        }
    });
    settle();
    EXPECT_EQ(Int::counter, 50u);       // traced through the cells
    off_frame([&] {                     // the walk in a frame of its own: a pointer it leaves would root an element
        int expected = 49;
        for (auto& p : l) {
            EXPECT_EQ(*p, expected--);
        }
        l = {};
    });
    settle();
    EXPECT_EQ(Int::counter, 0u);
}

TEST(ImList_Tests, InsideManagedObject) {
    struct Holder {
        sgcl::immutable::list<int> items;
    };
    settle();
    sgcl::tracked_ptr h = make_tracked<Holder>();
    off_frame([&] {
        h->items = counted(10);
    });
    settle();
    EXPECT_EQ(h->items.size(), 10u);
    EXPECT_EQ(h->items.front(), 9);
}

TEST(ImList_Tests, ALongListDiesInOneSweep) {
    settle();
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::immutable::list<int> l = counted(1000000);   // a million cells, dropped at once: no chain of destructors
        EXPECT_EQ(l.size(), 1000000u);
    });
    settle();
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ImList_Tests, StressAgainstOracle) {
    std::mt19937 rng(11);
    sgcl::immutable::list<int> l;
    std::list<int> o;
    sgcl::immutable::list<int> kept;
    std::list<int> kept_o;
    for (int step = 0; step < 60000; ++step) {
        switch (rng() % 8) {
            case 0: case 1: case 2: case 3: {
                int x = int(rng());
                l = l.push_front(x);
                o.push_front(x);
                break;
            }
            case 4: case 5:
                if (!o.empty()) {
                    l = l.pop_front();
                    o.pop_front();
                }
                break;
            case 6:
                if (step % 3000 == 0) {
                    l = l.reverse();
                    o.reverse();
                }
                break;
            default:
                if (step % 5000 == 0) {
                    kept = l;
                    kept_o = o;
                    collector::force_collect();
                }
                break;
        }
        if (step % 997 == 0) {
            ASSERT_TRUE(same(l, o)) << step;
            ASSERT_TRUE(same(kept, kept_o)) << step;
        }
    }
    EXPECT_TRUE(same(l, o));
}

TEST(ImList_Tests, ReadersUnderCopyOnWrite) {
    sgcl::concurrent::copy_on_write<sgcl::immutable::list<int>> current;
    sgcl::atomic<bool> stop = {false};
    sgcl::atomic<long> reads = {0};
    sgcl::atomic<bool> bad = {false};
    off_frame([&] {
        std::vector<std::thread> readers;
        for (int t = 0; t < 6; ++t) {
            readers.emplace_back([&] {
                size_t last = 0;
                while (!stop.load(std::memory_order_relaxed)) {
                    auto s = current.load();   // a snapshot: the list as it was
                    if (s->size() < last) {
                        bad = true;
                    }
                    last = s->size();
                    int expected = int(s->size()) - 1;
                    for (int x : *s) {
                        if (x != expected--) {
                            bad = true;
                        }
                    }
                    ++reads;
                }
            });
        }
        for (int i = 0; i < 20000; ++i) {
            current.update([i](auto& l) { l = l.push_front(i); });   // O(1): one cell, the rest shared
            if (i % 4000 == 0) {
                collector::force_collect();
            }
        }
        stop = true;
        for (auto& r : readers) {
            r.join();
        }
    });
    EXPECT_FALSE(bad.load());
    EXPECT_GT(reads.load(), 0);
    EXPECT_EQ(current.load()->size(), 20000u);
}

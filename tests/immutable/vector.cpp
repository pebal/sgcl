//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <random>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    // A vector of 0..n-1, built by push_back
    sgcl::immutable::vector<int> counted(int n) {
        sgcl::immutable::vector<int> v;
        for (int i = 0; i < n; ++i) {
            v = v.push_back(i);
        }
        return v;
    }

    template<class V, class O>
    bool same(const V& v, const O& o) {
        return v.size() == o.size() && std::equal(v.begin(), v.end(), o.begin());
    }

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(ImVector_Tests, Empty) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.begin(), v.end());
    EXPECT_EQ(v.depth(), 0u);
    EXPECT_EQ(collector::get_live_object_count(), before);   // no node for an empty vector
    EXPECT_THROW(v.at(0), std::out_of_range);
    EXPECT_THROW(v.set(0, 1), std::out_of_range);
    EXPECT_EQ(v, sgcl::immutable::vector<int>());
}

TEST(ImVector_Tests, ListAndRangeConstructors) {
    sgcl::immutable::vector<int> v = {1, 2, 3};
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v.front(), 1);
    EXPECT_EQ(v.back(), 3);
    EXPECT_EQ(v.at(1), 2);
    EXPECT_THROW(v.at(3), std::out_of_range);
    std::vector<int> o(5000);
    std::iota(o.begin(), o.end(), 0);
    sgcl::immutable::vector w(o.begin(), o.end());   // deduced
    static_assert(std::is_same_v<decltype(w), sgcl::immutable::vector<int>>);
    EXPECT_TRUE(same(w, o));
    EXPECT_EQ(w.depth(), 2u);   // 4968 in the trie: two levels of branches
    std::vector<int> back(w.begin(), w.end());
    EXPECT_EQ(back, o);
}

// The old version is what it was after every operation on it
TEST(ImVector_Tests, OldVersionUnchanged) {
    auto v = counted(100);
    auto pushed = v.push_back(100);
    auto popped = v.pop_back();
    auto set = v.set(50, -1);
    EXPECT_EQ(v.size(), 100u);
    EXPECT_EQ(pushed.size(), 101u);
    EXPECT_EQ(popped.size(), 99u);
    EXPECT_EQ(set.size(), 100u);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(v[size_t(i)], i);
        EXPECT_EQ(pushed[size_t(i)], i);
        if (i < 99) {
            EXPECT_EQ(popped[size_t(i)], i);
        }
        EXPECT_EQ(set[size_t(i)], i == 50 ? -1 : i);
    }
    EXPECT_EQ(pushed.back(), 100);
    EXPECT_EQ(popped.back(), 98);
    EXPECT_NE(v, set);
    EXPECT_EQ(v, set.set(50, 50));
}

// Sizes across the boundaries of the trie's levels: the tail alone, one
// leaf in the trie, a full root, a second level, a third
TEST(ImVector_Tests, LevelBoundaries) {
    for (int n : {31, 32, 33, 64, 65, 1023, 1024, 1025, 1056, 1057, 32768, 32769, 32800, 32801, 40000}) {
        auto v = counted(n);
        std::vector<int> o((size_t)n);
        std::iota(o.begin(), o.end(), 0);
        EXPECT_TRUE(same(v, o)) << n;
        EXPECT_EQ(v.back(), n - 1);
        for (int i = 0; i < n; i += 7) {
            EXPECT_EQ(v[size_t(i)], i) << n;
        }
        unsigned depth = n <= 32 ? 0 : n <= 32 + 1024 ? 1 : n <= 32 + 32768 ? 2 : 3;
        EXPECT_EQ(v.depth(), depth) << n;
        auto w = v.push_back(n);
        EXPECT_EQ(w.size(), size_t(n + 1));
        EXPECT_EQ(w.back(), n);
        EXPECT_EQ(w.pop_back(), v);
    }
}

TEST(ImVector_Tests, PopBackToEmpty) {
    for (int n : {1, 32, 33, 1056, 1057, 3000}) {
        auto v = counted(n);
        std::vector<int> o((size_t)n);
        std::iota(o.begin(), o.end(), 0);
        while (!v.empty()) {
            v = v.pop_back();
            o.pop_back();
            EXPECT_TRUE(same(v, o)) << n << " " << v.size();
        }
        EXPECT_EQ(v.depth(), 0u);
        EXPECT_EQ(v, sgcl::immutable::vector<int>());
    }
}

TEST(ImVector_Tests, SetEveryPosition) {
    const int n = 2100;
    auto v = counted(n);
    auto w = v;
    for (int i = 0; i < n; ++i) {
        w = w.set(size_t(i), -i);
    }
    for (int i = 0; i < n; ++i) {
        EXPECT_EQ(v[size_t(i)], i);
        EXPECT_EQ(w[size_t(i)], -i);
    }
}

// A tail at the edge of full: the push that fills it makes a leaf of the
// trie's kind, every other change of a tail a tail's, and a full tail made
// by a set is hung on the trie as a tail's; every version, of ints and of
// strings (the leaf with a destructor), reads what the oracle holds
template<class T, class Make>
void tail_at_the_edge(Make make) {
    for (int n : {30, 31, 32, 62, 63, 64, 1054, 1055, 1056}) {
        sgcl::immutable::vector<T> base;
        std::vector<T> o;
        for (int i = 0; i < n; ++i) {
            base = base.push_back(make(i));
            o.push_back(make(i));
        }
        sgcl::immutable::vector<T> versions[8];             // on the stack: a tracked pointer never in a std container
        std::vector<std::vector<T>> oracles;
        for (int b = 0; b < 8; ++b) {                // eight pushes from one base: eight tails filled or begun
            versions[b] = base.push_back(make(1000 + b));
            oracles.push_back(o);
            oracles.back().push_back(make(1000 + b));
        }
        auto set_tail = base.set(size_t(n - 1), make(-1));   // a set of the tail, full or not
        auto so = o;
        so[size_t(n - 1)] = make(-1);
        for (int i = 0; i < 70; ++i) {               // past two more leaves: the tail made by the set hung on the trie if it was full
            set_tail = set_tail.push_back(make(2000 + i));
            so.push_back(make(2000 + i));
        }
        EXPECT_TRUE(same(set_tail, so)) << n;
        auto popped = base.pop_back().push_back(make(-2)).push_back(make(-3));
        auto po = o;
        po.pop_back();
        po.push_back(make(-2));
        po.push_back(make(-3));
        EXPECT_TRUE(same(popped, po)) << n;
        EXPECT_TRUE(same(base, o)) << n;
        for (size_t b = 0; b < 8; ++b) {
            EXPECT_TRUE(same(versions[b], oracles[b])) << n << " " << b;
        }
        collector::force_collect();
    }
}

TEST(ImVector_Tests, TailAtTheEdgeOfFull) {
    tail_at_the_edge<int>([](int i) { return i; });
    tail_at_the_edge<std::string>([](int i) { return std::string(40, char('a' + (i & 15))) + std::to_string(i); });
}

// The tails a push_back copies and drops lie on pages of their own: a
// million elements built one push at a time keep the pages their leaves
// and branches need, where the dead tails among the leaves once kept
// eight times that (73 MB of pages for 8 MB of elements)
TEST(ImVector_Tests, PushBackKeepsPagesNearTheElements) {
    const long n = 1 << 20;
    settle();
    auto live0 = collector::get_statistics().live_bytes;
    sgcl::immutable::vector<long> v;
    for (long i = 0; i < n; ++i) {
        v = v.push_back(i);
    }
    settle();
    auto live = collector::get_statistics().live_bytes - live0;
    EXPECT_LT(live, 2 * n * sizeof(long)) << live / 1048576.0 << " MB of pages for " << n * sizeof(long) / 1048576.0 << " MB of elements";
    EXPECT_EQ(v.size(), size_t(n));
    EXPECT_EQ(v[size_t(n / 3)], n / 3);
}

TEST(ImVector_Tests, IteratorsAndRanges) {
    auto v = counted(5000);
    static_assert(std::random_access_iterator<sgcl::immutable::vector<int>::const_iterator>);
    static_assert(std::ranges::random_access_range<sgcl::immutable::vector<int>>);
    EXPECT_EQ(std::ranges::find(v, 4321) - v.begin(), 4321);
    EXPECT_EQ(*std::ranges::lower_bound(v, 1234), 1234);
    EXPECT_TRUE(std::ranges::is_sorted(v));
    EXPECT_EQ(std::accumulate(v.begin(), v.end(), 0L), 4999L * 5000 / 2);
    auto it = v.begin() + 100;
    EXPECT_EQ(*it, 100);
    EXPECT_EQ(it[50], 150);
    EXPECT_EQ(*(it - 1), 99);
    EXPECT_EQ(v.end() - v.begin(), 5000);
    EXPECT_LT(v.begin(), v.end());
    it -= 100;
    EXPECT_EQ(it, v.begin());
    it += 4999;
    EXPECT_EQ(*it++, 4999);
    EXPECT_EQ(it, v.end());
    --it;
    EXPECT_EQ(*it--, 4999);
    EXPECT_EQ(*it, 4998);
    std::vector<int> r(v.rbegin(), v.rend());
    EXPECT_EQ(r.front(), 4999);
    EXPECT_EQ(r.back(), 0);
    int sum = 0;
    for (int x : v) {
        sum += x;
    }
    EXPECT_EQ(sum, 4999 * 5000 / 2);
    std::vector<int> e;
    for (int x : v | std::views::filter([](int x) { return x % 2 == 0; }) | std::views::take(3)) {
        e.push_back(x);
    }
    EXPECT_EQ(e, (std::vector<int>{0, 2, 4}));
}

// Two versions differing in one element share all but a path: a handful
// of nodes more, not a second vector
TEST(ImVector_Tests, Sharing) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::vector<int> v, w, x;
    off_frame([&] {
        v = counted(100001);
    });
    const size_t one = collector::get_live_object_count() - before;
    EXPECT_GT(one, 3125u);       // 3125 full leaves and the branches over them, the tail
    EXPECT_LT(one, 3125u + 200);
    off_frame([&] {
        w = v.set(50000, -1);
    });
    const size_t two = collector::get_live_object_count() - before;
    EXPECT_EQ(two, one + 4);     // the leaf and the three branches above it
    off_frame([&] {
        x = v.push_back(7);
    });
    EXPECT_EQ(collector::get_live_object_count() - before, two + 1);   // the tail copied
    v = {};
    w = {};
    x = {};
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// The elements of a leaf die with it: Int counts its live objects
TEST(ImVector_Tests, ElementsDestroyed) {
    settle();
    EXPECT_EQ(Int::counter, 0u);
    sgcl::immutable::vector<Int> v, w;
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {
            v = v.push_back(Int(i));
        }
    });
    collector::force_collect(true);
    EXPECT_EQ(Int::counter, 1000u);        // the garbage tails collected, the elements with them
    off_frame([&] {
        w = v.set(10, Int(-1));
    });
    collector::force_collect(true);
    EXPECT_EQ(Int::counter, 1032u);        // one leaf copied: 32 elements more
    off_frame([&] {                        // the reads and the release off the frame: a stale word of a
        EXPECT_EQ(v[10], 10);              // leaf or of the handle spilled in this frame would hold it
        EXPECT_EQ(w[10], -1);              // (the conservative scan; the frames grow under a sanitizer)
        v = {};
    });
    settle();
    EXPECT_EQ(Int::counter, 1000u);        // the old leaf dead with the version
    off_frame([&] {
        w = {};
    });
    settle();
    EXPECT_EQ(Int::counter, 0u);
}

// An element holding a tracked_ptr keeps its object while any version
// reaches it, and lets it go with the last one
TEST(ImVector_Tests, TrackedElements) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::vector<tracked_ptr<Baz>> v, w;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            v = v.push_back(make_tracked<Baz>(i));
        }
    });
    EXPECT_EQ(collector::get_live_object_count() - before, 100u + 3 + 1 + 1);   // the Baz, three leaves, the tail, the root
    off_frame([&] {
        w = v.set(5, make_tracked<Baz>(-5));
    });
    EXPECT_EQ(collector::get_live_object_count() - before, 100u + 5 + 1 + 2);   // one Baz and a leaf and a root more
    off_frame([&] {                        // the raw pointers of the checks stay out of this frame
        EXPECT_EQ(v[5]->value, 5);
        EXPECT_EQ(w[5]->value, -5);
        EXPECT_EQ(v[6].get(), w[6].get());   // the same object in both
    });
    v = {};
    EXPECT_EQ(collector::get_live_object_count() - before, 100u + 5);   // the old leaf, the old root and Baz 5 gone
    w = {};
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ImVector_Tests, InsideManagedObject) {
    struct Holder {
        sgcl::immutable::vector<tracked_ptr<Baz>> items;
        sgcl::immutable::vector<std::string> names;
    };
    const size_t before = collector::get_live_object_count();
    tracked_ptr h = make_tracked<Holder>();
    off_frame([&] {
        for (int i = 0; i < 50; ++i) {
            h->items = h->items.push_back(make_tracked<Baz>(i));
            h->names = h->names.push_back(std::to_string(i));
        }
    });
    collector::force_collect(true);
    off_frame([&] {
        EXPECT_EQ(h->items.size(), 50u);
        EXPECT_EQ(h->items[49]->value, 49);
        EXPECT_EQ(h->names[49], "49");
    });
    h = nullptr;
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ImVector_Tests, Strings) {
    sgcl::immutable::vector<std::string> v = {"a", "b"};
    auto w = v.push_back("c").set(0, std::string("z"));
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(w[0], "z");
    EXPECT_EQ(w[2], "c");
    sgcl::immutable::vector<sgcl::string> s;
    for (int i = 0; i < 100; ++i) {
        s = s.push_back(sgcl::string("s") + std::to_string(i).c_str());
    }
    EXPECT_EQ(s[99], "s99");
    EXPECT_EQ(s.pop_back().size(), 99u);
}

// Random operations against std::vector, the collector running meanwhile
TEST(ImVector_Tests, StressAgainstOracle) {
    std::mt19937 rng(7);
    sgcl::immutable::vector<int> v;
    std::vector<int> o;
    sgcl::immutable::vector<int> kept;
    std::vector<int> kept_o;
    for (int step = 0; step < 60000; ++step) {
        switch (rng() % 8) {
            case 0: case 1: case 2: case 3: {
                int x = int(rng());
                v = v.push_back(x);
                o.push_back(x);
                break;
            }
            case 4: case 5:
                if (!o.empty()) {
                    v = v.pop_back();
                    o.pop_back();
                }
                break;
            case 6:
                if (!o.empty()) {
                    size_t i = rng() % o.size();
                    int x = int(rng());
                    v = v.set(i, x);
                    o[i] = x;
                }
                break;
            default:
                if (step % 5000 == 0) {
                    kept = v;
                    kept_o = o;
                    collector::force_collect();
                }
                break;
        }
        if (step % 997 == 0) {
            ASSERT_TRUE(same(v, o)) << step;
            ASSERT_TRUE(same(kept, kept_o)) << step;
        }
    }
    EXPECT_TRUE(same(v, o));
    EXPECT_TRUE(same(kept, kept_o));
}

// Readers on several threads hold versions while one thread publishes new
// ones through a copy_on_write: every snapshot is whole and stays what it is
TEST(ImVector_Tests, ReadersUnderCopyOnWrite) {
    sgcl::concurrent::copy_on_write<sgcl::immutable::vector<int>> current;
    sgcl::atomic<bool> stop = {false};
    sgcl::atomic<long> reads = {0};
    sgcl::atomic<bool> bad = {false};
    off_frame([&] {
        std::vector<std::thread> readers;
        for (int t = 0; t < 6; ++t) {
            readers.emplace_back([&] {
                size_t last = 0;
                while (!stop.load(std::memory_order_relaxed)) {
                    auto s = current.load();   // a snapshot: the vector as it was
                    if (s->size() < last) {
                        bad = true;
                    }
                    last = s->size();
                    int expected = 0;
                    for (int x : *s) {
                        if (x != expected++) {
                            bad = true;
                        }
                    }
                    ++reads;
                }
            });
        }
        for (int i = 0; i < 20000; ++i) {
            current.update([i](auto& v) { v = v.push_back(i); });   // O(1): the tail copied, the rest shared
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

// The same through an atomic pointer to a version
TEST(ImVector_Tests, ReadersUnderAtomic) {
    sgcl::atomic<tracked_ptr<sgcl::immutable::vector<int>>> current(make_tracked<sgcl::immutable::vector<int>>());
    sgcl::atomic<bool> stop = {false};
    sgcl::atomic<bool> bad = {false};
    off_frame([&] {
        std::vector<std::thread> readers;
        for (int t = 0; t < 4; ++t) {
            readers.emplace_back([&] {
                while (!stop.load(std::memory_order_relaxed)) {
                    tracked_ptr<sgcl::immutable::vector<int>> s = current.load();   // the version: never modified, only replaced
                    long sum = 0;
                    for (int x : *s) {
                        sum += x;
                    }
                    auto n = long(s->size());
                    if (sum != n * (n - 1) / 2) {
                        bad = true;
                    }
                }
            });
        }
        for (int i = 0; i < 10000; ++i) {
            auto v = current.load()->push_back(i);
            current.store(make_tracked<sgcl::immutable::vector<int>>(std::move(v)));
        }
        stop = true;
        for (auto& r : readers) {
            r.join();
        }
    });
    EXPECT_FALSE(bad.load());
    EXPECT_EQ(current.load()->size(), 10000u);
}

// Out of memory throws nothing (it ends the program): a change is
// noexcept as far as the elements' copies and constructions are
TEST(ImVector_Tests, NoexceptFollowsTheElements) {
    using V = sgcl::immutable::vector<int>;
    using S = sgcl::immutable::vector<std::string>;   // a copy that can throw, a move that cannot
    std::vector<int> ints;
    int i = 0;
    std::string str;
    static_assert(noexcept(V().push_back(i)) && noexcept(V().push_back(1)) && noexcept(V().emplace_back(1)));
    static_assert(noexcept(V().pop_back()));
    static_assert(noexcept(V(ints.begin(), ints.end())) && noexcept(V({1, 2})));
    static_assert(!noexcept(V().set(0, 1)) && !noexcept(V().at(0)));   // out_of_range
    static_assert(!noexcept(S().push_back(str)) && !noexcept(S().push_back(std::string())));   // the tail's copy
    static_assert(!noexcept(S().emplace_back()) && !noexcept(S().pop_back()));
    std::istringstream in("1 2");
    static_assert(!noexcept(V(std::istream_iterator<int>(in), std::istream_iterator<int>())));   // the iterator reads a stream
    SUCCEED();
}

// A vector moved from keeps its version, as the copy does (vector.md (5),
// operator_assign.md (2)), and every member works on it; an assignment of
// a vector to itself, by a copy or a move, changes nothing
TEST(ImVector_Tests, MovedFromAndSelfAssignment) {
    for (int n : {0, 1, 32, 33, 1057}) {
        auto v = counted(n);
        const auto copy = v;
        auto moved = std::move(v);
        EXPECT_EQ(v, copy) << n;
        EXPECT_EQ(moved, copy) << n;
        EXPECT_EQ(v.size(), size_t(n));
        EXPECT_EQ(std::distance(v.begin(), v.end()), n);
        EXPECT_EQ(v.push_back(n).size(), size_t(n + 1));
        if (n > 0) {
            EXPECT_EQ(v.back(), n - 1);
            EXPECT_EQ(v.pop_back().size(), size_t(n - 1));
            EXPECT_EQ(v.set(0, -1)[0], -1);
        }
        sgcl::immutable::vector<int> w;
        w = std::move(v);
        EXPECT_EQ(v, copy) << n;
        EXPECT_EQ(w, copy) << n;
        auto& alias = v;   // through a reference: the compiler's warning would see `v = v`
        v = alias;
        EXPECT_EQ(v, copy) << n;
        v = std::move(alias);
        EXPECT_EQ(v, copy) << n;
        EXPECT_EQ(v.depth(), copy.depth());
    }
}

// The edges of an empty vector and of one of one element: the iterators
// meet, the reverse ones too; a push into the empty vector and the pop
// back to empty, which holds no node again
TEST(ImVector_Tests, EmptyAndOneElement) {
    sgcl::immutable::vector<int> e;
    EXPECT_EQ(e.cbegin(), e.cend());
    EXPECT_EQ(e.rbegin(), e.rend());
    EXPECT_EQ(e.crbegin(), e.crend());
    EXPECT_EQ(e.end() - e.begin(), 0);
    EXPECT_EQ(sgcl::immutable::vector<int>::const_iterator(), sgcl::immutable::vector<int>::const_iterator());
    EXPECT_EQ(sgcl::immutable::vector<int>({}), e);
    std::vector<int> none;
    EXPECT_TRUE(sgcl::immutable::vector<int>(none.begin(), none.end()).empty());
    std::istringstream nothing("");
    EXPECT_TRUE(sgcl::immutable::vector<int>(std::istream_iterator<int>(nothing), std::istream_iterator<int>()).empty());
    std::istringstream three("1 2 3");   // a range read once
    sgcl::immutable::vector<int> read(std::istream_iterator<int>(three), std::istream_iterator<int>{});
    EXPECT_EQ(std::vector<int>(read.begin(), read.end()), (std::vector<int>{1, 2, 3}));
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        auto one = e.push_back(7);
        EXPECT_EQ(one.size(), 1u);
        EXPECT_EQ(one.front(), 7);
        EXPECT_EQ(&one.front(), &one.back());
        EXPECT_EQ(one[0], 7);
        EXPECT_EQ(one.at(0), 7);
        EXPECT_THROW(one.at(1), std::out_of_range);
        EXPECT_EQ(one.depth(), 0u);
        EXPECT_EQ(*one.rbegin(), 7);
        EXPECT_EQ(std::next(one.rbegin()), one.rend());
        EXPECT_EQ(one.set(0, 8)[0], 8);
        EXPECT_EQ(one[0], 7);
        auto none_again = one.pop_back();
        EXPECT_TRUE(none_again.empty());
        EXPECT_EQ(none_again, e);
        EXPECT_EQ(none_again.depth(), 0u);
        EXPECT_EQ(one.emplace_back(8).back(), 8);
    });
    settle();
    EXPECT_EQ(collector::get_live_object_count(), before);   // the tails of the frame collected, none for the empty vector
}

// The positions at and past the end: at and set throw out_of_range for
// size() and for the largest size_type, whatever the vector's shape, and
// the vector is left as it was (at.md, set.md)
TEST(ImVector_Tests, PositionsPastTheEnd) {
    for (int n : {0, 1, 32, 33, 1056, 1057}) {
        auto v = counted(n);
        for (size_t i : {size_t(n), size_t(n) + 1, size_t(n) + 32, SIZE_MAX}) {
            EXPECT_THROW(v.at(i), std::out_of_range) << n << " " << i;
            EXPECT_THROW(v.set(i, -1), std::out_of_range) << n << " " << i;
            const int x = -1;
            EXPECT_THROW(v.set(i, x), std::out_of_range) << n << " " << i;
        }
        EXPECT_EQ(v, counted(n));
        if (n > 0) {
            EXPECT_EQ(v.at(size_t(n - 1)), n - 1);   // the last position, in the tail
            EXPECT_EQ(v.set(size_t(n - 1), -1).back(), -1);
        }
    }
}

// An argument that is an element of the vector itself: push_back,
// emplace_back and set read it from the version they were called on,
// which holds it while the new one is made, and a result assigned to the
// same variable replaces that version only after; a vector built from
// its own iterators is equal to it. Strings, whose copy reads what it
// copies, at every shape of the tail
TEST(ImVector_Tests, ArgumentsFromTheVectorItself) {
    auto text = [](int i) {
        return std::string(24, char('a' + i % 26)) + std::to_string(i);
    };
    for (int n : {1, 31, 32, 33, 64, 1056, 1057}) {
        sgcl::immutable::vector<std::string> v;
        std::vector<std::string> o;
        for (int i = 0; i < n; ++i) {
            v = v.push_back(text(i));
            o.push_back(text(i));
        }
        auto last = size_t(n - 1);
        auto pushed = v.push_back(v.back());
        EXPECT_EQ(pushed.back(), o.back()) << n;
        EXPECT_EQ(pushed.size(), size_t(n + 1));
        EXPECT_EQ(v.push_back(v.front()).back(), o.front()) << n;
        EXPECT_EQ(v.emplace_back(v[last / 2]).back(), o[last / 2]) << n;
        EXPECT_EQ(v.set(0, v[last])[0], o[last]) << n;
        EXPECT_EQ(v.set(last, v[0])[last], o[0]) << n;
        EXPECT_EQ(v.set(last, v[last]), v) << n;
        sgcl::immutable::vector<std::string> again(v.begin(), v.end());
        EXPECT_EQ(again, v) << n;
        auto w = v;
        auto wo = o;
        w = w.push_back(w.back());   // the variable's own version read, then replaced
        wo.push_back(wo.back());
        w = w.set(0, w[last]);
        wo[0] = wo[last];
        w = w.pop_back().push_back(w.front());
        wo.pop_back();
        wo.push_back(wo.front());
        EXPECT_TRUE(same(w, wo)) << n;
        EXPECT_TRUE(same(v, o)) << n;
    }
}

// Elements without a default constructor and with a destructor: a leaf
// constructs them one by one in its storage, a full leaf of the trie and
// a tail alike
TEST(ImVector_Tests, ElementsWithoutADefaultConstructor) {
    struct Named {
        explicit Named(int i)
        : name(std::to_string(i)) {
        }

        std::string name;
    };
    static_assert(!std::is_default_constructible_v<Named>);
    sgcl::immutable::vector<Named> v;
    for (int i : range(100)) {
        v = v.emplace_back(i);
    }
    v = v.set(5, Named(-5)).pop_back();
    EXPECT_EQ(v.size(), 99u);
    EXPECT_EQ(v[5].name, "-5");
    EXPECT_EQ(v.back().name, "98");
    std::vector<Named> o = {Named(1), Named(2)};
    EXPECT_EQ(sgcl::immutable::vector<Named>(o.begin(), o.end())[1].name, "2");
}

// A walk backwards and from the end, across the tail's boundary and the
// leaves': the iterator looks its leaf up again whenever it leaves one
TEST(ImVector_Tests, IteratorsAcrossTheBoundaries) {
    for (int n : {1, 31, 32, 33, 64, 65, 1056, 1057}) {
        auto v = counted(n);
        std::vector<int> back(v.rbegin(), v.rend());
        std::vector<int> o((size_t)n);
        std::iota(o.rbegin(), o.rend(), 0);
        EXPECT_EQ(back, o) << n;
        EXPECT_EQ(v.end()[-1], n - 1);
        EXPECT_EQ(*(v.end() - n), 0);
        auto it = v.end();
        for (int i = n - 1; i >= 0; --i) {
            --it;
            ASSERT_EQ(*it, i) << n;
        }
        EXPECT_EQ(it, v.begin());
        for (int i = 0; i < n; i += 31) {   // jumps of 31: into another leaf at nearly every step
            ASSERT_EQ(v.begin()[i], i) << n;
            ASSERT_EQ(*(v.end() - (n - i)), i) << n;
        }
    }
}

// update: f of the element at a position, set in its place, f called
// once: at the first and the last position, in the tail and in the trie,
// at the sizes around a leaf and a level; past the end out_of_range and f
// not called; f that reads the vector itself; f that throws leaves the
// vector and gives nothing
TEST(ImVector_Tests, Update) {
    for (int n : {0, 1, 32, 33, 1056, 1057, 33000}) {
        auto v = counted(n);
        for (int i : {0, 31, 32, n / 2, n - 1}) {
            if (i < 0 || i >= n) {
                continue;
            }
            int calls = 0;
            auto u = v.update(size_t(i), [&](int x) { ++calls; return x * 10 + 1; });
            EXPECT_EQ(calls, 1);
            EXPECT_EQ(u.size(), size_t(n));
            EXPECT_EQ(u[size_t(i)], i * 10 + 1) << n << " " << i;
            EXPECT_EQ(u.set(size_t(i), i), v);
            // f reads the vector it updates
            auto r = v.update(size_t(i), [&](int x) { return x + v.front() + int(v.size()); });
            EXPECT_EQ(r[size_t(i)], i + n);
        }
        int calls = 0;
        for (size_t i : {size_t(n), size_t(n) + 1, SIZE_MAX}) {
            EXPECT_THROW((void)v.update(i, [&](int x) { ++calls; return x; }), std::out_of_range);
        }
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(v, counted(n));
    }
    auto v = counted(40);
    EXPECT_THROW((void)v.update(35, [](int) -> int { throw std::runtime_error("f"); }), std::runtime_error);
    EXPECT_THROW((void)v.update(3, [](int) -> int { throw std::runtime_error("f"); }), std::runtime_error);
    EXPECT_EQ(v, counted(40));
    sgcl::immutable::vector<std::string> words{"a", "b"};
    EXPECT_EQ(words.update(1, [](const std::string& w) { return w + w; })[1], "bb");
    auto moved = std::move(v);
    EXPECT_EQ(v.update(0, [](int x) { return x - 1; })[0], -1);
}

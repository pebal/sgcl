//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include "sgcl/core/map.h"
#include "sgcl/core/multimap.h"
#include "sgcl/core/multiset.h"
#include "sgcl/core/set.h"

#include <algorithm>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    struct IntHash {
        size_t operator()(const Int& v) const noexcept {
            return std::hash<int>{}(v);
        }
    };

    struct IntEqual {
        bool operator()(const Int& a, const Int& b) const noexcept {
            return (int)a == (int)b;
        }
    };

    using IntSet = sgcl::set<Int, IntHash, IntEqual>;
    using IntMultiset = sgcl::multiset<Int, IntHash, IntEqual>;

    struct StringViewHash {
        using is_transparent = void;

        size_t operator()(std::string_view s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct StringViewEqual {
        using is_transparent = void;

        bool operator()(std::string_view a, std::string_view b) const noexcept {
            return a == b;
        }
    };

    struct Throwing {
        int value;

        explicit Throwing(int v)
        : value(v) {
            if (v < 0) {
                throw std::runtime_error("Throwing");
            }
        }

        bool operator==(const Throwing& other) const noexcept {
            return value == other.value;
        }
    };

    struct ThrowingHash {
        size_t operator()(const Throwing& t) const noexcept {
            return std::hash<int>{}(t.value);
        }
    };

    struct SeededHash {
        size_t seed = 0;

        size_t operator()(int v) const noexcept {
            return std::hash<int>{}(v) ^ seed;
        }
    };

    // Equal elements land in the same bucket whatever the value: chains of
    // equal keys are long, so the multi containers' adjacency is exercised.
    struct CollidingHash {
        size_t operator()(int v) const noexcept {
            return (size_t)(v % 4);
        }
    };

    template<class Set>
    concept FindsStringView = requires(Set& s, std::string_view sv) {
        s.find(sv);
    };

    struct Holder {
        sgcl::set<tracked_ptr<Baz>> set;
    };

    template<class Container>
    size_t sum_of_bucket_sizes(const Container& c) {
        size_t sum = 0;
        for (size_t n = 0; n < c.bucket_count(); ++n) {
            sum += c.bucket_size(n);
        }
        return sum;
    }

    int key_of(int v) {
        return v;
    }

    int key_of(const Int& v) {
        return v;
    }

    template<class T>
    int key_of(const std::pair<const int, T>& p) {
        return p.first;
    }

    template<class T>
    int key_of(const std::pair<const Int, T>& p) {
        return p.first;
    }

    // Every key's elements form one contiguous run in the iteration order.
    template<class Container>
    bool equal_keys_adjacent(const Container& c) {
        std::vector<int> seen;
        for (auto it = c.begin(); it != c.end();) {
            auto key = key_of(*it);
            if (std::find(seen.begin(), seen.end(), key) != seen.end()) {
                return false;
            }
            seen.push_back(key);
            while (it != c.end() && key_of(*it) == key) {
                ++it;
            }
        }
        return true;
    }
}

// set

TEST(Set_Test, DefaultConstructorEmpty) {
    IntSet set;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0u);
    EXPECT_EQ(set.bucket_count(), 0u);
    EXPECT_EQ(set.begin(), set.end());
    EXPECT_EQ(set.find(1), set.end());
    EXPECT_EQ(set.count(1), 0u);
    EXPECT_FALSE(set.contains(1));
    EXPECT_EQ(set.erase(1), 0u);
    EXPECT_TRUE(set.extract(1).empty());
    EXPECT_EQ(set.load_factor(), 0.0f);
    EXPECT_EQ(set.max_load_factor(), 1.0f);
}

TEST(Set_Test, Constructors) {
    sgcl::set<int> sized(20);
    EXPECT_EQ(sized.bucket_count(), 32u);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    std::vector<int> source = {1, 2, 3, 2, 1};
    sgcl::set<int> ranged(source.begin(), source.end());
    EXPECT_EQ(ranged.size(), 3u);
    sgcl::set<int> listed = {5, 6, 5, 7};
    EXPECT_EQ(listed.size(), 3u);
    EXPECT_TRUE(listed.contains(5));
    sgcl::set<int, SeededHash> seeded({1, 2}, 16, SeededHash{3});
    EXPECT_EQ(seeded.bucket_count(), 16u);
    EXPECT_EQ(seeded.hash_function().seed, 3u);
    EXPECT_EQ(seeded.size(), 2u);
    sgcl::set deduced(source.begin(), source.end());
    static_assert(std::is_same_v<decltype(deduced), sgcl::set<int>>);
    sgcl::set deduced_list = {std::string("a"), std::string("b")};
    static_assert(std::is_same_v<decltype(deduced_list), sgcl::set<std::string>>);
    EXPECT_EQ(deduced_list.size(), 2u);
}

TEST(Set_Test, CopyMoveAssign) {
    IntSet other = {1, 2, 3};
    IntSet copy(other);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(collector::get_live_object_count(), 10u);
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_TRUE(copy.contains(1) && copy.contains(2) && copy.contains(3));
    copy.erase(1);
    EXPECT_EQ(other.size(), 3u);
    EXPECT_EQ(Int::counter, 5u);

    auto it = other.find(2);
    IntSet moved(std::move(other));
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.bucket_count(), 0u);
    EXPECT_EQ(moved.size(), 3u);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(Int::counter, 5u);

    copy = moved;
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(Int::counter, 6u);
    other = std::move(copy);
    EXPECT_EQ(other.size(), 3u);
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(Int::counter, 6u);
    other = {7, 8};
    EXPECT_EQ(other.size(), 2u);
    EXPECT_TRUE(other.contains(7));
    EXPECT_EQ(Int::counter, 5u);
    other = other;
    EXPECT_EQ(other.size(), 2u);
}

TEST(Set_Test, InsertAndEmplace) {
    sgcl::set<std::string> set;
    std::string a = "a";
    auto [it, inserted] = set.insert(a);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, "a");
    auto [dup, dup_inserted] = set.insert(a);
    EXPECT_FALSE(dup_inserted);
    EXPECT_EQ(dup, it);
    auto [rit, rinserted] = set.insert(std::string("b"));
    EXPECT_TRUE(rinserted);
    auto [pit, pinserted] = set.insert("c");   // insert(P&&)
    EXPECT_TRUE(pinserted);
    EXPECT_EQ(*pit, "c");
    auto hinted = set.insert(set.cbegin(), "d");
    EXPECT_EQ(*hinted, "d");
    auto hinted_dup = set.insert(set.cbegin(), a);
    EXPECT_EQ(hinted_dup, it);
    std::vector<std::string> more = {"e", "a", "f"};
    set.insert(more.begin(), more.end());
    set.insert({"g", "b"});
    EXPECT_EQ(set.size(), 7u);
    auto [eit, einserted] = set.emplace(3, 'z');
    EXPECT_TRUE(einserted);
    EXPECT_EQ(*eit, "zzz");
    auto [edup, edup_inserted] = set.emplace("zzz");
    EXPECT_FALSE(edup_inserted);
    EXPECT_EQ(edup, eit);
    auto ehint = set.emplace_hint(set.cend(), "y");
    EXPECT_EQ(*ehint, "y");
    EXPECT_EQ(set.size(), 9u);
    EXPECT_EQ(collector::get_live_object_count(), 11u);
    for (const auto& s : {"a", "b", "c", "d", "e", "f", "g", "zzz", "y"}) {
        EXPECT_TRUE(set.contains(s));
    }
}

TEST(Set_Test, Erase) {
    IntSet set;
    for (int i = 0; i < 30; ++i) {
        set.insert(i);
    }
    EXPECT_EQ(set.erase(5), 1u);
    EXPECT_EQ(set.erase(5), 0u);
    EXPECT_EQ(Int::counter, 29u);
    auto it = set.find(6);
    auto next = std::next(it);
    EXPECT_EQ(set.erase(it), next);
    EXPECT_EQ(Int::counter, 28u);
    IntSet::const_iterator cit = set.find(7);
    set.erase(cit);
    EXPECT_EQ(Int::counter, 27u);
    auto first = std::next(set.begin(), 3);
    auto last = std::next(first, 10);
    EXPECT_EQ(set.erase(first, last), last);
    EXPECT_EQ(set.size(), 17u);
    EXPECT_EQ(Int::counter, 17u);
    EXPECT_EQ(sum_of_bucket_sizes(set), 17u);
    size_t visited = 0;
    for (auto it = set.begin(); it != set.end(); ++visited) {
        if (*it % 2 == 0) {
            it = set.erase(it);
        } else {
            ++it;
        }
    }
    EXPECT_EQ(visited, 17u);
    for (const auto& v : set) {
        EXPECT_EQ(v % 2, 1);
    }
    auto big = (size_t)std::ranges::count_if(set, [](const Int& v) { return v > 20; });
    EXPECT_EQ(erase_if(set, [](const Int& v) { return v > 20; }), big);
    EXPECT_EQ(Int::counter, set.size());
    EXPECT_EQ(set.erase(set.begin(), set.end()), set.end());
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(set.erase(set.end()), set.end());
}

TEST(Set_Test, ClearAndDestructor) {
    off_frame([] {   // the iterator insert returns is a raw pointer: it must not stay in this frame
        sgcl::set<Int, SeededHash, IntEqual> set({1, 2, 3}, 0, SeededHash{8});
        set.max_load_factor(0.5f);
        auto buckets = set.bucket_count();
        set.clear();
        EXPECT_TRUE(set.empty());
        EXPECT_EQ(Int::counter, 0u);
        EXPECT_EQ(set.bucket_count(), buckets);
        EXPECT_EQ(set.hash_function().seed, 8u);
        EXPECT_EQ(set.max_load_factor(), 0.5f);
        EXPECT_EQ(collector::get_live_object_count(), 2u);
        set.insert(4);
        EXPECT_EQ(set.size(), 1u);
        EXPECT_EQ(Int::counter, 1u);
    });
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(Set_Test, IteratorsAndSwap) {
    using Set = sgcl::set<int>;
    static_assert(std::forward_iterator<Set::iterator>);
    static_assert(std::forward_iterator<Set::const_iterator>);
    static_assert(std::forward_iterator<Set::local_iterator>);
    static_assert(std::ranges::forward_range<Set>);
    static_assert(std::is_same_v<Set::iterator, Set::const_iterator>);   // a key is never modified in place, as in std
    static_assert(std::is_same_v<Set::iterator::reference, const int&>);
    static_assert(std::is_same_v<Set::local_iterator, Set::const_local_iterator>);
    static_assert(std::is_trivially_copyable_v<Set::iterator>);
    static_assert(std::is_trivially_copyable_v<Set::const_iterator>);
    static_assert(std::is_trivially_copyable_v<Set::local_iterator>);
    static_assert(std::is_trivially_copyable_v<Set::const_local_iterator>);
    static_assert(sizeof(Set::iterator) == sizeof(void*));

    Set a = {1, 2, 3, 4, 5};
    Set b = {10, 20};
    EXPECT_EQ(std::ranges::count_if(a, [](int v) { return v > 2; }), 3);
    EXPECT_EQ(std::ranges::distance(a), 5);
    auto it3 = a.find(3);
    Set::const_iterator cit3 = it3;
    EXPECT_TRUE(it3 == cit3);
    a.erase(1);
    a.erase(5);
    EXPECT_EQ(*it3, 3);
    a.swap(b);
    EXPECT_EQ(a.size(), 2u);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(*it3, 3);
    size_t reached = 0;
    for (auto it = it3; it != b.end(); ++it) {
        ++reached;
        EXPECT_TRUE(b.contains(*it));
    }
    EXPECT_GE(reached, 1u);
    swap(a, b);
    EXPECT_TRUE(a.contains(3));
    Set c(std::move(a));
    EXPECT_EQ(*it3, 3);
    EXPECT_EQ(c.find(3), it3);
    EXPECT_TRUE(a.empty());
    Set d;
    d = std::move(c);
    EXPECT_EQ(d.find(3), it3);
    b.rehash(256);
    d.rehash(256);
    EXPECT_EQ(*it3, 3);
    EXPECT_EQ(d.find(3), it3);
}

// Iterators are raw pointers: a std::vector of them is legal, and each one
// stays valid across a collection while its element is in the set.
TEST(Set_Test, IteratorsInUnmanagedMemory) {
    using Set = sgcl::set<tracked_ptr<Baz>>;
    Set set;
    std::vector<Set::iterator> its;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            its.push_back(set.insert(make_tracked<Baz>(i)).first);
        }
    });
    collector::force_collect(true);
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ((*its[i])->value, i);
        ASSERT_EQ(set.find(*its[i]), its[i]);
    }
    off_frame([&] {
        set.erase(*its[3]);
    });
    collector::force_collect(true);
    EXPECT_EQ(set.size(), 99u);
    EXPECT_EQ(collector::get_live_object_count(), 2 * 99 + 2);   // the nodes, the Baz, the buckets, the sentinel
}

TEST(Set_Test, NodeHandles) {
    IntSet set = {1, 2, 3};
    auto node = set.extract(2);
    ASSERT_TRUE(node);
    EXPECT_EQ(node.value(), 2);
    EXPECT_EQ(set.size(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    node.value() = 20;
    IntSet empty;
    auto result = empty.insert(std::move(node));
    EXPECT_TRUE(result.inserted);
    EXPECT_TRUE(result.node.empty());
    EXPECT_EQ(*result.position, 20);
    EXPECT_TRUE(node.empty());
    auto again = empty.insert(std::move(node));
    EXPECT_FALSE(again.inserted);
    EXPECT_EQ(again.position, empty.end());
    EXPECT_TRUE(again.node.empty());
    EXPECT_EQ(empty.size(), 1u);

    auto three = set.extract(set.find(3));
    empty.insert(3);
    auto blocked = empty.insert(std::move(three));
    EXPECT_FALSE(blocked.inserted);
    EXPECT_EQ(*blocked.position, 3);
    ASSERT_FALSE(blocked.node.empty());
    EXPECT_EQ(Int::counter, 4u);
    auto hinted = set.insert(set.cend(), std::move(blocked.node));
    EXPECT_EQ(*hinted, 3);
    EXPECT_EQ(set.size(), 2u);
    auto dropped = set.extract(1);
    EXPECT_EQ(Int::counter, 4u);
    dropped = {};
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(set.extract(100).empty());
}

TEST(Set_Test, Merge) {
    IntSet a = {1, 2, 3};
    IntSet b = {3, 4, 5};
    auto it4 = b.find(4);
    a.merge(b);
    EXPECT_EQ(a.size(), 5u);
    EXPECT_EQ(b.size(), 1u);
    EXPECT_TRUE(b.contains(3));
    EXPECT_EQ(a.find(4), it4);
    EXPECT_EQ(Int::counter, 6u);
    IntMultiset multi = {5, 6, 6, 7};
    a.merge(multi);
    EXPECT_EQ(a.size(), 7u);
    EXPECT_EQ(multi.size(), 2u);
    EXPECT_EQ(multi.count(5), 1u);
    EXPECT_EQ(multi.count(6), 1u);
    EXPECT_EQ(Int::counter, 10u);
    IntMultiset sink;
    sink.merge(a);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(sink.size(), 7u);
    sink.merge(std::move(multi));
    EXPECT_EQ(sink.size(), 9u);
    EXPECT_EQ(sink.count(6), 2u);
    EXPECT_EQ(sink.count(5), 2u);
    EXPECT_TRUE(multi.empty());
    EXPECT_EQ(Int::counter, 10u);
}

TEST(Set_Test, RehashReserveAndBuckets) {
    sgcl::set<int> set;
    for (int i = 0; i < 300; ++i) {
        set.insert(i * 3);
    }
    EXPECT_EQ(set.bucket_count(), 512u);
    set.rehash(100);
    EXPECT_EQ(set.bucket_count(), 512u);   // never below what the elements need
    set.rehash(3000);
    EXPECT_EQ(set.bucket_count(), 4096u);
    EXPECT_EQ(sum_of_bucket_sizes(set), 300u);
    for (int i = 0; i < 300; ++i) {
        EXPECT_TRUE(set.contains(i * 3));
    }
    for (size_t n = 0; n < set.bucket_count(); ++n) {
        for (auto it = set.cbegin(n); it != set.cend(n); ++it) {
            EXPECT_EQ(set.bucket(*it), n);
        }
    }
    set.max_load_factor(2.0f);
    set.reserve(10);
    EXPECT_EQ(set.bucket_count(), 256u);
    EXPECT_EQ(std::ranges::distance(set), 300);
    set.max_load_factor(0.5f);
    set.insert(-1);
    EXPECT_EQ(set.bucket_count(), 1024u);
    EXPECT_LE(set.load_factor(), 0.5f);
    EXPECT_EQ(set.size(), 301u);
}

TEST(Set_Test, TransparentLookup) {
    using Transparent = sgcl::set<std::string, StringViewHash, StringViewEqual>;
    static_assert(FindsStringView<Transparent>);
    static_assert(!FindsStringView<sgcl::set<std::string>>);
    Transparent set = {"alpha", "beta", "gamma"};
    std::string_view beta = "beta";
    EXPECT_NE(set.find(beta), set.end());
    EXPECT_EQ(set.count("gamma"), 1u);
    EXPECT_TRUE(set.contains(beta));
    EXPECT_FALSE(set.contains("delta"));
    auto [first, last] = set.equal_range(beta);
    EXPECT_EQ(std::distance(first, last), 1);
    EXPECT_EQ(*first, "beta");
    auto node = set.extract(std::string_view("gamma"));
    EXPECT_EQ(node.value(), "gamma");
    EXPECT_EQ(set.erase(beta), 1u);
    EXPECT_EQ(set.erase("nothing"), 0u);
    EXPECT_EQ(set.size(), 1u);
}

TEST(Set_Test, TrackedElements) {
    tracked_ptr<Holder> holder = make_tracked<Holder>();
    tracked_ptr<Baz> kept = make_tracked<Baz>(1);
    off_frame([&] {
        holder->set.insert(kept);
        holder->set.insert(make_tracked<Baz>(2));
        holder->set.insert(make_tracked<Baz>(3));
    });
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 9u);   // the holder, 3 nodes, 3 Baz, the buckets, the sentinel
    off_frame([&] {
        EXPECT_EQ(holder->set.size(), 3u);
        EXPECT_TRUE(holder->set.contains(kept));
        int sum = 0;
        for (const auto& p : holder->set) {
            sum += p->value;
        }
        EXPECT_EQ(sum, 6);
        holder->set.erase(kept);
    });
    EXPECT_EQ(collector::get_live_object_count(), 8u);   // the node is gone, its Baz is still `kept`
    holder = nullptr;
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // `kept`
}

TEST(Set_Test, ThrowingConstructorLeavesSetUnchanged) {
    sgcl::set<Throwing, ThrowingHash> set;
    set.emplace(1);
    auto buckets = set.bucket_count();
    EXPECT_THROW(set.emplace(-1), std::runtime_error);
    EXPECT_EQ(set.size(), 1u);
    EXPECT_EQ(set.bucket_count(), buckets);
    EXPECT_EQ(std::ranges::distance(set), 1);
    set.emplace(2);
    EXPECT_EQ(set.size(), 2u);
}

TEST(Set_Test, Equality) {
    sgcl::set<int> a, b(1024);
    for (int i = 0; i < 100; ++i) {
        a.insert(i);
        b.insert(99 - i);
    }
    EXPECT_TRUE(a == b);
    b.erase(0);
    EXPECT_FALSE(a == b);
    b.insert(100);
    EXPECT_TRUE(a != b);
}

TEST(Set_Test, StressAgainstStd) {
    sgcl::set<int> set;
    std::unordered_set<int> oracle;
    std::mt19937 rng(777);
    std::uniform_int_distribution<int> key(0, 10000);
    std::uniform_int_distribution<int> op(0, 99);
    for (int i = 0; i < 100000; ++i) {
        auto k = key(rng);
        auto o = op(rng);
        if (o < 50) {
            ASSERT_EQ(set.insert(k).second, oracle.insert(k).second);
        } else if (o < 80) {
            ASSERT_EQ(set.erase(k), oracle.erase(k));
        } else {
            ASSERT_EQ(set.contains(k), oracle.contains(k));
        }
        ASSERT_EQ(set.size(), oracle.size());
        if (i % 10000 == 9999) {
            collector::force_collect();
        }
    }
    for (int k : oracle) {
        ASSERT_TRUE(set.contains(k));
    }
    EXPECT_EQ(std::ranges::distance(set), (ptrdiff_t)oracle.size());
}

// multimap

TEST(Multimap_Test, DuplicateKeys) {
    sgcl::multimap<int, Int> map;
    auto it1 = map.insert({1, 10});
    auto it2 = map.insert({1, 11});
    auto it3 = map.emplace(1, 12);
    map.insert({2, 20});
    EXPECT_EQ(map.size(), 4u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_NE(it1, it2);
    EXPECT_NE(it2, it3);
    EXPECT_EQ(map.count(1), 3u);
    EXPECT_EQ(map.count(2), 1u);
    EXPECT_EQ(map.count(3), 0u);
    EXPECT_TRUE(map.contains(1));
    auto [first, last] = map.equal_range(1);
    EXPECT_EQ(std::distance(first, last), 3);
    std::vector<int> values;
    for (auto it = first; it != last; ++it) {
        EXPECT_EQ(it->first, 1);
        values.push_back(it->second);
    }
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::vector<int>{10, 11, 12}));
    EXPECT_EQ(map.find(1)->first, 1);
    EXPECT_EQ(map.find(3), map.end());
    EXPECT_TRUE(equal_keys_adjacent(map));
    auto [none, none_end] = map.equal_range(3);
    EXPECT_EQ(none, none_end);
    auto hinted = map.insert(map.cbegin(), {2, 21});
    EXPECT_EQ(hinted->second, 21);
    auto ehinted = map.emplace_hint(map.cbegin(), 2, 22);
    EXPECT_EQ(ehinted->second, 22);
    map.insert({{3, 30}, {3, 31}});
    std::vector<std::pair<const int, Int>> more = {{4, 40}, {4, 41}};
    map.insert(more.begin(), more.end());
    EXPECT_EQ(map.size(), 10u);
    EXPECT_EQ(map.count(2), 3u);
    EXPECT_EQ(map.count(3), 2u);
    EXPECT_EQ(map.count(4), 2u);
    EXPECT_TRUE(equal_keys_adjacent(map));
    EXPECT_EQ(Int::counter, 12u);   // 10 in the map, 2 in `more`
}

TEST(Multimap_Test, EraseAndExtract) {
    sgcl::multimap<int, Int, CollidingHash> map;
    for (int i = 0; i < 40; ++i) {
        map.emplace(i % 8, i);
    }
    EXPECT_EQ(map.size(), 40u);
    EXPECT_EQ(map.count(3), 5u);
    EXPECT_TRUE(equal_keys_adjacent(map));
    EXPECT_EQ(map.erase(3), 5u);
    EXPECT_EQ(map.erase(3), 0u);
    EXPECT_EQ(map.count(3), 0u);
    EXPECT_EQ(Int::counter, 35u);
    auto it = map.find(4);
    auto next = std::next(it);
    EXPECT_EQ(map.erase(it), next);
    EXPECT_EQ(map.count(4), 4u);
    EXPECT_TRUE(equal_keys_adjacent(map));
    auto [first, last] = map.equal_range(5);
    EXPECT_EQ(map.erase(first, last), last);
    EXPECT_EQ(map.count(5), 0u);
    EXPECT_EQ(map.size(), 29u);
    EXPECT_EQ(Int::counter, 29u);
    EXPECT_EQ(sum_of_bucket_sizes(map), 29u);
    auto node = map.extract(6);
    EXPECT_EQ(node.key(), 6);
    EXPECT_EQ(map.count(6), 4u);
    auto pos = map.insert(std::move(node));
    EXPECT_EQ(pos->first, 6);
    EXPECT_EQ(map.count(6), 5u);
    EXPECT_TRUE(node.empty());
    EXPECT_EQ(map.insert(std::move(node)), map.end());
    auto handle = map.extract(map.find(7));
    auto hinted = map.insert(map.cbegin(), std::move(handle));
    EXPECT_EQ(hinted->first, 7);
    EXPECT_EQ(map.count(7), 5u);
    EXPECT_TRUE(equal_keys_adjacent(map));
    EXPECT_EQ(erase_if(map, [](const auto& p) { return p.second % 2 == 0; }), 19u);   // keys 0, 2, 4, 6
    EXPECT_EQ(map.size(), 10u);
    EXPECT_EQ(Int::counter, 10u);
    map.clear();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(map.empty());
}

TEST(Multimap_Test, RehashKeepsRunsInOrder) {
    sgcl::multimap<int, int, CollidingHash> map;
    for (int i = 0; i < 200; ++i) {
        map.emplace(i % 16, i);
    }
    std::vector<int> before;
    for (const auto& [k, v] : map) {
        before.push_back(v);
    }
    map.rehash(4096);
    EXPECT_EQ(map.bucket_count(), 4096u);
    std::vector<int> after;
    for (const auto& [k, v] : map) {
        after.push_back(v);
    }
    EXPECT_TRUE(equal_keys_adjacent(map));
    for (int key = 0; key < 16; ++key) {
        std::vector<int> a, b;
        for (int v : before) {
            if (v % 16 == key) {
                a.push_back(v);
            }
        }
        for (int v : after) {
            if (v % 16 == key) {
                b.push_back(v);
            }
        }
        EXPECT_EQ(a, b);   // the relative order of equivalent elements survives
        EXPECT_EQ(map.count(key), 200u / 16 + (key < 8 ? 1u : 0u));
    }
    map.rehash(1);
    EXPECT_EQ(map.bucket_count(), 256u);
    EXPECT_TRUE(equal_keys_adjacent(map));
    EXPECT_EQ(sum_of_bucket_sizes(map), 200u);
    sgcl::multimap<int, int, CollidingHash> copy = map;
    std::vector<int> copied;
    for (const auto& [k, v] : copy) {
        copied.push_back(v);
    }
    std::vector<int> original;
    for (const auto& [k, v] : map) {
        original.push_back(v);
    }
    EXPECT_EQ(copied, original);
}

TEST(Multimap_Test, EqualityAsMultisets) {
    sgcl::multimap<int, int> a = {{1, 1}, {1, 2}, {2, 5}};
    sgcl::multimap<int, int> b = {{2, 5}, {1, 2}, {1, 1}};
    sgcl::multimap<int, int> c = {{1, 1}, {1, 1}, {2, 5}};
    sgcl::multimap<int, int> d = {{1, 1}, {1, 2}, {2, 5}, {2, 5}};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);   // same keys and size, different multiset of values
    EXPECT_FALSE(a == d);
    EXPECT_FALSE(c == d);
    c.erase(c.find(1));
    c.insert({1, 2});
    EXPECT_TRUE(a == c);
    sgcl::multimap<int, int> e = {{1, 1}, {1, 1}, {1, 2}};
    sgcl::multimap<int, int> f = {{1, 1}, {1, 2}, {1, 2}};
    EXPECT_FALSE(e == f);
    EXPECT_TRUE(e == e);
}

TEST(Multimap_Test, MergeAndSwap) {
    sgcl::multimap<int, Int> a = {{1, 1}, {1, 2}};
    sgcl::map<int, Int> b = {{1, 3}, {2, 4}};
    auto it = b.find(2);
    a.merge(b);   // a multimap takes everything
    EXPECT_EQ(a.size(), 4u);
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(a.count(1), 3u);
    EXPECT_EQ(a.find(2), it);
    EXPECT_TRUE(equal_keys_adjacent(a));
    sgcl::multimap<int, Int> c = {{1, 5}, {3, 6}};
    a.merge(std::move(c));
    EXPECT_EQ(a.size(), 6u);
    EXPECT_EQ(a.count(1), 4u);
    EXPECT_TRUE(c.empty());
    EXPECT_EQ(Int::counter, 6u);
    sgcl::multimap<int, Int> d = {{9, 9}};
    swap(a, d);
    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(d.size(), 6u);
    EXPECT_EQ(it->second, 4);
    b.merge(d);   // the unique map takes one of each key
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d.count(1), 3u);
    EXPECT_EQ(Int::counter, 7u);
}

TEST(Multimap_Test, StressAgainstStd) {
    sgcl::multimap<int, int> map;
    std::unordered_multimap<int, int> oracle;
    std::mt19937 rng(4242);
    std::uniform_int_distribution<int> key(0, 300);
    std::uniform_int_distribution<int> op(0, 99);
    for (int i = 0; i < 30000; ++i) {
        auto k = key(rng);
        auto o = op(rng);
        if (o < 60) {
            map.emplace(k, i);
            oracle.emplace(k, i);
        } else if (o < 75) {
            ASSERT_EQ(map.erase(k), oracle.erase(k));
        } else if (o < 85) {
            auto it = map.find(k);
            ASSERT_EQ(it != map.end(), oracle.contains(k));
            if (it != map.end()) {
                // the same element: find() may return any element of the key
                auto [ofirst, olast] = oracle.equal_range(k);
                auto oit = std::find_if(ofirst, olast, [&](const auto& p) { return p.second == it->second; });
                ASSERT_NE(oit, olast);
                map.erase(it);
                oracle.erase(oit);
            }
        } else {
            ASSERT_EQ(map.count(k), oracle.count(k));
        }
        ASSERT_EQ(map.size(), oracle.size());
        if (i % 5000 == 4999) {
            collector::force_collect();
            ASSERT_TRUE(equal_keys_adjacent(map));
            for (int j = 0; j <= 300; ++j) {
                auto [first, last] = map.equal_range(j);
                auto [ofirst, olast] = oracle.equal_range(j);
                ASSERT_EQ(std::distance(first, last), std::distance(ofirst, olast));
                std::vector<int> a, b;
                for (auto it = first; it != last; ++it) {
                    a.push_back(it->second);
                }
                for (auto it = ofirst; it != olast; ++it) {
                    b.push_back(it->second);
                }
                std::sort(a.begin(), a.end());
                std::sort(b.begin(), b.end());
                ASSERT_EQ(a, b);
            }
        }
    }
}

// multiset

TEST(Multiset_Test, DuplicateElements) {
    IntMultiset set;
    auto it1 = set.insert(1);
    auto it2 = set.insert(1);
    auto it3 = set.emplace(1);
    set.insert(2);
    EXPECT_NE(it1, it2);
    EXPECT_NE(it1, it3);
    EXPECT_EQ(set.size(), 4u);
    EXPECT_EQ(set.count(1), 3u);
    EXPECT_EQ(set.count(2), 1u);
    EXPECT_EQ(Int::counter, 4u);
    auto [first, last] = set.equal_range(1);
    EXPECT_EQ(std::distance(first, last), 3);
    for (auto it = first; it != last; ++it) {
        EXPECT_EQ(*it, 1);
    }
    EXPECT_TRUE(equal_keys_adjacent(set));
    set.insert({3, 3, 3});
    std::vector<int> more = {4, 4};
    set.insert(more.begin(), more.end());
    EXPECT_EQ(set.count(3), 3u);
    EXPECT_EQ(set.count(4), 2u);
    EXPECT_EQ(set.erase(1), 3u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(set.count(1), 0u);
    set.erase(set.find(3));
    EXPECT_EQ(set.count(3), 2u);
    auto node = set.extract(4);
    EXPECT_EQ(node.value(), 4);
    EXPECT_EQ(set.count(4), 1u);
    EXPECT_EQ(*set.insert(std::move(node)), 4);
    EXPECT_EQ(set.count(4), 2u);
    EXPECT_EQ(set.insert(set.cbegin(), std::move(node)), set.end());
    EXPECT_EQ(erase_if(set, [](const Int& v) { return v == 4; }), 2u);
    EXPECT_EQ(set.size(), 3u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(equal_keys_adjacent(set));
    IntMultiset copy = set;
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(copy.count(3), 2u);
    EXPECT_EQ(Int::counter, 6u);
    copy.clear();
    EXPECT_EQ(Int::counter, 3u);
}

TEST(Multiset_Test, EqualityAsMultisets) {
    sgcl::multiset<int> a = {1, 1, 2};
    sgcl::multiset<int> b = {2, 1, 1};
    sgcl::multiset<int> c = {1, 2, 2};
    sgcl::multiset<int> d = {1, 1, 2, 2};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a == d);
    EXPECT_TRUE(a != c);
    c.erase(c.find(2));
    c.insert(1);
    EXPECT_TRUE(a == c);
}

TEST(Multiset_Test, RehashMergeAndTransparentLookup) {
    sgcl::multiset<int, CollidingHash> set;
    for (int i = 0; i < 100; ++i) {
        set.insert(i % 10);
    }
    auto it = set.find(7);
    set.rehash(1024);
    EXPECT_EQ(set.bucket_count(), 1024u);
    EXPECT_TRUE(equal_keys_adjacent(set));
    EXPECT_EQ(*it, 7);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(set.count(i), 10u);
    }
    EXPECT_EQ(sum_of_bucket_sizes(set), 100u);
    for (size_t n = 0; n < set.bucket_count(); ++n) {
        for (auto lit = set.begin(n); lit != set.end(n); ++lit) {
            EXPECT_EQ(set.bucket(*lit), n);
        }
    }
    sgcl::multiset<int, CollidingHash> other = {7, 7, 11};
    set.merge(other);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(set.count(7), 12u);
    EXPECT_EQ(set.count(11), 1u);
    EXPECT_TRUE(equal_keys_adjacent(set));
    sgcl::set<int, CollidingHash> unique = {7, 12};
    set.merge(unique);
    EXPECT_EQ(set.count(7), 13u);
    EXPECT_EQ(set.count(12), 1u);
    EXPECT_TRUE(unique.empty());

    using Transparent = sgcl::multiset<std::string, StringViewHash, StringViewEqual>;
    static_assert(FindsStringView<Transparent>);
    static_assert(!FindsStringView<sgcl::multiset<std::string>>);
    Transparent names = {"a", "a", "b"};
    EXPECT_EQ(names.count(std::string_view("a")), 2u);
    EXPECT_TRUE(names.contains("b"));
    auto [first, last] = names.equal_range("a");
    EXPECT_EQ(std::distance(first, last), 2);
    EXPECT_EQ(names.erase(std::string_view("a")), 2u);
    EXPECT_EQ(names.size(), 1u);
}

TEST(Multiset_Test, IteratorsAcrossSwapMoveAndErase) {
    sgcl::multiset<int> a = {1, 1, 2, 3};
    sgcl::multiset<int> b = {9};
    auto it = a.find(2);
    a.erase(1);
    a.erase(3);
    EXPECT_EQ(*it, 2);
    a.swap(b);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(b.find(2), it);
    sgcl::multiset<int> c(std::move(b));
    EXPECT_EQ(c.find(2), it);
    EXPECT_EQ(std::next(it), c.end());
    EXPECT_EQ(std::ranges::count_if(c, [](int v) { return v == 2; }), 1);
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"
#include "tests/containers/boundary.h"

#include "sgcl/core/sorted_map.h"
#include "sgcl/core/sorted_multimap.h"
#include "sgcl/core/sorted_multiset.h"
#include "sgcl/core/sorted_set.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <random>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    static_assert(std::bidirectional_iterator<sgcl::sorted_set<int>::iterator>);
    static_assert(std::bidirectional_iterator<sgcl::sorted_set<int>::const_iterator>);
    static_assert(std::bidirectional_iterator<sgcl::sorted_multiset<int>::iterator>);
    static_assert(std::bidirectional_iterator<sgcl::sorted_multimap<int, int>::iterator>);
    static_assert(std::bidirectional_iterator<sgcl::sorted_multimap<int, int>::const_iterator>);
    static_assert(std::ranges::bidirectional_range<sgcl::sorted_set<std::string>>);
    static_assert(std::ranges::bidirectional_range<sgcl::sorted_multiset<int>>);
    // One raw node pointer: storable anywhere, not only on the stack.
    static_assert(std::is_trivially_copyable_v<sgcl::sorted_set<int>::iterator>);
    static_assert(std::is_trivially_copyable_v<sgcl::sorted_multiset<int>::iterator>);
    static_assert(std::is_trivially_copyable_v<sgcl::sorted_multimap<int, int>::const_iterator>);
    static_assert(sizeof(sgcl::sorted_set<int>::iterator) == sizeof(void*));
    static_assert(std::ranges::bidirectional_range<sgcl::sorted_multimap<int, int>>);
    static_assert(std::is_same_v<sgcl::sorted_set<int>::iterator, sgcl::sorted_set<int>::const_iterator>);
    static_assert(std::is_same_v<sgcl::sorted_multiset<int>::iterator, sgcl::sorted_multiset<int>::const_iterator>);
    static_assert(std::is_same_v<sgcl::sorted_set<int>::node_type, sgcl::sorted_multiset<int>::node_type>);
    static_assert(std::is_same_v<sgcl::sorted_multimap<int, int>::node_type, sgcl::sorted_multimap<int, int, std::greater<int>>::node_type>);
    static_assert(!std::is_copy_constructible_v<sgcl::sorted_set<int>::node_type>);
    static_assert(std::is_move_assignable_v<sgcl::sorted_set<int>::node_type>);

    template<class C, class K>
    concept LooksUpWith = requires(C& c, const K& k) {
        c.find(k);
        c.count(k);
        c.contains(k);
        c.equal_range(k);
        c.lower_bound(k);
        c.upper_bound(k);
    };

    static_assert(LooksUpWith<sgcl::sorted_set<std::string, std::less<>>, std::string_view>);
    static_assert(!LooksUpWith<sgcl::sorted_set<std::string>, std::string_view>);
    static_assert(LooksUpWith<sgcl::sorted_multiset<std::string, std::less<>>, std::string_view>);
    static_assert(!LooksUpWith<sgcl::sorted_multiset<std::string>, std::string_view>);
    static_assert(LooksUpWith<sgcl::sorted_multimap<std::string, int, std::less<>>, std::string_view>);
    static_assert(!LooksUpWith<sgcl::sorted_multimap<std::string, int>, std::string_view>);

    template<class C>
    std::vector<typename C::value_type> elements_of(const C& c) {
        return std::vector<typename C::value_type>(c.begin(), c.end());
    }

    template<class C>
    std::vector<typename C::key_type> keys_of(const C& c) {
        std::vector<typename C::key_type> result;
        for (auto& [k, v] : c) {
            result.push_back(k);
        }
        return result;
    }

    template<class C>
    std::vector<typename C::mapped_type> values_of(const C& c) {
        std::vector<typename C::mapped_type> result;
        for (auto& [k, v] : c) {
            result.push_back(v);
        }
        return result;
    }

    struct OnlyLess {
        int value;

        bool operator<(const OnlyLess& other) const noexcept {
            return value < other.value;
        }

        bool operator==(const OnlyLess& other) const noexcept {
            return value == other.value;
        }
    };

    // A key made from an int, counting the conversions: a range of ints
    // inserted converts each element once.
    struct FromInt {
        int value;
        inline static size_t conversions = 0;

        FromInt(int v)
        : value(v) {
            ++conversions;
        }

        bool operator<(const FromInt& other) const noexcept {
            return value < other.value;
        }
    };

    struct Tracked {
        tracked_ptr<Baz> ptr;
        int key;

        Tracked(int k)
        : ptr(make_tracked<Baz>(k))
        , key(k) {
        }

        bool operator<(const Tracked& other) const noexcept {
            return key < other.key;
        }
    };
}

TEST(SortedSet_Test, DefaultConstructorEmpty) {
    sgcl::sorted_set<int> s;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.begin(), s.end());
    EXPECT_EQ(s.cbegin(), s.cend());
    EXPECT_EQ(s.rbegin(), s.rend());
    EXPECT_EQ(s.crbegin(), s.crend());
    EXPECT_EQ(s.find(1), s.end());
    EXPECT_FALSE(s.contains(1));
    EXPECT_EQ(s.erase(1), 0u);
    EXPECT_TRUE(tree_is_valid(s));
    static_assert(noexcept(sgcl::sorted_set<int>()));
}

TEST(SortedSet_Test, Constructors) {
    std::vector<int> src = {3, 1, 2, 1, 3};
    sgcl::sorted_set<int> range(src.begin(), src.end());
    EXPECT_EQ(elements_of(range), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    sgcl::sorted_set<int> ilist = {5, 4, 4, 6};
    EXPECT_EQ(elements_of(ilist), (std::vector<int>{4, 5, 6}));
    sgcl::sorted_set<int, std::greater<int>> greater(src.begin(), src.end(), std::greater<int>{});
    EXPECT_EQ(elements_of(greater), (std::vector<int>{3, 2, 1}));
    EXPECT_TRUE(greater.key_comp()(2, 1));
    EXPECT_TRUE(greater.value_comp()(2, 1));
    sgcl::sorted_set<int> copy(ilist);
    EXPECT_EQ(copy, ilist);
    auto it = ilist.find(5);
    sgcl::sorted_set<int> moved(std::move(ilist));
    EXPECT_TRUE(ilist.empty());
    EXPECT_EQ(elements_of(moved), (std::vector<int>{4, 5, 6}));
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(std::next(it), moved.find(6));
    EXPECT_TRUE(tree_is_valid(moved));
}

TEST(SortedSet_Test, RangeOfAnotherType) {
    // A range whose elements are not the key type: each is converted
    // once, into its node, and the node's key compared. string_views
    // into a set of strings, which std::less<std::string> cannot compare
    // with a string_view at all.
    std::vector<std::string_view> views = {"b", "a", "c", "a"};
    sgcl::sorted_set<std::string> s(views.begin(), views.end());
    EXPECT_EQ(elements_of(s), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_TRUE(tree_is_valid(s));
    s.insert(views.begin(), views.end());
    EXPECT_EQ(s.size(), 3u);
    sgcl::sorted_multiset<std::string> ms(views.begin(), views.end());
    EXPECT_EQ(elements_of(ms), (std::vector<std::string>{"a", "a", "b", "c"}));
    const char* words[] = {"y", "x", "z"};
    sgcl::sorted_set<std::string> from_literals(std::begin(words), std::end(words));
    EXPECT_EQ(elements_of(from_literals), (std::vector<std::string>{"x", "y", "z"}));
    // one conversion per element, none per comparison
    std::vector<int> ints = {3, 1, 2, 1, 5, 4};
    FromInt::conversions = 0;
    sgcl::sorted_set<FromInt> from_ints(ints.begin(), ints.end());
    EXPECT_EQ(FromInt::conversions, ints.size());
    EXPECT_EQ(from_ints.size(), 5u);
    EXPECT_EQ(from_ints.begin()->value, 1);
    from_ints.insert(ints.begin(), ints.end());
    EXPECT_EQ(FromInt::conversions, 2 * ints.size());
    EXPECT_EQ(from_ints.size(), 5u);
    FromInt::conversions = 0;
    sgcl::sorted_multiset<FromInt> multi(ints.begin(), ints.end());
    EXPECT_EQ(FromInt::conversions, ints.size());
    EXPECT_EQ(multi.size(), ints.size());
    EXPECT_TRUE(tree_is_valid(multi));
}

TEST(SortedSet_Test, Assignment) {
    sgcl::sorted_set<Int> a = {1, 2, 3};
    sgcl::sorted_set<Int> b = {9};
    b = a;
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(a, b);
    b = std::move(a);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(b.size(), 3u);
    b = {7, 8};
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(elements_of(b), (std::vector<Int>{7, 8}));
    b = std::initializer_list<Int>();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
}

TEST(SortedSet_Test, Iteration) {
    sgcl::sorted_set<std::string> s = {"d", "b", "a", "c"};
    std::vector<std::string> forward(s.begin(), s.end());
    EXPECT_EQ(forward, (std::vector<std::string>{"a", "b", "c", "d"}));
    std::vector<std::string> backward(s.rbegin(), s.rend());
    EXPECT_EQ(backward, (std::vector<std::string>{"d", "c", "b", "a"}));
    std::vector<std::string> stepped;
    for (auto it = s.end(); it != s.begin();) {
        it--;
        stepped.push_back(*it);
    }
    EXPECT_EQ(stepped, backward);
    EXPECT_EQ(*--s.end(), "d");
    EXPECT_EQ(std::distance(s.cbegin(), s.cend()), 4);
    EXPECT_TRUE(std::ranges::is_sorted(s));
    EXPECT_EQ(std::ranges::count_if(s, [](const std::string& v) { return v < "c"; }), 2);
}

TEST(SortedSet_Test, Insert) {
    sgcl::sorted_set<std::string> s;
    std::string a = "a";
    auto r = s.insert(a);
    EXPECT_TRUE(r.second);
    EXPECT_EQ(*r.first, "a");
    r = s.insert(a);
    EXPECT_FALSE(r.second);
    r = s.insert(std::string("b"));
    EXPECT_TRUE(r.second);
    auto it = s.insert(s.end(), "c");
    EXPECT_EQ(*it, "c");
    it = s.insert(s.begin(), "c");
    EXPECT_EQ(*it, "c");
    std::vector<std::string> more = {"e", "d", "a"};
    s.insert(more.begin(), more.end());
    s.insert({"f", "b"});
    EXPECT_EQ(elements_of(s), (std::vector<std::string>{"a", "b", "c", "d", "e", "f"}));
    r = s.emplace(3, 'x');
    EXPECT_TRUE(r.second);
    EXPECT_EQ(*r.first, "xxx");
    it = s.emplace_hint(s.end(), "zzz");
    EXPECT_EQ(it, std::prev(s.end()));
    it = s.emplace_hint(s.begin(), "zzz");
    EXPECT_EQ(it, std::prev(s.end()));
    EXPECT_EQ(s.size(), 8u);
    EXPECT_TRUE(tree_is_valid(s));
}

TEST(SortedSet_Test, Erase) {
    sgcl::sorted_set<Int> s;
    off_frame([&] {
        for (int i = 0; i < 10; ++i) {
            s.insert(i);
        }
        EXPECT_EQ(Int::counter, 10u);
        auto it = s.erase(s.find(3));
        EXPECT_EQ(*it, 4);
        EXPECT_EQ(Int::counter, 9u);
        EXPECT_EQ(s.erase(4), 1u);
        EXPECT_EQ(s.erase(4), 0u);
        it = s.erase(s.find(6), s.find(9));
        EXPECT_EQ(*it, 9);
        EXPECT_EQ(elements_of(s), (std::vector<Int>{0, 1, 2, 5, 9}));
        EXPECT_EQ(Int::counter, 5u);
        EXPECT_TRUE(tree_is_valid(s));
        EXPECT_EQ(std::erase_if(s, [](const Int& v) { return v < 2; }), 2u);
        EXPECT_EQ(Int::counter, 3u);
        s.clear();
        EXPECT_EQ(Int::counter, 0u);
        EXPECT_TRUE(s.empty());
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
}

TEST(SortedSet_Test, Swap) {
    sgcl::sorted_set<int> a = {1, 2};
    sgcl::sorted_set<int> b = {3};
    auto it = a.find(1);
    a.swap(b);
    EXPECT_EQ(elements_of(a), (std::vector<int>{3}));
    EXPECT_EQ(elements_of(b), (std::vector<int>{1, 2}));
    EXPECT_EQ(it, b.begin());
    swap(a, b);
    EXPECT_EQ(elements_of(a), (std::vector<int>{1, 2}));
    EXPECT_EQ(it, a.begin());
    EXPECT_TRUE(tree_is_valid(a));
    EXPECT_TRUE(tree_is_valid(b));
}

TEST(SortedSet_Test, NodeHandles) {
    sgcl::sorted_set<Int> s = {1, 2, 3};
    sgcl::sorted_set<Int> empty;
    off_frame([&] {
        auto nh = s.extract(2);
        EXPECT_FALSE(nh.empty());
        EXPECT_EQ(nh.value(), 2);
        EXPECT_EQ(s.size(), 2u);
        EXPECT_EQ(Int::counter, 3u);
        auto r = empty.insert(std::move(nh));
        EXPECT_TRUE(r.inserted);
        EXPECT_TRUE(r.node.empty());
        EXPECT_EQ(r.position, empty.begin());
        EXPECT_TRUE(nh.empty());
        r = empty.insert(std::move(nh));
        EXPECT_FALSE(r.inserted);
        EXPECT_EQ(r.position, empty.end());
        EXPECT_EQ(empty.size(), 1u);
        nh = s.extract(s.begin());
        EXPECT_EQ(nh.value(), 1);
        s.insert(1);
        r = s.insert(std::move(nh));
        EXPECT_FALSE(r.inserted);
        EXPECT_EQ(*r.position, 1);
        EXPECT_EQ(r.node.value(), 1);
        r.node.value() = 5;
        auto it = s.insert(s.end(), std::move(r.node));
        EXPECT_EQ(*it, 5);
        EXPECT_EQ(elements_of(s), (std::vector<Int>{1, 3, 5}));
        EXPECT_EQ(Int::counter, 4u);
        {
            auto dropped = s.extract(5);
            EXPECT_EQ(Int::counter, 4u);
        }
        EXPECT_EQ(Int::counter, 3u);
        EXPECT_TRUE(tree_is_valid(s));
        EXPECT_TRUE(tree_is_valid(empty));
    });
    EXPECT_EQ(collector::get_live_object_count(), 5u);
}

TEST(SortedSet_Test, MergeAndComparison) {
    sgcl::sorted_set<int> a = {1, 3, 5};
    sgcl::sorted_multiset<int> b = {1, 2, 2, 4};
    a.merge(b);
    EXPECT_EQ(elements_of(a), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(elements_of(b), (std::vector<int>{1, 2}));
    sgcl::sorted_set<int, std::greater<int>> g = {6, 5};
    a.merge(g);
    EXPECT_EQ(elements_of(a), (std::vector<int>{1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(elements_of(g), (std::vector<int>{5}));
    EXPECT_TRUE(tree_is_valid(a));
    EXPECT_TRUE(tree_is_valid(b));
    EXPECT_TRUE(tree_is_valid(g));

    sgcl::sorted_set<int> x = {1, 2};
    sgcl::sorted_set<int> y = {1, 3};
    EXPECT_TRUE(x < y);
    EXPECT_TRUE(x != y);
    EXPECT_EQ(x <=> x, std::strong_ordering::equal);
    sgcl::sorted_set<OnlyLess> p = {{1}, {2}};
    sgcl::sorted_set<OnlyLess> q = {{1}, {2}, {3}};
    EXPECT_TRUE(p < q);
    EXPECT_TRUE(p == p);
    EXPECT_EQ(p <=> q, std::weak_ordering::less);
}

TEST(SortedSet_Test, TransparentLookup) {
    sgcl::sorted_set<std::string, std::less<>> s = {"apple", "cherry", "banana"};
    const auto& cs = s;
    std::string_view banana = "banana";
    EXPECT_EQ(*s.find(banana), "banana");
    EXPECT_EQ(cs.find(std::string_view("kiwi")), cs.end());
    EXPECT_EQ(s.count(banana), 1u);
    EXPECT_TRUE(cs.contains("cherry"));
    EXPECT_EQ(*s.lower_bound(std::string_view("b")), "banana");
    EXPECT_EQ(*cs.upper_bound(banana), "cherry");
    auto [first, last] = s.equal_range(banana);
    EXPECT_EQ(*first, "banana");
    EXPECT_EQ(*last, "cherry");
}

TEST(SortedSet_Test, ElementsHoldingTrackedPointers) {
    sgcl::sorted_set<Tracked> s;
    for (int i = 0; i < 20; ++i) {
        s.emplace(i);
    }
    EXPECT_EQ(collector::get_live_object_count(), 41u);
    collector::force_collect(true);
    off_frame([&] {   // references into nodes and erased nodes must not linger in this frame
        int sum = 0;
        for (auto& t : s) {
            sum += t.ptr->value;
        }
        EXPECT_EQ(sum, 190);
        for (int i = 0; i < 20; i += 2) {
            s.erase(Tracked(i));
        }
    });
    EXPECT_EQ(s.size(), 10u);
    EXPECT_EQ(collector::get_live_object_count(), 21u);
    EXPECT_TRUE(tree_is_valid(s));
}

TEST(SortedSet_Test, IteratorsInAVector) {
    sgcl::sorted_set<int> s;
    for (int i = 0; i < 100; ++i) {
        s.insert(i);
    }
    std::vector<sgcl::sorted_set<int>::iterator> its;
    for (auto it = s.begin(); it != s.end(); ++it) {
        its.push_back(it);
    }
    ASSERT_EQ(its.size(), 100u);
    collector::force_collect(true);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(*its[i], i);
    }
    for (int i = 0; i < 100; i += 2) {
        s.erase(its[i]);
    }
    collector::force_collect(true);
    EXPECT_EQ(s.size(), 50u);
    for (int i = 1; i < 100; i += 2) {
        EXPECT_EQ(*its[i], i);
        EXPECT_EQ(std::next(its[i]), i + 2 < 100 ? its[i + 2] : s.end());
    }
    EXPECT_TRUE(tree_is_valid(s));
}

TEST(SortedSet_Test, RawIteratorSurvivesCollection) {
    sgcl::sorted_set<Tracked> s;
    off_frame([&] {
        for (int i = 0; i < 30; ++i) {
            s.emplace(i);
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), 61u);
    // Frames as in SortedMap_Test.RawIteratorSurvivesCollection: a raw word left
    // in a frame retains its node until it is overwritten.
    off_frame([&] {
        auto it = s.find(Tracked(15));
        for (int round = 0; round < 3; ++round) {
            off_frame([&] {
                for (int i = 0; i < 30; ++i) {
                    if (i != 15) {
                        s.erase(Tracked(i));
                    }
                }
            });
            collector::force_collect(true);
            EXPECT_EQ(collector::get_live_object_count(), 3u);
            off_frame([&] {
                EXPECT_EQ(it->key, 15);
                EXPECT_EQ(it->ptr->value, 15);
                EXPECT_EQ(it, s.begin());
                EXPECT_EQ(std::next(it), s.end());
            });
            off_frame([&] {
                for (int i = 0; i < 30; ++i) {
                    s.emplace(i);
                }
            });
            collector::force_collect(true);
            EXPECT_EQ(s.size(), 30u);
            EXPECT_EQ(collector::get_live_object_count(), 61u);
            off_frame([&] {
                EXPECT_EQ(it->key, 15);
                EXPECT_EQ(std::next(it)->key, 16);
                EXPECT_EQ(std::prev(it)->key, 14);
            });
            EXPECT_TRUE(tree_is_valid(s));
        }
        s.erase(it);
    });
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 59u);
    EXPECT_EQ(s.size(), 29u);
    EXPECT_FALSE(s.contains(Tracked(15)));
}

TEST(SortedMultiset_Test, Duplicates) {
    sgcl::sorted_multiset<int> s = {3, 1, 2, 1, 3, 1};
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(elements_of(s), (std::vector<int>{1, 1, 1, 2, 3, 3}));
    EXPECT_EQ(s.count(1), 3u);
    EXPECT_EQ(s.count(2), 1u);
    EXPECT_EQ(s.count(4), 0u);
    EXPECT_TRUE(s.contains(3));
    EXPECT_EQ(s.find(1), s.begin());
    auto [first, last] = s.equal_range(1);
    EXPECT_EQ(std::distance(first, last), 3);
    EXPECT_EQ(first, s.lower_bound(1));
    EXPECT_EQ(last, s.upper_bound(1));
    EXPECT_EQ(*last, 2);
    EXPECT_EQ(s.lower_bound(4), s.end());
    auto it = s.insert(1);
    EXPECT_EQ(std::distance(s.begin(), it), 3);
    EXPECT_EQ(s.erase(1), 4u);
    EXPECT_EQ(elements_of(s), (std::vector<int>{2, 3, 3}));
    it = s.erase(s.find(3));
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_TRUE(tree_is_valid(s));
    it = s.emplace_hint(s.end(), 3);
    EXPECT_EQ(it, std::prev(s.end()));
    EXPECT_EQ(std::erase_if(s, [](int v) { return v == 3; }), 2u);
    EXPECT_EQ(elements_of(s), (std::vector<int>{2}));
}

TEST(SortedMultiset_Test, NodeHandlesAndMerge) {
    sgcl::sorted_multiset<Int> s = {1, 1, 2};
    auto nh = s.extract(1);
    EXPECT_EQ(nh.value(), 1);
    EXPECT_EQ(s.count(1), 1u);
    sgcl::sorted_multiset<Int> empty;
    auto it = empty.insert(std::move(nh));
    EXPECT_EQ(it, empty.begin());
    EXPECT_TRUE(nh.empty());
    EXPECT_EQ(empty.insert(std::move(nh)), empty.end());
    EXPECT_EQ(empty.size(), 1u);
    it = s.insert(s.extract(s.begin()));
    EXPECT_EQ(*it, 1);
    it = s.insert(s.begin(), s.extract(2));
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(elements_of(s), (std::vector<Int>{1, 2}));
    sgcl::sorted_set<Int> u = {2, 3};
    s.merge(u);
    EXPECT_TRUE(u.empty());
    EXPECT_EQ(elements_of(s), (std::vector<Int>{1, 2, 2, 3}));
    s.merge(empty);
    EXPECT_EQ(elements_of(s), (std::vector<Int>{1, 1, 2, 2, 3}));
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_TRUE(tree_is_valid(s));
    s.clear();
    EXPECT_EQ(Int::counter, 0u);
}

TEST(SortedMultiset_Test, StressAgainstStdMultiset) {
    sgcl::sorted_multiset<int> s;
    std::multiset<int> oracle;
    std::mt19937 rng(777);
    std::uniform_int_distribution<int> key(0, 255);
    std::uniform_int_distribution<int> op(0, 9);
    for (int i = 1; i <= 50000; ++i) {
        int k = key(rng);
        int o = op(rng);
        if (o < 5) {
            s.insert(k);
            oracle.insert(k);
        } else if (o < 7) {
            ASSERT_EQ(s.erase(k), oracle.erase(k));
        } else if (o < 8) {
            auto it = s.find(k);
            auto oit = oracle.find(k);
            ASSERT_EQ(it == s.end(), oit == oracle.end());
            if (oit != oracle.end()) {
                s.erase(it);
                oracle.erase(oit);
            }
        } else {
            ASSERT_EQ(s.count(k), oracle.count(k));
            auto [a, b] = s.equal_range(k);
            auto [oa, ob] = oracle.equal_range(k);
            ASSERT_EQ(std::distance(a, b), std::distance(oa, ob));
        }
        ASSERT_EQ(s.size(), oracle.size());
        if (i % 5000 == 0) {
            collector::force_collect();
            ASSERT_TRUE(tree_is_valid(s));
            ASSERT_TRUE(std::equal(s.begin(), s.end(), oracle.begin(), oracle.end()));
        }
    }
    EXPECT_TRUE(std::equal(s.rbegin(), s.rend(), oracle.rbegin(), oracle.rend()));
}

TEST(SortedMultimap_Test, DuplicatesAndBounds) {
    sgcl::sorted_multimap<std::string, int> m = {{"b", 1}, {"a", 1}, {"b", 2}, {"c", 1}, {"b", 3}};
    EXPECT_EQ(m.size(), 5u);
    EXPECT_EQ(keys_of(m), (std::vector<std::string>{"a", "b", "b", "b", "c"}));
    // equivalent keys keep insertion order
    EXPECT_EQ(values_of(m), (std::vector<int>{1, 1, 2, 3, 1}));
    EXPECT_EQ(m.count("b"), 3u);
    auto [first, last] = m.equal_range("b");
    EXPECT_EQ(first, m.lower_bound("b"));
    EXPECT_EQ(last, m.upper_bound("b"));
    EXPECT_EQ(std::distance(first, last), 3);
    EXPECT_EQ(first->second, 1);
    EXPECT_EQ(last->first, "c");
    EXPECT_EQ(m.find("b")->second, 1);
    auto it = m.insert({"b", 4});
    EXPECT_EQ(std::prev(it)->second, 3);
    it = m.emplace("b", 5);
    EXPECT_EQ(std::prev(it)->second, 4);
    it = m.emplace_hint(m.lower_bound("b"), "b", 0);
    EXPECT_EQ(it, m.lower_bound("b"));
    EXPECT_EQ(std::next(it)->second, 1);
    it = m.insert(m.end(), {"b", 6});
    EXPECT_EQ(std::next(it)->first, "c");
    it = m.insert(std::pair<const char*, int>("d", 1));
    EXPECT_EQ(it, std::prev(m.end()));
    EXPECT_EQ(values_of(m), (std::vector<int>{1, 0, 1, 2, 3, 4, 5, 6, 1, 1}));
    EXPECT_TRUE(tree_is_valid(m));
    const auto& cm = m;
    auto [cfirst, clast] = cm.equal_range("z");
    EXPECT_EQ(cfirst, cm.end());
    EXPECT_EQ(clast, cm.end());
    EXPECT_EQ(m.erase("b"), 7u);
    EXPECT_EQ(keys_of(m), (std::vector<std::string>{"a", "c", "d"}));
    EXPECT_EQ(m.erase("b"), 0u);
    EXPECT_TRUE(tree_is_valid(m));
    it = m.erase(m.begin(), m.find("d"));
    EXPECT_EQ(it->first, "d");
    EXPECT_EQ(m.size(), 1u);
}

TEST(SortedMultimap_Test, LifetimeAndHandles) {
    sgcl::sorted_multimap<int, Int> m;
    for (int i = 0; i < 6; ++i) {
        m.emplace(i / 2, i);
    }
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    auto nh = m.extract(1);
    EXPECT_EQ(nh.key(), 1);
    EXPECT_EQ(nh.mapped(), 2);
    EXPECT_EQ(m.count(1), 1u);
    nh.key() = 9;
    auto it = m.insert(std::move(nh));
    EXPECT_EQ(it, std::prev(m.end()));
    EXPECT_TRUE(nh.empty());
    EXPECT_EQ(m.insert(std::move(nh)), m.end());
    sgcl::sorted_multimap<int, Int> other;
    it = other.insert(m.extract(it));
    EXPECT_EQ(it->first, 9);
    EXPECT_EQ(other.size(), 1u);
    EXPECT_EQ(m.size(), 5u);
    EXPECT_EQ(m.erase(2), 2u);
    EXPECT_EQ(Int::counter, 4u);
    sgcl::sorted_map<int, Int> u = {{0, 100}, {7, 7}};
    m.merge(u);
    EXPECT_TRUE(u.empty());
    EXPECT_EQ(keys_of(m), (std::vector<int>{0, 0, 0, 1, 7}));
    EXPECT_EQ(values_of(m), (std::vector<Int>{0, 1, 100, 3, 7}));
    EXPECT_TRUE(tree_is_valid(m));
    m = other;
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(m, other);
    sgcl::sorted_multimap<int, Int> empty;
    m.swap(empty);
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(empty.size(), 1u);
    empty.clear();
    other.clear();
    EXPECT_EQ(Int::counter, 0u);
}

TEST(SortedMultimap_Test, StressAgainstStdMultimap) {
    sgcl::sorted_multimap<int, int> m;
    std::multimap<int, int> oracle;
    std::mt19937 rng(4242);
    std::uniform_int_distribution<int> key(0, 511);
    std::uniform_int_distribution<int> op(0, 9);
    for (int i = 1; i <= 50000; ++i) {
        int k = key(rng);
        int o = op(rng);
        if (o < 4) {
            auto it = m.emplace(k, i);
            auto oit = oracle.emplace(k, i);
            ASSERT_EQ(it->second, oit->second);
        } else if (o < 5) {
            auto it = m.emplace_hint(m.end(), k, i);
            auto oit = oracle.emplace_hint(oracle.end(), k, i);
            ASSERT_EQ(std::next(it) == m.end(), std::next(oit) == oracle.end());
        } else if (o < 7) {
            ASSERT_EQ(m.erase(k), oracle.erase(k));
        } else if (o < 8) {
            auto it = m.find(k);
            auto oit = oracle.find(k);
            ASSERT_EQ(it == m.end(), oit == oracle.end());
            if (oit != oracle.end()) {
                ASSERT_EQ(it->second, oit->second);
                m.erase(it);
                oracle.erase(oit);
            }
        } else {
            auto [a, b] = m.equal_range(k);
            auto [oa, ob] = oracle.equal_range(k);
            ASSERT_TRUE(std::equal(a, b, oa, ob));
        }
        ASSERT_EQ(m.size(), oracle.size());
        if (i % 5000 == 0) {
            collector::force_collect();
            ASSERT_TRUE(tree_is_valid(m));
            ASSERT_TRUE(std::equal(m.begin(), m.end(), oracle.begin(), oracle.end()));
        }
    }
    EXPECT_TRUE(std::equal(m.rbegin(), m.rend(), oracle.rbegin(), oracle.rend()));
}

TEST(SortedSet_Test, DeductionGuides) {
    sgcl::sorted_set s = {3, 1, 2};
    static_assert(std::is_same_v<decltype(s), sgcl::sorted_set<int>>);
    std::vector<int> v = {5, 4, 4};
    sgcl::sorted_set from_range(v.begin(), v.end());
    static_assert(std::is_same_v<decltype(from_range), sgcl::sorted_set<int>>);
    sgcl::sorted_set greater({3, 1}, std::greater<int>());
    static_assert(std::is_same_v<decltype(greater), sgcl::sorted_set<int, std::greater<int>>>);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(from_range.size(), 2u);
    EXPECT_EQ(*greater.begin(), 3);
}

TEST(SortedMultiset_Test, DeductionGuides) {
    sgcl::sorted_multiset s = {3, 1, 1};
    static_assert(std::is_same_v<decltype(s), sgcl::sorted_multiset<int>>);
    std::vector<int> v = {5, 4, 4};
    sgcl::sorted_multiset from_range(v.begin(), v.end());
    static_assert(std::is_same_v<decltype(from_range), sgcl::sorted_multiset<int>>);
    EXPECT_EQ(s.count(1), 2u);
    EXPECT_EQ(from_range.size(), 3u);
}

TEST(SortedMultimap_Test, DeductionGuides) {
    sgcl::sorted_multimap m = {std::pair{1, 2.0}, std::pair{1, 3.0}};
    static_assert(std::is_same_v<decltype(m), sgcl::sorted_multimap<int, double>>);
    sgcl::sorted_multimap from_range(m.begin(), m.end());
    static_assert(std::is_same_v<decltype(from_range), sgcl::sorted_multimap<int, double>>);
    EXPECT_EQ(m.count(1), 2u);
    EXPECT_EQ(from_range.size(), 2u);
}

// The public surface of the trees as std's: the flag the shared tree is
// built on is not a member of theirs; insert_return_type only where a
// node handle's insert returns it
namespace {
    template<class C>
    concept HasMultiFlag = requires { C::Multi; } || requires { C::multi; };

    template<class C>
    concept HasInsertReturnType = requires { typename C::insert_return_type; };
}

TEST(SortedContainers_Test, PublicSurfaceAsStd) {
    static_assert(!HasMultiFlag<sgcl::sorted_map<int, int>>);
    static_assert(!HasMultiFlag<sgcl::sorted_multimap<int, int>>);
    static_assert(!HasMultiFlag<sgcl::sorted_set<int>>);
    static_assert(!HasMultiFlag<sgcl::sorted_multiset<int>>);
    static_assert(HasInsertReturnType<sgcl::sorted_map<int, int>>);
    static_assert(HasInsertReturnType<sgcl::sorted_set<int>>);
    static_assert(!HasInsertReturnType<sgcl::sorted_multimap<int, int>>);
    static_assert(!HasInsertReturnType<sgcl::sorted_multiset<int>>);
}

// A node handle swaps, as std's do: the member and the free swap found
// by argument lookup, each handle then owning the other's element
namespace {
    template<class H>
    concept MemberSwap = requires(H& a, H& b) { { a.swap(b) } noexcept; };
}

TEST(SortedContainers_Test, NodeHandleSwap) {
    using MapHandle = sgcl::sorted_map<int, std::string>::node_type;
    using SetHandle = sgcl::sorted_set<int>::node_type;
    static_assert(MemberSwap<MapHandle>);
    static_assert(MemberSwap<SetHandle>);
    static_assert(std::is_nothrow_swappable_v<MapHandle>);
    static_assert(std::is_nothrow_swappable_v<SetHandle>);

    sgcl::sorted_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    auto a = m.extract(1);
    auto b = m.extract(2);
    a.swap(b);
    EXPECT_EQ(a.key(), 2);
    EXPECT_EQ(a.mapped(), "two");
    EXPECT_EQ(b.key(), 1);
    swap(a, b);
    EXPECT_EQ(a.key(), 1);
    EXPECT_EQ(b.mapped(), "two");
    MapHandle empty;
    swap(a, empty);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(empty.key(), 1);

    sgcl::sorted_set<int> s = {5, 6};
    auto c = s.extract(5);
    auto d = s.extract(6);
    swap(c, d);
    EXPECT_EQ(c.value(), 6);
    EXPECT_EQ(d.value(), 5);
    std::ranges::swap(c, d);
    EXPECT_EQ(c.value(), 5);
    EXPECT_TRUE(s.insert(std::move(c)).inserted);
    EXPECT_TRUE(s.contains(5));
}

// The edges of swap: two empty handles, a moved-from one, a handle with
// itself (the node stays, the element alive), and the element dying with
// the handle it ended in
TEST(SortedContainers_Test, NodeHandleSwapEdges) {
    using Handle = sgcl::sorted_map<int, Int>::node_type;
    Handle a;
    Handle b;
    a.swap(b);
    EXPECT_TRUE(a.empty());
    EXPECT_TRUE(b.empty());

    auto before = Int::counter;
    {
        sgcl::sorted_map<int, Int> m = {{1, 10}, {2, 20}};
        Handle c = m.extract(1);
        Handle d = std::move(c);                         // c moved from: empty
        swap(c, d);
        EXPECT_EQ((int)c.mapped(), 10);
        EXPECT_TRUE(d.empty());
        c.swap(c);                                       // with itself: the same node, the element untouched
        EXPECT_EQ(c.key(), 1);
        EXPECT_EQ((int)c.mapped(), 10);
        swap(c, c);
        EXPECT_EQ((int)c.mapped(), 10);
        EXPECT_EQ(Int::counter, before + 2);             // both elements alive: one in the map, one in c
    }
    EXPECT_EQ(Int::counter, before);                     // each destroyed once, by the map and by the handle

    sgcl::sorted_multiset<int> ms = {3, 3};
    auto e = ms.extract(ms.begin());
    sgcl::sorted_multiset<int>::node_type f;
    e.swap(f);
    EXPECT_TRUE(e.empty());
    EXPECT_EQ(f.value(), 3);
    EXPECT_EQ(ms.insert(std::move(f)), std::next(ms.begin()));   // back in, after the equal one
}

// erase and extract by a key of another type, where the comparator is
// transparent, as the lookups and the hash containers' erase and extract
// (C++23's): none, one or all the equivalent elements; an iterator is
// still an iterator, never a key
namespace {
    struct ThrowingViewLess {
        using is_transparent = void;
        bool operator()(const std::string& a, const std::string& b) const noexcept { return a < b; }
        bool operator()(const std::string& a, std::string_view b) const { return a < b; }
        bool operator()(std::string_view a, const std::string& b) const { return a < b; }
    };

    template<class C, class K>
    concept ErasesBy = requires(C& c, const K& k) { c.erase(k); c.extract(k); };
}

TEST(SortedContainers_Test, EraseAndExtractByAKeyOfAnotherType) {
    static_assert(ErasesBy<sgcl::sorted_map<std::string, int, std::less<>>, std::string_view>);
    static_assert(ErasesBy<sgcl::sorted_multiset<std::string, std::less<>>, const char*>);
    static_assert(!ErasesBy<sgcl::sorted_map<std::string, int>, std::string_view>);   // std::less<std::string>: no
    static_assert(!ErasesBy<sgcl::sorted_set<std::string>, std::string_view>);

    sgcl::sorted_map<std::string, int, std::less<>> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::string_view a = "a";
    static_assert(noexcept(m.erase(a)));
    EXPECT_EQ(m.erase(a), 1u);
    EXPECT_EQ(m.erase(a), 0u);                           // absent
    auto nh = m.extract(std::string_view("b"));
    EXPECT_EQ(nh.key(), "b");
    EXPECT_EQ(nh.mapped(), 2);
    EXPECT_TRUE(m.extract(std::string_view("zz")).empty());
    EXPECT_EQ(m.erase(m.begin()), m.end());              // an iterator, not a key: the last element
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.erase(a), 0u);                           // an empty tree, no header yet or none left
    EXPECT_TRUE(m.extract(a).empty());
    sgcl::sorted_map<std::string, int, std::less<>> never;
    EXPECT_EQ(never.erase(a), 0u);
    EXPECT_TRUE(never.extract(a).empty());

    sgcl::sorted_multimap<std::string, int, std::less<>> mm = {{"x", 1}, {"x", 2}, {"y", 3}};
    EXPECT_EQ(mm.erase(std::string_view("x")), 2u);      // every equivalent element
    EXPECT_EQ(mm.size(), 1u);
    sgcl::sorted_multiset<std::string, std::less<>> ms = {"p", "p", "q"};
    auto one = ms.extract("p");                          // one of the equivalent ones
    EXPECT_EQ(one.value(), "p");
    EXPECT_EQ(ms.count("p"), 1u);

    sgcl::sorted_map<std::string, int, ThrowingViewLess> t = {{"k", 1}};
    std::string_view k = "k";
    static_assert(!noexcept(t.erase(k)));               // as noexcept as the comparator's calls with the view
    static_assert(!noexcept(t.extract(k)));
    std::string key = "k";
    static_assert(noexcept(t.erase(key)));
    EXPECT_EQ(t.erase(k), 1u);
}

// Boundaries (DESIGN 408)

// The keys at the ends of their type: the least and the greatest found,
// their bounds, min and max, erased; a lookup below the least and above
// the greatest; max_size the largest difference_type
TEST(SortedContainers_Test, KeysAtTheEndsOfTheirType) {
    sgcl::sorted_map<int, int> m = {{INT_MIN, 1}, {0, 2}, {INT_MAX, 3}};
    EXPECT_EQ(m.max_size(), size_t(PTRDIFF_MAX));
    EXPECT_EQ(m.begin()->first, INT_MIN);
    EXPECT_EQ(m.rbegin()->first, INT_MAX);
    EXPECT_EQ(m.min().first, INT_MIN);
    EXPECT_EQ(m.max().first, INT_MAX);
    EXPECT_EQ(m.lower_bound(INT_MIN), m.begin());
    EXPECT_EQ(m.upper_bound(INT_MAX), m.end());
    EXPECT_EQ(m.lower_bound(INT_MAX)->second, 3);
    EXPECT_EQ(m.at(INT_MAX), 3);
    EXPECT_EQ(m.erase(INT_MIN), 1u);
    EXPECT_EQ(m.erase(INT_MAX), 1u);
    EXPECT_EQ(m.size(), 1u);
    sgcl::sorted_multiset<unsigned> s = {0u, UINT_MAX, UINT_MAX, 0u};
    EXPECT_EQ(s.count(UINT_MAX), 2u);
    EXPECT_EQ(s.count(0u), 2u);
    auto [first, last] = s.equal_range(UINT_MAX);
    EXPECT_EQ(std::distance(first, last), 2);
    EXPECT_EQ(last, s.end());
    EXPECT_EQ(s.lower_bound(1u), first);
}

namespace {
    // Every member on a tree without a header (default, moved from): it
    // works as an empty one, and takes elements again after
    template<class M, class K, class E>
    void expect_tree_works_empty(M& m, const K& key, const E& element) {
        EXPECT_TRUE(m.empty());
        EXPECT_EQ(m.size(), 0u);
        EXPECT_EQ(m.begin(), m.end());
        EXPECT_EQ(m.rbegin(), m.rend());
        EXPECT_EQ(m.find(key), m.end());
        EXPECT_FALSE(m.contains(key));
        EXPECT_EQ(m.count(key), 0u);
        EXPECT_EQ(m.lower_bound(key), m.end());
        EXPECT_EQ(m.upper_bound(key), m.end());
        auto [first, last] = m.equal_range(key);
        EXPECT_EQ(first, m.end());
        EXPECT_EQ(last, m.end());
        EXPECT_EQ(m.erase(key), 0u);
        EXPECT_EQ(m.erase(m.begin(), m.end()), m.end());
        EXPECT_TRUE(m.extract(key).empty());
        EXPECT_EQ(sgcl::erase_if(m, [](auto&) { return true; }), 0u);
        EXPECT_TRUE(m == M());
        EXPECT_FALSE(m < M());
        M copy(m);
        EXPECT_TRUE(copy.empty());
        M other;
        m.swap(other);
        m.merge(other);
        other.merge(m);
        EXPECT_TRUE(m.empty());
        m.clear();
        m.insert(m.end(), element);   // the end() of no header as the hint
        EXPECT_EQ(m.size(), 1u);
        EXPECT_TRUE(m.contains(key));
        m.clear();
        EXPECT_TRUE(m.empty());
    }
}

TEST(SortedContainers_Test, MovedFromAndDefaultWorkAsEmpty) {
    sgcl::sorted_map<sgcl::string, int> m = {{"a", 1}, {"b", 2}};
    sgcl::sorted_map<sgcl::string, int> to(std::move(m));
    EXPECT_EQ(to.size(), 2u);
    EXPECT_FALSE(m.take("a").has_value());
    EXPECT_EQ(m.value_or("a", -1), -1);
    EXPECT_THROW(m.at("a"), std::out_of_range);
    expect_tree_works_empty(m, sgcl::string("a"), std::pair<const sgcl::string, int>("a", 1));
    sgcl::sorted_map<sgcl::string, int> assigned = {{"c", 3}};
    assigned = std::move(to);
    EXPECT_EQ(assigned.size(), 2u);
    expect_tree_works_empty(to, sgcl::string("a"), std::pair<const sgcl::string, int>("a", 1));
    sgcl::sorted_map<int, int> d;
    expect_tree_works_empty(d, 1, std::pair<const int, int>(1, 1));
    sgcl::sorted_multimap<int, int> mm = {{1, 1}, {1, 2}};
    auto mm2 = std::move(mm);
    expect_tree_works_empty(mm, 1, std::pair<const int, int>(1, 1));
    sgcl::sorted_set<int> s = {1, 2};
    auto s2 = std::move(s);
    expect_tree_works_empty(s, 1, 1);
    sgcl::sorted_multiset<int> ms = {1, 1};
    auto ms2 = std::move(ms);
    expect_tree_works_empty(ms, 1, 1);
    EXPECT_EQ(ms2.size(), 2u);
}

namespace {
    // A tree on both sides: a copy and a move assignment to itself, swap
    // with itself and a merge of itself change nothing
    template<class M>
    void expect_tree_keeps_itself(M m) {
        const M before = m;
        auto first = m.begin();
        auto end = m.end();
        auto& self = m;
        m = self;
        m = std::move(self);
        m.swap(self);
        swap(m, self);
        m.merge(self);
        m.merge(std::move(self));
        EXPECT_TRUE(m == before);
        EXPECT_EQ(m.begin(), first);   // nothing was rebuilt
        EXPECT_EQ(m.end(), end);
        EXPECT_TRUE(tree_is_valid(m));
        EXPECT_TRUE(m == self);
        EXPECT_FALSE(m < self);
    }
}

TEST(SortedContainers_Test, ATreeOnBothSidesKeepsItself) {
    expect_tree_keeps_itself(sgcl::sorted_map<sgcl::string, int>{{"a", 1}, {"b", 2}, {"c", 3}});
    expect_tree_keeps_itself(sgcl::sorted_multimap<int, int>{{1, 1}, {1, 2}, {2, 3}});
    expect_tree_keeps_itself(sgcl::sorted_set<int>{1, 2, 3});
    expect_tree_keeps_itself(sgcl::sorted_multiset<int>{1, 1, 2});
    expect_tree_keeps_itself(sgcl::sorted_set<int>());
}

// The tree's own element, key or value as the argument: an insertion of
// an element it holds (nothing in a unique tree, a copy beside it in a
// multi one), with and without a hint; operator[], try_emplace,
// insert_or_assign of the value under the key; take, extract and erase by
// the key of the element they take, the multi trees' run whole, with keys
// a destructor overwrites
TEST(SortedContainers_Test, TheTreesOwnElementAsTheArgument) {
    using boundary::Poisoned;
    sgcl::sorted_map<Poisoned, Poisoned> m;
    for (int i = 0; i < 8; ++i) {
        m.emplace(i, i * 10);
    }
    auto it = m.find(3);
    EXPECT_FALSE(m.insert(*it).second);
    EXPECT_FALSE(m.emplace(*it).second);
    EXPECT_EQ(m.insert(it, *it), it);
    EXPECT_EQ(m.emplace_hint(m.end(), *it), it);
    EXPECT_FALSE(m.try_emplace(it->first, 0).second);
    EXPECT_EQ(&m[it->first], &it->second);
    EXPECT_FALSE(m.insert_or_assign(it->first, it->second).second);
    EXPECT_EQ(it->second, Poisoned(30));
    EXPECT_EQ(m.erase(m.find(5)->first), 1u);
    auto taken = m.take(m.find(6)->first);
    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(*taken, Poisoned(60));
    auto nh = m.extract(m.find(7)->first);
    ASSERT_FALSE(nh.empty());
    EXPECT_EQ(nh.key(), Poisoned(7));
    EXPECT_EQ(m.size(), 5u);
    EXPECT_TRUE(tree_is_valid(m));

    sgcl::sorted_multimap<Poisoned, int> mm;
    for (int i = 0; i < 3; ++i) {
        mm.emplace(1, i);
        mm.emplace(2, i);
    }
    for (int i = 0; i < 10; ++i) {
        mm.insert(*mm.find(1));
        mm.insert(mm.begin(), *mm.find(2));
    }
    EXPECT_EQ(mm.count(1), 13u);
    EXPECT_EQ(mm.count(2), 13u);
    EXPECT_EQ(mm.erase(mm.find(1)->first), 13u);
    auto run = mm.equal_range(2);
    EXPECT_EQ(mm.erase(std::next(run.first, 5)->first), 13u);
    EXPECT_TRUE(mm.empty());
    sgcl::sorted_multiset<Poisoned> ms = {4, 4, 4, 5};
    EXPECT_EQ(ms.erase(*std::next(ms.begin())), 3u);
    EXPECT_EQ(ms.size(), 1u);
    sgcl::sorted_set<Poisoned> s = {1, 2};
    EXPECT_EQ(s.erase(*s.begin()), 1u);
    EXPECT_FALSE(s.insert(*s.begin()).second);
    EXPECT_EQ(s.size(), 1u);
    EXPECT_TRUE(tree_is_valid(s));
}

// One element and empty ranges: an empty range erased at either end, the
// one element erased by iterator, by key and by range, bounds around it
TEST(SortedContainers_Test, OneElementAndEmptyRanges) {
    sgcl::sorted_set<int> s = {5};
    EXPECT_EQ(s.erase(s.begin(), s.begin()), s.begin());
    EXPECT_EQ(s.erase(s.end(), s.end()), s.end());
    EXPECT_EQ(s.lower_bound(4), s.begin());
    EXPECT_EQ(s.upper_bound(5), s.end());
    EXPECT_EQ(&s.min(), &s.max());
    EXPECT_EQ(std::prev(s.end()), s.begin());
    EXPECT_EQ(s.erase(s.begin()), s.end());
    EXPECT_TRUE(s.empty());
    s.insert(5);
    EXPECT_EQ(s.erase(5), 1u);
    s.insert(5);
    EXPECT_EQ(s.erase(s.begin(), s.end()), s.end());
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(tree_is_valid(s));
    std::vector<int> none;
    s.insert(none.begin(), none.end());
    s.insert(std::initializer_list<int>{});
    EXPECT_TRUE(s.empty());
}

// The iterators as the pages state them: end() is the header, made by the
// first insertion (an end() of no header is not end() after it), kept by
// clear and a copy assignment, taken along by swap and the moves; a move
// assignment brings the other tree's header. The pages of sorted_set and
// sorted_multiset said every operator= kept end()
TEST(SortedContainers_Test, IteratorsAsThePagesStateThem) {
    sgcl::sorted_set<int> s;
    auto no_header = s.end();
    s.insert(1);
    EXPECT_NE(s.end(), no_header);
    auto end = s.end();
    auto one = s.begin();
    for (int i = 2; i < 100; ++i) {
        s.insert(i);
    }
    s.erase(50);
    EXPECT_EQ(s.begin(), one);
    EXPECT_EQ(s.end(), end);
    s.clear();
    EXPECT_EQ(s.end(), end);
    const sgcl::sorted_set<int> copy = {7, 8};
    s = copy;
    EXPECT_EQ(s.end(), end);
    s = {9};
    EXPECT_EQ(s.end(), end);
    sgcl::sorted_set<int> other = {3};
    auto other_end = other.end();
    s = std::move(other);
    EXPECT_EQ(s.end(), other_end);
    EXPECT_NE(s.end(), end);
    sgcl::sorted_multiset<int> a = {1, 1};
    sgcl::sorted_multiset<int> b;
    auto a_first = a.begin();
    auto a_end = a.end();
    a.swap(b);
    EXPECT_EQ(b.begin(), a_first);
    EXPECT_EQ(b.end(), a_end);
    sgcl::sorted_multiset<int> moved(std::move(b));
    EXPECT_EQ(moved.end(), a_end);
}

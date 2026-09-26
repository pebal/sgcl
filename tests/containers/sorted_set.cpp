//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include "sgcl/core/sorted_map.h"
#include "sgcl/core/sorted_multimap.h"
#include "sgcl/core/sorted_multiset.h"
#include "sgcl/core/sorted_set.h"

#include <algorithm>
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

    struct ThrowingLess {
        inline static int countdown = -1;

        bool operator()(int a, int b) const {
            if (countdown >= 0 && countdown-- == 0) {
                throw std::runtime_error("compare");
            }
            return a < b;
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

TEST(SortedSet_Test, ThrowingComparator) {
    sgcl::sorted_set<int, ThrowingLess> s;
    for (int i = 0; i < 32; ++i) {
        s.insert(i * 2);
    }
    std::vector<int> before = elements_of(s);
    off_frame([&] {   // nodes built for the failed inserts must not linger in this frame
    for (int at : {0, 2, 4}) {
        ThrowingLess::countdown = at;
        EXPECT_THROW(s.insert(33), std::runtime_error);
        ThrowingLess::countdown = -1;
        EXPECT_EQ(elements_of(s), before);
        ThrowingLess::countdown = at;
        EXPECT_THROW(s.emplace(35), std::runtime_error);
        ThrowingLess::countdown = -1;
        EXPECT_EQ(elements_of(s), before);
        ThrowingLess::countdown = at;
        EXPECT_THROW(s.erase(36), std::runtime_error);
        ThrowingLess::countdown = -1;
        EXPECT_EQ(elements_of(s), before);
        EXPECT_TRUE(tree_is_valid(s));
    }
    });
    EXPECT_EQ(collector::get_live_object_count(), 33u);
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

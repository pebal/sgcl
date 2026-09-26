//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include "sgcl/core/sorted_map.h"
#include "sgcl/core/sorted_multimap.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    static_assert(std::bidirectional_iterator<sgcl::sorted_map<int, int>::iterator>);
    static_assert(std::bidirectional_iterator<sgcl::sorted_map<int, int>::const_iterator>);
    static_assert(std::bidirectional_iterator<sgcl::sorted_map<std::string, std::string>::reverse_iterator>);
    static_assert(std::ranges::bidirectional_range<sgcl::sorted_map<int, int>>);
    static_assert(std::convertible_to<sgcl::sorted_map<int, int>::iterator, sgcl::sorted_map<int, int>::const_iterator>);
    static_assert(!std::convertible_to<sgcl::sorted_map<int, int>::const_iterator, sgcl::sorted_map<int, int>::iterator>);
    static_assert(!std::is_copy_constructible_v<sgcl::sorted_map<int, int>::node_type>);
    static_assert(std::is_move_constructible_v<sgcl::sorted_map<int, int>::node_type>);
    // One raw node pointer: storable anywhere, not only on the stack.
    static_assert(std::is_trivially_copyable_v<sgcl::sorted_map<int, int>::iterator>);
    static_assert(std::is_trivially_copyable_v<sgcl::sorted_map<int, int>::const_iterator>);
    static_assert(std::is_trivially_copyable_v<sgcl::sorted_multimap<int, int>::iterator>);
    static_assert(sizeof(sgcl::sorted_map<int, int>::iterator) == sizeof(void*));

    template<class M, class K>
    concept LooksUpWith = requires(M& m, const K& k) {
        m.find(k);
        m.count(k);
        m.contains(k);
        m.equal_range(k);
        m.lower_bound(k);
        m.upper_bound(k);
    };

    // std::string is only explicitly constructible from a string_view: the
    // lookup compiles for a transparent comparator alone.
    static_assert(LooksUpWith<sgcl::sorted_map<std::string, int, std::less<>>, std::string_view>);
    static_assert(!LooksUpWith<sgcl::sorted_map<std::string, int>, std::string_view>);
    static_assert(LooksUpWith<sgcl::sorted_map<std::string, int>, std::string>);

    template<class M>
    std::vector<typename M::key_type> keys_of(const M& m) {
        std::vector<typename M::key_type> result;
        for (auto& [k, v] : m) {
            result.push_back(k);
        }
        return result;
    }

    struct Holder {
        tracked_ptr<Baz> ptr;
    };

    struct Owner {
        sgcl::sorted_map<int, Int> values;
    };

    struct Pinned {
        int value;

        explicit Pinned(int v)
        : value(v) {
        }

        Pinned(const Pinned&) = delete;
        Pinned(Pinned&&) = delete;
        Pinned& operator=(const Pinned&) = delete;
        Pinned& operator=(Pinned&&) = delete;
    };

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
        // Throws on the countdown-th comparison once armed.
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

    // A mapped value whose copy throws on the countdown-th copy once armed.
    struct ThrowingCopy {
        int value;
        inline static int countdown = -1;
        inline static size_t live = 0;

        explicit ThrowingCopy(int v)
        : value(v) {
            ++live;
        }

        ThrowingCopy(const ThrowingCopy& other)
        : value(other.value) {
            if (countdown >= 0 && countdown-- == 0) {
                throw std::runtime_error("copy");
            }
            ++live;
        }

        ~ThrowingCopy() {
            --live;
        }
    };

    // Two owners of a node handle, made before and after the node they
    // take: the sweep destroys the owner and the node in either order.
    struct HandleOwnerBefore {
        sgcl::sorted_map<int, Int>::node_type handle;
    };

    struct HandleOwnerAfter {
        sgcl::sorted_map<int, Int>::node_type handle;
    };
}

TEST(SortedMap_Test, DefaultConstructorEmpty) {
    sgcl::sorted_map<int, int> m;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.begin(), m.end());
    EXPECT_EQ(m.cbegin(), m.cend());
    EXPECT_EQ(m.rbegin(), m.rend());
    EXPECT_EQ(m.crbegin(), m.crend());
    EXPECT_EQ(m.find(1), m.end());
    EXPECT_EQ(m.count(1), 0u);
    EXPECT_FALSE(m.contains(1));
    EXPECT_EQ(m.lower_bound(1), m.end());
    EXPECT_EQ(m.upper_bound(1), m.end());
    EXPECT_EQ(m.equal_range(1).first, m.end());
    EXPECT_EQ(m.erase(1), 0u);
    EXPECT_TRUE(m.extract(1).empty());
    EXPECT_EQ(m.begin(), m.erase(m.begin(), m.end()));
    m.clear();
    EXPECT_TRUE(tree_is_valid(m));
    EXPECT_GT(m.max_size(), 0u);
    static_assert(noexcept(sgcl::sorted_map<int, int>()));
}

TEST(SortedMap_Test, ConstructorComparator) {
    sgcl::sorted_map<int, int, std::greater<int>> m(std::greater<int>{});
    m.insert({{1, 10}, {3, 30}, {2, 20}});
    EXPECT_EQ(keys_of(m), (std::vector<int>{3, 2, 1}));
    EXPECT_TRUE(m.key_comp()(2, 1));
    EXPECT_TRUE(m.value_comp()(*m.begin(), *std::next(m.begin())));
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, ConstructorRange) {
    std::vector<std::pair<int, int>> src = {{3, 30}, {1, 10}, {2, 20}, {1, 11}};
    sgcl::sorted_map<int, int> m(src.begin(), src.end());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(keys_of(m), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(m.at(1), 10);
    sgcl::sorted_map<int, int, std::greater<int>> g(src.begin(), src.end(), std::greater<int>{});
    EXPECT_EQ(keys_of(g), (std::vector<int>{3, 2, 1}));
}

TEST(SortedMap_Test, RangeOfAnotherType) {
    // A range whose elements are not the value type: each is converted
    // once, into its node, and the node's key compared. Pairs of a
    // string_view and an int into string keys, which std::less<std::string>
    // cannot compare with a string_view at all.
    std::vector<std::pair<std::string_view, int>> src = {{"b", 2}, {"a", 1}, {"c", 3}, {"a", 4}};
    sgcl::sorted_map<std::string, int> m(src.begin(), src.end());
    EXPECT_EQ(keys_of(m), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(m["a"], 1);
    EXPECT_TRUE(tree_is_valid(m));
    m.insert(src.begin(), src.end());
    EXPECT_EQ(m.size(), 3u);
    sgcl::sorted_multimap<std::string, int> mm(src.begin(), src.end());
    EXPECT_EQ(mm.size(), 4u);
    EXPECT_EQ(keys_of(mm), (std::vector<std::string>{"a", "a", "b", "c"}));
    // one conversion per element, none per comparison
    std::vector<std::pair<int, int>> ints = {{3, 30}, {1, 10}, {2, 20}, {1, 11}, {5, 50}, {4, 40}};
    FromInt::conversions = 0;
    sgcl::sorted_map<FromInt, int> from_ints(ints.begin(), ints.end());
    EXPECT_EQ(FromInt::conversions, ints.size());
    EXPECT_EQ(from_ints.size(), 5u);
    EXPECT_EQ(from_ints.begin()->first.value, 1);
    EXPECT_EQ(from_ints.begin()->second, 10);
    from_ints.insert(ints.begin(), ints.end());
    EXPECT_EQ(FromInt::conversions, 2 * ints.size());
    EXPECT_EQ(from_ints.size(), 5u);
    EXPECT_TRUE(tree_is_valid(from_ints));
}

TEST(SortedMap_Test, InitializerList) {
    sgcl::sorted_map<std::string, int> m = {{"b", 2}, {"a", 1}, {"c", 3}, {"a", 4}};
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(keys_of(m), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(m["a"], 1);
    sgcl::sorted_map<std::string, int, std::greater<>> g({{"b", 2}, {"a", 1}}, std::greater<>{});
    EXPECT_EQ(keys_of(g), (std::vector<std::string>{"b", "a"}));
}

TEST(SortedMap_Test, CopyConstructor) {
    sgcl::sorted_map<int, Int> other = {{1, 1}, {2, 2}, {3, 3}};
    sgcl::sorted_map<int, Int> m(other);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m, other);
    EXPECT_TRUE(tree_is_valid(m));
    m[4] = 4;
    EXPECT_EQ(other.size(), 3u);
}

TEST(SortedMap_Test, CopyOfALargeTree) {
    // A copy is made shape for shape, without comparisons: the same
    // order, the invariants, nodes of its own; for a multimap the
    // equivalent keys in the same order.
    sgcl::sorted_map<int, Int> other;
    sgcl::sorted_multimap<int, Int> others;
    std::mt19937 rng(777);
    std::uniform_int_distribution<int> key(0, 100000);
    off_frame([&] {
        for (int i = 0; i < 2000; ++i) {
            int k = key(rng);
            other.emplace(k, i);
            others.emplace(k % 100, i);
        }
    });
    size_t before = Int::counter;
    sgcl::sorted_map<int, Int> m(other);
    EXPECT_TRUE(tree_is_valid(m));
    EXPECT_EQ(m, other);
    EXPECT_EQ(m.size(), other.size());
    EXPECT_EQ(Int::counter, before + other.size());
    EXPECT_EQ(collector::get_live_object_count(), 2 * other.size() + others.size() + 3);
    off_frame([&] {
        EXPECT_NE(&*m.begin(), &*other.begin());
        EXPECT_TRUE(std::equal(m.rbegin(), m.rend(), other.rbegin(), other.rend()));
    });
    sgcl::sorted_multimap<int, Int> ms = others;
    EXPECT_TRUE(tree_is_valid(ms));
    EXPECT_EQ(ms, others);
    off_frame([&] {
        auto it = others.begin();
        for (auto& [k, v] : ms) {
            EXPECT_EQ(k, it->first);
            EXPECT_EQ(v, it->second);
            ++it;
        }
    });
    // assigned over a non-empty map: its elements die first
    sgcl::sorted_map<int, Int> assigned = {{-1, -1}, {-2, -2}};
    assigned = other;
    EXPECT_TRUE(tree_is_valid(assigned));
    EXPECT_EQ(assigned, other);
    EXPECT_EQ(Int::counter, before + 2 * other.size() + others.size());
    // an element copy that throws: the copy holds nothing, the source is
    // untouched, the nodes made so far are garbage
    using Throwing = sgcl::sorted_map<int, ThrowingCopy>;
    Throwing source;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            source.try_emplace(i, i);
        }
    });
    off_frame([&] {   // the partial trees are garbage: their nodes must not linger in this frame's words
        for (int at : {0, 1, 37, 99}) {
            ThrowingCopy::countdown = at;
            EXPECT_THROW(Throwing c(source), std::runtime_error);
            ThrowingCopy::countdown = -1;
            EXPECT_EQ(ThrowingCopy::live, 100u);
            EXPECT_TRUE(tree_is_valid(source));
            Throwing target;
            target.try_emplace(-1, -1);
            ThrowingCopy::countdown = at;
            EXPECT_THROW(target = source, std::runtime_error);
            ThrowingCopy::countdown = -1;
            EXPECT_TRUE(target.empty());
            EXPECT_TRUE(tree_is_valid(target));
            EXPECT_EQ(ThrowingCopy::live, 100u);
        }
    });
    collector::clear_stack();
    EXPECT_EQ(collector::get_live_object_count(), 3 * other.size() + 2 * others.size() + 100 + 6);
}

TEST(SortedMap_Test, MoveConstructor) {
    sgcl::sorted_map<int, Int> other = {{1, 1}, {2, 2}, {3, 3}};
    auto it = other.find(2);
    sgcl::sorted_map<int, Int> m(std::move(other));
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.begin(), other.end());
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(keys_of(m), (std::vector<int>{1, 2, 3}));
    // the iterator follows the node into the new container
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(++it, m.find(3));
    other[7] = 7;
    EXPECT_EQ(other.size(), 1u);
    EXPECT_TRUE(tree_is_valid(other));
}

TEST(SortedMap_Test, CopyAssignment) {
    sgcl::sorted_map<int, Int> other = {{1, 1}, {2, 2}, {3, 3}};
    sgcl::sorted_map<int, Int> m = {{9, 9}};
    m = other;
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(m, other);
    m = m;
    EXPECT_EQ(m.size(), 3u);
    other = sgcl::sorted_map<int, Int>();
    m = other;
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.begin(), m.end());
}

TEST(SortedMap_Test, MoveAssignment) {
    sgcl::sorted_map<int, Int> other = {{4, 4}, {5, 5}, {6, 6}};
    sgcl::sorted_map<int, Int> m = {{1, 1}};
    m = std::move(other);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(keys_of(m), (std::vector<int>{4, 5, 6}));
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, ListAssignment) {
    sgcl::sorted_map<int, Int> m = {{1, 1}};
    m = {{7, 7}, {8, 8}, {9, 9}};
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(keys_of(m), (std::vector<int>{7, 8, 9}));
    m = std::initializer_list<std::pair<const int, Int>>();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(m.empty());
}

TEST(SortedMap_Test, Iteration) {
    sgcl::sorted_map<int, int> m = {{5, 50}, {1, 10}, {3, 30}, {4, 40}, {2, 20}};
    std::vector<int> forward;
    for (auto it = m.begin(); it != m.end(); it++) {
        forward.push_back(it->first);
    }
    EXPECT_EQ(forward, (std::vector<int>{1, 2, 3, 4, 5}));
    std::vector<int> backward;
    for (auto it = m.end(); it != m.begin();) {
        --it;
        backward.push_back((*it).second);
    }
    EXPECT_EQ(backward, (std::vector<int>{50, 40, 30, 20, 10}));
    std::vector<int> reverse;
    for (auto it = m.rbegin(); it != m.rend(); ++it) {
        reverse.push_back(it->first);
    }
    EXPECT_EQ(reverse, (std::vector<int>{5, 4, 3, 2, 1}));
    const auto& cm = m;
    EXPECT_EQ(std::distance(cm.begin(), cm.end()), 5);
    EXPECT_EQ(std::distance(cm.crbegin(), cm.crend()), 5);
    EXPECT_EQ(std::prev(m.end())->first, 5);
    EXPECT_EQ((--m.end())->first, 5);
    auto it = m.begin();
    auto copy = it++;
    EXPECT_EQ(copy->first, 1);
    EXPECT_EQ(it->first, 2);
    copy = it--;
    EXPECT_EQ(copy->first, 2);
    EXPECT_EQ(it->first, 1);
    sgcl::sorted_map<int, int>::const_iterator cit = it;
    EXPECT_EQ(cit, it);
    EXPECT_EQ(it, cit);
    EXPECT_TRUE(std::ranges::is_sorted(m));
    EXPECT_TRUE(std::ranges::is_sorted(m | std::views::keys));
    EXPECT_EQ(std::ranges::distance(m | std::views::reverse), 5);
    it->second = 99;
    EXPECT_EQ(m[1], 99);
}

TEST(SortedMap_Test, Insert) {
    sgcl::sorted_map<int, std::string> m;
    std::pair<const int, std::string> v(1, "one");
    auto r = m.insert(v);
    EXPECT_TRUE(r.second);
    EXPECT_EQ(r.first->second, "one");
    r = m.insert(v);
    EXPECT_FALSE(r.second);
    EXPECT_EQ(r.first, m.begin());
    std::string moved = "two";
    r = m.insert(std::pair<const int, std::string>(2, std::move(moved)));
    EXPECT_TRUE(r.second);
    EXPECT_EQ(m[2], "two");
    r = m.insert(std::pair<int, const char*>(3, "three"));
    EXPECT_TRUE(r.second);
    r = m.insert({3, "again"});
    EXPECT_FALSE(r.second);
    EXPECT_EQ(m[3], "three");
    auto it = m.insert(m.end(), {4, "four"});
    EXPECT_EQ(it->first, 4);
    it = m.insert(m.begin(), std::pair<const int, std::string>(0, "zero"));
    EXPECT_EQ(it, m.begin());
    it = m.insert(m.find(4), std::pair<int, const char*>(4, "dup"));
    EXPECT_EQ(it->second, "four");
    std::vector<std::pair<int, std::string>> more = {{6, "six"}, {5, "five"}, {6, "dup"}};
    m.insert(more.begin(), more.end());
    m.insert({{7, "seven"}, {5, "dup"}});
    EXPECT_EQ(m.size(), 8u);
    EXPECT_EQ(keys_of(m), (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}));
    EXPECT_EQ(m[6], "six");
    EXPECT_EQ(m[5], "five");
    EXPECT_TRUE(tree_is_valid(m));
    EXPECT_EQ(collector::get_live_object_count(), 9u);
}

TEST(SortedMap_Test, Emplace) {
    sgcl::sorted_map<std::string, Int> m;
    auto r = m.emplace("b", 2);
    EXPECT_TRUE(r.second);
    r = m.emplace(std::piecewise_construct, std::forward_as_tuple("a"), std::forward_as_tuple(1));
    EXPECT_TRUE(r.second);
    EXPECT_EQ(r.first, m.begin());
    r = m.emplace("a", 5);
    EXPECT_FALSE(r.second);
    EXPECT_EQ(r.first->second, 1);
    EXPECT_EQ(Int::counter, 2u);
    auto it = m.emplace_hint(m.end(), "c", 3);
    EXPECT_EQ(it->first, "c");
    it = m.emplace_hint(m.begin(), "c", 4);
    EXPECT_EQ(it->second, 3);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(keys_of(m), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, TryEmplaceNonCopyable) {
    sgcl::sorted_map<int, std::unique_ptr<int>> m;
    auto r = m.try_emplace(1, new int(10));
    EXPECT_TRUE(r.second);
    r = m.try_emplace(1, new int(11));
    EXPECT_FALSE(r.second);
    EXPECT_EQ(*r.first->second, 10);
    auto it = m.try_emplace(m.end(), 2, std::make_unique<int>(20));
    EXPECT_EQ(*it->second, 20);
    int key = 3;
    it = m.try_emplace(m.end(), key, nullptr);
    EXPECT_EQ(it->second, nullptr);
    m[3] = std::make_unique<int>(30);
    EXPECT_EQ(*m[3], 30);
    EXPECT_EQ(m[4], nullptr);
    EXPECT_EQ(m.size(), 4u);
    EXPECT_EQ(*m.at(1), 10);
    m.erase(1);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, TryEmplaceNonMovable) {
    sgcl::sorted_map<int, Pinned> m;
    auto r = m.try_emplace(1, 10);
    EXPECT_TRUE(r.second);
    EXPECT_EQ(r.first->second.value, 10);
    r = m.try_emplace(1, 11);
    EXPECT_FALSE(r.second);
    auto it = m.try_emplace(m.end(), 2, 20);
    EXPECT_EQ(it->second.value, 20);
    m.emplace(std::piecewise_construct, std::forward_as_tuple(3), std::forward_as_tuple(30));
    EXPECT_EQ(m.size(), 3u);
    auto nh = m.extract(2);
    EXPECT_EQ(nh.mapped().value, 20);
    m.insert(std::move(nh));
    EXPECT_EQ(m.at(2).value, 20);
}

TEST(SortedMap_Test, InsertOrAssign) {
    sgcl::sorted_map<std::string, Int> m;
    auto r = m.insert_or_assign("a", 1);
    EXPECT_TRUE(r.second);
    r = m.insert_or_assign("a", 2);
    EXPECT_FALSE(r.second);
    EXPECT_EQ(r.first->second, 2);
    std::string key = "b";
    auto it = m.insert_or_assign(m.end(), key, 3);
    EXPECT_EQ(it->second, 3);
    it = m.insert_or_assign(m.begin(), std::move(key), 4);
    EXPECT_EQ(it->second, 4);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(Int::counter, 2u);
}

TEST(SortedMap_Test, IndexAndAt) {
    sgcl::sorted_map<std::string, int> m;
    m["x"] = 1;
    EXPECT_EQ(m["x"], 1);
    EXPECT_EQ(m["y"], 0);
    std::string key = "z";
    m[std::move(key)] = 3;
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at("z"), 3);
    const auto& cm = m;
    EXPECT_EQ(cm.at("x"), 1);
    EXPECT_THROW(m.at("w"), std::out_of_range);
    EXPECT_THROW(cm.at("w"), std::out_of_range);
    EXPECT_EQ(m.size(), 3u);
}

TEST(SortedMap_Test, Erase) {
    sgcl::sorted_map<int, Int> m;
    // The frame with the iterators and the references into the nodes is
    // dead and cleared before the count.
    off_frame([&] {
        for (int i = 0; i < 10; ++i) {
            m.emplace(i, i);
        }
        EXPECT_EQ(Int::counter, 10u);
        auto it = m.erase(m.find(3));
        EXPECT_EQ(it->first, 4);
        EXPECT_EQ(Int::counter, 9u);
        sgcl::sorted_map<int, Int>::const_iterator cit = m.find(4);
        it = m.erase(cit);
        EXPECT_EQ(it->first, 5);
        EXPECT_EQ(m.erase(5), 1u);
        EXPECT_EQ(m.erase(5), 0u);
        EXPECT_EQ(Int::counter, 7u);
        it = m.erase(m.find(1), m.find(8));
        EXPECT_EQ(it->first, 8);
        EXPECT_EQ(keys_of(m), (std::vector<int>{0, 8, 9}));
        EXPECT_EQ(Int::counter, 3u);
        EXPECT_TRUE(tree_is_valid(m));
        it = m.erase(std::prev(m.end()));
        EXPECT_EQ(it, m.end());
        it = m.erase(m.begin(), m.end());
        EXPECT_EQ(it, m.end());
        EXPECT_TRUE(m.empty());
        EXPECT_EQ(Int::counter, 0u);
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    m[1] = 1;
    EXPECT_EQ(m.size(), 1u);
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, EraseIf) {
    sgcl::sorted_map<int, Int> m;
    for (int i = 0; i < 10; ++i) {
        m.emplace(i, i);
    }
    auto erased = std::erase_if(m, [](const auto& p) { return p.first % 2 == 0; });
    EXPECT_EQ(erased, 5u);
    EXPECT_EQ(keys_of(m), (std::vector<int>{1, 3, 5, 7, 9}));
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_TRUE(tree_is_valid(m));
    erased = erase_if(m, [](const auto&) { return true; });
    EXPECT_EQ(erased, 5u);
    EXPECT_TRUE(m.empty());
}

TEST(SortedMap_Test, ClearAndDestructorAreEager) {
    off_frame([] {
        sgcl::sorted_map<int, Int> m = {{1, 1}, {2, 2}, {3, 3}};
        EXPECT_EQ(Int::counter, 3u);
        m.clear();
        EXPECT_EQ(Int::counter, 0u);
        EXPECT_TRUE(m.empty());
        EXPECT_EQ(m.begin(), m.end());
        EXPECT_EQ(collector::get_live_object_count(), 1u);
        m[4] = 4;
        EXPECT_EQ(Int::counter, 1u);
        EXPECT_TRUE(tree_is_valid(m));
    });
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(SortedMap_Test, Swap) {
    sgcl::sorted_map<int, Int> a = {{1, 1}, {2, 2}};
    sgcl::sorted_map<int, Int> b = {{3, 3}, {4, 4}, {5, 5}};
    auto it = a.find(2);
    a.swap(b);
    EXPECT_EQ(keys_of(a), (std::vector<int>{3, 4, 5}));
    EXPECT_EQ(keys_of(b), (std::vector<int>{1, 2}));
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(++it, b.end());
    swap(a, b);
    EXPECT_EQ(keys_of(a), (std::vector<int>{1, 2}));
    std::swap(a, b);
    EXPECT_EQ(keys_of(a), (std::vector<int>{3, 4, 5}));
    sgcl::sorted_map<int, Int> empty;
    a.swap(empty);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(keys_of(empty), (std::vector<int>{3, 4, 5}));
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_TRUE(tree_is_valid(a));
    EXPECT_TRUE(tree_is_valid(empty));
}

TEST(SortedMap_Test, IteratorsSurviveOtherErasures) {
    sgcl::sorted_map<int, int> m;
    for (int i = 0; i < 100; ++i) {
        m.emplace(i, i * 10);
    }
    auto it = m.find(50);
    auto first = m.begin();
    for (int i = 0; i < 100; ++i) {
        if (i != 50 && i != 0) {
            m.erase(i);
        }
    }
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(it->first, 50);
    EXPECT_EQ(it->second, 500);
    EXPECT_EQ(first, m.begin());
    EXPECT_EQ(std::next(first), it);
    EXPECT_EQ(std::next(it), m.end());
    EXPECT_EQ(std::prev(m.end()), it);
    for (int i = 100; i < 200; ++i) {
        m.emplace(i, i);
    }
    EXPECT_EQ(it->first, 50);
    EXPECT_EQ(std::next(it)->first, 100);
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, IteratorsInAVector) {
    using iterator = sgcl::sorted_map<int, int>::iterator;
    using const_iterator = sgcl::sorted_map<int, int>::const_iterator;
    sgcl::sorted_map<int, int> m;
    for (int i = 0; i < 100; ++i) {
        m.emplace(i, i * 10);
    }
    // On the heap, where a tracked_ptr could not live: the tree roots the
    // node behind each of them for as long as its element is in the map.
    std::vector<iterator> its;
    for (auto it = m.begin(); it != m.end(); ++it) {
        its.push_back(it);
    }
    std::vector<const_iterator> cits(its.begin(), its.end());
    ASSERT_EQ(its.size(), 100u);
    collector::force_collect(true);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(its[i]->first, i);
        EXPECT_EQ(cits[i]->second, i * 10);
        EXPECT_EQ(its[i], cits[i]);
    }
    for (int i = 1; i < 100; i += 2) {
        m.erase(i);
    }
    collector::force_collect(true);
    for (int i = 0; i < 100; i += 2) {
        EXPECT_EQ(its[i]->first, i);
        EXPECT_EQ(std::next(cits[i]), i + 2 < 100 ? cits[i + 2] : m.cend());
    }
    EXPECT_EQ(std::prev(m.end()), its[98]);
    its[4]->second = 4;
    EXPECT_EQ(m.at(4), 4);
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, RawIteratorSurvivesCollection) {
    sgcl::sorted_map<int, Holder> m;
    off_frame([&] {
        for (int i = 0; i < 50; ++i) {
            m[i].ptr = make_tracked<Baz>(i);
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), 101u);
    // Everything around the iterator's element comes and goes, with full
    // collections in between: the element stays, so the iterator stays.
    // Frames: the iterator and its checks never share one with an exact
    // count of nodes erased later, because a raw word left in a frame
    // retains its node until it is overwritten.
    off_frame([&] {
        auto it = m.find(25);
        for (int round = 0; round < 3; ++round) {
            off_frame([&] {
                for (int i = 0; i < 50; ++i) {
                    if (i != 25) {
                        m.erase(i);
                    }
                }
            });
            collector::force_collect(true);
            EXPECT_EQ(collector::get_live_object_count(), 3u);
            EXPECT_EQ(m.size(), 1u);
            off_frame([&] {
                EXPECT_EQ(it->first, 25);
                EXPECT_EQ(it->second.ptr->value, 25);
                EXPECT_EQ(it, m.begin());
                EXPECT_EQ(std::next(it), m.end());
                EXPECT_EQ(std::prev(m.end()), it);
            });
            off_frame([&] {
                for (int i = 0; i < 50; ++i) {
                    if (i != 25) {
                        m[i].ptr = make_tracked<Baz>(i + 100 * (round + 1));
                    }
                }
            });
            collector::force_collect(true);
            EXPECT_EQ(collector::get_live_object_count(), 101u);
            off_frame([&] {
                EXPECT_EQ(it->first, 25);
                EXPECT_EQ(std::next(it)->first, 26);
                EXPECT_EQ(std::prev(it)->second.ptr->value, 24 + 100 * (round + 1));
            });
            EXPECT_TRUE(tree_is_valid(m));
        }
        m.erase(it);
    });
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 99u);
    EXPECT_EQ(m.size(), 49u);
    EXPECT_FALSE(m.contains(25));
}

TEST(SortedMap_Test, NodeHandles) {
    sgcl::sorted_map<int, Int> m = {{1, 1}, {2, 2}, {3, 3}};
    sgcl::sorted_map<int, Int> empty;
    off_frame([&] {
        auto nh = m.extract(2);
        EXPECT_FALSE(nh.empty());
        EXPECT_TRUE(static_cast<bool>(nh));
        EXPECT_EQ(nh.key(), 2);
        EXPECT_EQ(nh.mapped(), 2);
        EXPECT_EQ(m.size(), 2u);
        EXPECT_EQ(Int::counter, 3u);
        EXPECT_TRUE(tree_is_valid(m));

        auto r = empty.insert(std::move(nh));
        EXPECT_TRUE(r.inserted);
        EXPECT_TRUE(nh.empty());
        EXPECT_TRUE(r.node.empty());
        EXPECT_EQ(r.position, empty.begin());
        EXPECT_EQ(empty.size(), 1u);
        EXPECT_TRUE(tree_is_valid(empty));

        // the same handle again: it is empty now
        r = empty.insert(std::move(nh));
        EXPECT_FALSE(r.inserted);
        EXPECT_EQ(r.position, empty.end());
        EXPECT_TRUE(r.node.empty());
        EXPECT_EQ(empty.size(), 1u);

        // an equivalent key: the handle keeps the node, the key may change
        nh = m.extract(m.begin());
        EXPECT_EQ(nh.key(), 1);
        m[1] = 10;
        r = m.insert(std::move(nh));
        EXPECT_FALSE(r.inserted);
        EXPECT_EQ(r.position->second, 10);
        EXPECT_FALSE(r.node.empty());
        EXPECT_EQ(r.node.key(), 1);
        r.node.key() = 7;
        r.node.mapped() = 70;
        auto it = m.insert(m.end(), std::move(r.node));
        EXPECT_EQ(it->first, 7);
        EXPECT_EQ(it->second, 70);
        EXPECT_TRUE(r.node.empty());
        EXPECT_EQ(keys_of(m), (std::vector<int>{1, 3, 7}));
        EXPECT_EQ(Int::counter, 4u);
        EXPECT_TRUE(tree_is_valid(m));

        // a handle dropped unused destroys its element
        {
            auto dropped = m.extract(7);
            EXPECT_EQ(Int::counter, 4u);
        }
        EXPECT_EQ(Int::counter, 3u);
        EXPECT_TRUE(m.extract(99).empty());
        EXPECT_EQ(m.insert(m.end(), sgcl::sorted_map<int, Int>::node_type()), m.end());

        // moving out empties the source
        auto a = m.extract(1);
        auto b = std::move(a);
        EXPECT_TRUE(a.empty());
        EXPECT_EQ(b.key(), 1);
        a = std::move(b);
        EXPECT_TRUE(b.empty());
        EXPECT_EQ(a.key(), 1);
        EXPECT_EQ(Int::counter, 3u);
    });
    // a died with its element; m holds 3, empty holds 2
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(keys_of(m), (std::vector<int>{3}));
    EXPECT_EQ(keys_of(empty), (std::vector<int>{2}));
    EXPECT_EQ(collector::get_live_object_count(), 4u);
}

TEST(SortedMap_Test, NodeHandleDyingInASweep) {
    // A handle inside a managed object nobody refers to any more: the
    // object dies in a sweep, and the node it holds is garbage of the
    // same sweep, which destroys the element itself, before or after the
    // handle's destructor runs. The handle must destroy nothing then:
    // exactly one destruction, and no touch of a node swept already.
    for (int round = 0; round < 20; ++round) {
        off_frame([] {
            tracked_ptr<HandleOwnerBefore> before = make_tracked<HandleOwnerBefore>();
            sgcl::sorted_map<int, Int> m = {{1, 1}, {2, 2}, {3, 3}};
            tracked_ptr<HandleOwnerAfter> after = make_tracked<HandleOwnerAfter>();
            before->handle = m.extract(1);
            after->handle = m.extract(3);
            EXPECT_EQ(m.size(), 1u);
            EXPECT_EQ(Int::counter, 3u);
            before = nullptr;
            after = nullptr;
        });
        collector::clear_stack();
        collector::force_collect(true);
        collector::force_collect(true);
        collector::force_collect(true);
        EXPECT_EQ(Int::counter, 0u);
        EXPECT_EQ(collector::get_live_object_count(), 0u);
    }
}

TEST(SortedMap_Test, Merge) {
    sgcl::sorted_map<int, Int> a = {{1, 1}, {3, 3}, {5, 5}};
    sgcl::sorted_map<int, Int> b = {{2, 2}, {3, 30}, {4, 4}};
    auto three = b.find(3);
    a.merge(b);
    EXPECT_EQ(keys_of(a), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(keys_of(b), (std::vector<int>{3}));
    EXPECT_EQ(a[3], 3);
    EXPECT_EQ(three, b.begin());
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_TRUE(tree_is_valid(a));
    EXPECT_TRUE(tree_is_valid(b));
    a.merge(a);
    EXPECT_EQ(a.size(), 5u);

    sgcl::sorted_multimap<int, Int> mm = {{0, 0}, {1, 10}, {6, 6}, {6, 60}};
    a.merge(mm);
    EXPECT_EQ(keys_of(a), (std::vector<int>{0, 1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(keys_of(mm), (std::vector<int>{1, 6}));
    EXPECT_EQ(a[6], 6);
    EXPECT_EQ(mm.find(6)->second, 60);
    sgcl::sorted_map<int, Int, std::greater<int>> g = {{7, 7}, {0, 100}};
    a.merge(std::move(g));
    EXPECT_EQ(keys_of(a), (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}));
    EXPECT_EQ(g.size(), 1u);
    EXPECT_EQ(Int::counter, 12u);
    sgcl::sorted_map<int, Int> empty;
    empty.merge(a);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(empty.size(), 8u);
    EXPECT_TRUE(tree_is_valid(empty));
    EXPECT_TRUE(tree_is_valid(a));
}

TEST(SortedMap_Test, Lookup) {
    sgcl::sorted_map<int, int> m = {{1, 1}, {3, 3}, {5, 5}, {7, 7}};
    const auto& cm = m;
    EXPECT_EQ(m.find(3)->second, 3);
    EXPECT_EQ(cm.find(4), cm.end());
    EXPECT_EQ(m.count(3), 1u);
    EXPECT_EQ(m.count(4), 0u);
    EXPECT_TRUE(m.contains(5));
    EXPECT_FALSE(m.contains(6));
    EXPECT_EQ(m.lower_bound(3)->first, 3);
    EXPECT_EQ(m.lower_bound(4)->first, 5);
    EXPECT_EQ(cm.lower_bound(8), cm.end());
    EXPECT_EQ(m.upper_bound(3)->first, 5);
    EXPECT_EQ(m.upper_bound(0)->first, 1);
    EXPECT_EQ(cm.upper_bound(7), cm.end());
    auto [first, last] = m.equal_range(5);
    EXPECT_EQ(first->first, 5);
    EXPECT_EQ(last->first, 7);
    auto [cfirst, clast] = cm.equal_range(4);
    EXPECT_EQ(cfirst, clast);
    EXPECT_EQ(cfirst->first, 5);
    auto [efirst, elast] = m.equal_range(9);
    EXPECT_EQ(efirst, m.end());
    EXPECT_EQ(elast, m.end());
}

TEST(SortedMap_Test, TransparentLookup) {
    sgcl::sorted_map<std::string, int, std::less<>> m = {{"apple", 1}, {"cherry", 3}, {"banana", 2}};
    const auto& cm = m;
    std::string_view banana = "banana";
    EXPECT_EQ(m.find(banana)->second, 2);
    EXPECT_EQ(cm.find(std::string_view("kiwi")), cm.end());
    EXPECT_EQ(m.find("apple")->second, 1);
    EXPECT_EQ(m.count(banana), 1u);
    EXPECT_TRUE(m.contains(banana));
    EXPECT_FALSE(cm.contains(std::string_view("kiwi")));
    EXPECT_EQ(m.lower_bound(std::string_view("b"))->first, "banana");
    EXPECT_EQ(cm.upper_bound(std::string_view("banana"))->first, "cherry");
    auto [first, last] = m.equal_range(banana);
    EXPECT_EQ(first->first, "banana");
    EXPECT_EQ(last->first, "cherry");
    auto [cfirst, clast] = cm.equal_range(std::string_view("z"));
    EXPECT_EQ(cfirst, cm.end());
    EXPECT_EQ(clast, cm.end());
}

TEST(SortedMap_Test, Comparison) {
    sgcl::sorted_map<int, int> a = {{1, 1}, {2, 2}};
    sgcl::sorted_map<int, int> b = {{1, 1}, {2, 2}};
    sgcl::sorted_map<int, int> c = {{1, 1}, {2, 3}};
    sgcl::sorted_map<int, int> d = {{1, 1}, {2, 2}, {3, 3}};
    sgcl::sorted_map<int, int> e;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(a < d);
    EXPECT_TRUE(d > a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(e < a);
    EXPECT_TRUE(e == (sgcl::sorted_map<int, int>()));
    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
    EXPECT_EQ(a <=> c, std::strong_ordering::less);
    EXPECT_EQ(d <=> a, std::strong_ordering::greater);

    sgcl::sorted_map<int, OnlyLess> x = {{1, {1}}};
    sgcl::sorted_map<int, OnlyLess> y = {{1, {2}}};
    EXPECT_TRUE(x < y);
    EXPECT_TRUE(x != y);
    EXPECT_EQ(x <=> y, std::weak_ordering::less);
    EXPECT_EQ(x <=> x, std::weak_ordering::equivalent);
}

TEST(SortedMap_Test, HintedInsertion) {
    sgcl::sorted_map<int, int> m;
    for (int i = 0; i < 1000; ++i) {
        auto it = m.emplace_hint(m.end(), i, i);
        EXPECT_EQ(it->first, i);
    }
    EXPECT_EQ(m.size(), 1000u);
    EXPECT_TRUE(tree_is_valid(m));
    for (int i = -1; i > -1000; --i) {
        auto it = m.insert(m.begin(), {i, i});
        EXPECT_EQ(it, m.begin());
    }
    EXPECT_TRUE(tree_is_valid(m));
    EXPECT_TRUE(std::ranges::is_sorted(m));
    // wrong hints still land in order
    auto it = m.emplace_hint(m.begin(), 5000, 0);
    EXPECT_EQ(it, std::prev(m.end()));
    it = m.emplace_hint(m.end(), -5000, 0);
    EXPECT_EQ(it, m.begin());
    it = m.emplace_hint(m.find(10), 500, 0);
    EXPECT_EQ(std::prev(it)->first, 499);
    it = m.emplace_hint(m.find(10), 10, 1);
    EXPECT_EQ(it->second, 10);
    EXPECT_EQ(m.size(), 2001u);
    EXPECT_TRUE(std::ranges::is_sorted(m));
    EXPECT_TRUE(tree_is_valid(m));
    // the hint right after the key's place
    sgcl::sorted_map<int, int> g = {{1, 1}, {3, 3}, {5, 5}};
    EXPECT_EQ(g.emplace_hint(g.find(3), 2, 2)->first, 2);
    EXPECT_EQ(g.emplace_hint(g.find(3), 4, 4)->first, 4);
    EXPECT_EQ(g.emplace_hint(g.find(5), 6, 6)->first, 6);
    EXPECT_EQ(g.emplace_hint(g.find(1), 0, 0)->first, 0);
    EXPECT_EQ(keys_of(g), (std::vector<int>{0, 1, 2, 3, 4, 5, 6}));
    EXPECT_TRUE(tree_is_valid(g));
}

TEST(SortedMap_Test, ThrowingComparatorLeavesContainerUnchanged) {
    sgcl::sorted_map<int, Int, ThrowingLess> m;
    std::map<int, int> oracle;
    for (int i = 0; i < 64; ++i) {
        m.emplace(i * 2, i);
        oracle.emplace(i * 2, i);
    }
    auto check = [&] {
        EXPECT_TRUE(tree_is_valid(m));
        EXPECT_EQ(m.size(), oracle.size());
        EXPECT_EQ(Int::counter, oracle.size());
        auto it = m.begin();
        for (auto& [k, v] : oracle) {
            EXPECT_EQ(it->first, k);
            EXPECT_EQ(it->second, v);
            ++it;
        }
        EXPECT_EQ(it, m.end());
    };
    for (int at : {0, 1, 3, 5}) {
        ThrowingLess::countdown = at;
        EXPECT_THROW(m.insert({33, 1}), std::runtime_error);
        ThrowingLess::countdown = -1;
        check();
        ThrowingLess::countdown = at;
        EXPECT_THROW(m.emplace(35, 1), std::runtime_error);
        ThrowingLess::countdown = -1;
        check();
        ThrowingLess::countdown = at;
        EXPECT_THROW(m.try_emplace(37, 1), std::runtime_error);
        ThrowingLess::countdown = -1;
        check();
        ThrowingLess::countdown = at;
        EXPECT_THROW(m[39], std::runtime_error);
        ThrowingLess::countdown = -1;
        check();
        ThrowingLess::countdown = at;
        EXPECT_THROW(m.emplace_hint(m.find(40), 39, 1), std::runtime_error);
        ThrowingLess::countdown = -1;
        check();
        ThrowingLess::countdown = at;
        EXPECT_THROW(m.find(41), std::runtime_error);
        ThrowingLess::countdown = -1;
        check();
    }
    // a throw in the middle of a range insert leaves a valid prefix: the
    // keys go past the maximum, so each costs the one comparison of an
    // append, and the fourth throws
    std::vector<std::pair<int, int>> more = {{201, 1}, {203, 3}, {205, 5}, {207, 7}};
    ThrowingLess::countdown = 3;
    EXPECT_THROW(m.insert(more.begin(), more.end()), std::runtime_error);
    ThrowingLess::countdown = -1;
    EXPECT_TRUE(tree_is_valid(m));
    EXPECT_EQ(m.size(), oracle.size() + 3);
    EXPECT_TRUE(std::ranges::is_sorted(m | std::views::keys));
    EXPECT_EQ(Int::counter, m.size());
    m.insert(more.begin(), more.end());
    EXPECT_EQ(m.size(), oracle.size() + more.size());
    EXPECT_TRUE(tree_is_valid(m));
}

TEST(SortedMap_Test, MappedTrackedPointer) {
    sgcl::sorted_map<int, Holder> m;
    off_frame([&] {
        for (int i = 1; i <= 3; ++i) {
            m[i].ptr = make_tracked<Baz>(i * 11);
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    collector::force_collect(true);
    off_frame([&] {
        EXPECT_EQ(m[1].ptr->value, 11);
        EXPECT_EQ(m[2].ptr->value, 22);
        EXPECT_EQ(m[3].ptr->value, 33);
        m.erase(2);
    });
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    auto nh = m.extract(3);
    off_frame([&] {
        EXPECT_EQ(nh.mapped().ptr->value, 33);
    });
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    nh = sgcl::sorted_map<int, Holder>::node_type();
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    m.clear();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
}

TEST(SortedMap_Test, ContainerInsideManagedObject) {
    off_frame([] {
        tracked_ptr<Owner> owner = make_tracked<Owner>();
        off_frame([&] {   // the iterators emplace returns are raw: none may stay in this frame
            for (int i = 0; i < 50; ++i) {
                owner->values.emplace(i, i);
            }
        });
        EXPECT_EQ(Int::counter, 50u);
        EXPECT_EQ(collector::get_live_object_count(), 52u);
        tracked_ptr<Owner> copy = make_tracked<Owner>(*owner);
        EXPECT_EQ(Int::counter, 100u);
        EXPECT_EQ(copy->values, owner->values);
        owner = nullptr;
        EXPECT_EQ(collector::get_live_object_count(), 52u);
        EXPECT_EQ(Int::counter, 50u);
        EXPECT_TRUE(tree_is_valid(copy->values));
    });
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
}

TEST(SortedMap_Test, StressAgainstStdMap) {
    sgcl::sorted_map<int, int> m;
    std::map<int, int> oracle;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> key(0, 4095);
    std::uniform_int_distribution<int> op(0, 99);
    constexpr int operations = 200000;
    // In its own frame: the references the loop leaves behind would
    // retain nodes through the final counts.
    off_frame([&] {
    for (int i = 1; i <= operations; ++i) {
        int k = key(rng);
        int o = op(rng);
        if (o < 40) {
            auto r = m.emplace(k, i);
            auto e = oracle.emplace(k, i);
            ASSERT_EQ(r.second, e.second);
            ASSERT_EQ(r.first->second, e.first->second);
        } else if (o < 50) {
            auto it = m.insert_or_assign(k, -i).first;
            oracle.insert_or_assign(k, -i);
            ASSERT_EQ(it->second, -i);
        } else if (o < 75) {
            ASSERT_EQ(m.erase(k), oracle.erase(k));
        } else if (o < 80) {
            auto it = m.find(k);
            auto oit = oracle.find(k);
            if (oit != oracle.end()) {
                ASSERT_NE(it, m.end());
                auto next = m.erase(it);
                auto onext = oracle.erase(oit);
                ASSERT_EQ(next == m.end(), onext == oracle.end());
                if (next != m.end()) {
                    ASSERT_EQ(next->first, onext->first);
                }
            } else {
                ASSERT_EQ(it, m.end());
            }
        } else if (o < 90) {
            auto it = m.find(k);
            auto oit = oracle.find(k);
            ASSERT_EQ(it == m.end(), oit == oracle.end());
            if (oit != oracle.end()) {
                ASSERT_EQ(it->second, oit->second);
            }
            ASSERT_EQ(m.contains(k), oracle.contains(k));
        } else {
            auto lb = m.lower_bound(k);
            auto olb = oracle.lower_bound(k);
            ASSERT_EQ(lb == m.end(), olb == oracle.end());
            if (olb != oracle.end()) {
                ASSERT_EQ(lb->first, olb->first);
            }
            auto ub = m.upper_bound(k);
            auto oub = oracle.upper_bound(k);
            ASSERT_EQ(ub == m.end(), oub == oracle.end());
            if (oub != oracle.end()) {
                ASSERT_EQ(ub->first, oub->first);
            }
        }
        ASSERT_EQ(m.size(), oracle.size());
        if (i % 10000 == 0) {
            collector::force_collect();
        }
        if (i % 5000 == 0) {
            ASSERT_TRUE(tree_is_valid(m));
            ASSERT_TRUE(std::equal(m.begin(), m.end(), oracle.begin(), oracle.end()));
            ASSERT_TRUE(std::equal(m.rbegin(), m.rend(), oracle.rbegin(), oracle.rend()));
        }
    }
    });
    collector::force_collect(true);
    EXPECT_TRUE(tree_is_valid(m));
    off_frame([&] {   // raw iterators: none may stay in this frame through clear()
        EXPECT_TRUE(std::equal(m.begin(), m.end(), oracle.begin(), oracle.end()));
    });
    EXPECT_EQ(collector::get_live_object_count(), m.size() + 1);
    m.clear();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
}

TEST(SortedMap_Test, DeductionGuides) {
    sgcl::sorted_map m = {std::pair{1, 2.0}, std::pair{2, 3.0}};
    static_assert(std::is_same_v<decltype(m), sgcl::sorted_map<int, double>>);
    sgcl::sorted_map from_range(m.begin(), m.end());
    static_assert(std::is_same_v<decltype(from_range), sgcl::sorted_map<int, double>>);
    sgcl::sorted_map greater({std::pair{1, 2}}, std::greater<int>());
    static_assert(std::is_same_v<decltype(greater), sgcl::sorted_map<int, int, std::greater<int>>>);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(from_range.at(2), 3.0);
}

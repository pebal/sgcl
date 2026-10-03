//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"
#include "tests/containers/boundary.h"

#include "sgcl/core/map.h"
#include "sgcl/core/multimap.h"
#include "sgcl/core/multiset.h"
#include "sgcl/core/ordered_map.h"
#include "sgcl/core/ordered_set.h"
#include "sgcl/core/set.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
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

    // A key whose constructions are counted: a transparent lookup with an
    // int must not construct one.
    struct CountedKey {
        int value;
        inline static int constructions = 0;

        CountedKey(int v)
        : value(v) {
            ++constructions;
        }

        bool operator==(const CountedKey& other) const noexcept {
            return value == other.value;
        }
    };

    struct CountedKeyHash {
        using is_transparent = void;

        size_t operator()(int v) const noexcept {
            return std::hash<int>{}(v);
        }

        size_t operator()(const CountedKey& k) const noexcept {
            return std::hash<int>{}(k.value);
        }
    };

    struct CountedKeyEqual {
        using is_transparent = void;

        bool operator()(int a, int b) const noexcept {
            return a == b;
        }

        bool operator()(const CountedKey& a, int b) const noexcept {
            return a.value == b;
        }

        bool operator()(int a, const CountedKey& b) const noexcept {
            return a == b.value;
        }

        bool operator()(const CountedKey& a, const CountedKey& b) const noexcept {
            return a.value == b.value;
        }
    };

    struct OpaqueKeyHash {
        size_t operator()(const CountedKey& k) const noexcept {
            return std::hash<int>{}(k.value);
        }
    };

    struct NonMovable {
        int value;

        explicit NonMovable(int v)
        : value(v) {
        }

        NonMovable() = delete;
        NonMovable(const NonMovable&) = delete;
        NonMovable(NonMovable&&) = delete;
        NonMovable& operator=(const NonMovable&) = delete;
    };

    struct Throwing {
        int value;

        explicit Throwing(int v)
        : value(v) {
            if (v < 0) {
                throw std::runtime_error("Throwing");
            }
        }
    };

    struct SeededHash {
        size_t seed = 0;

        size_t operator()(int v) const noexcept {
            return std::hash<int>{}(v) ^ seed;
        }
    };

    struct SeededEqual {
        int tag = 0;

        bool operator()(int a, int b) const noexcept {
            return a == b;
        }
    };

    template<class Map>
    concept FindsStringView = requires(Map& m, std::string_view sv) {
        m.find(sv);
    };

    template<class Map>
    concept FindsInt = requires(Map& m) {
        m.find(5);
    };

    struct Holder {
        sgcl::map<int, tracked_ptr<Baz>> map;
    };

    template<class Map>
    size_t sum_of_bucket_sizes(const Map& map) {
        size_t sum = 0;
        for (size_t n = 0; n < map.bucket_count(); ++n) {
            sum += map.bucket_size(n);
        }
        return sum;
    }
}

TEST(Map_Test, DefaultConstructorEmpty) {
    sgcl::map<int, Int> map;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
    EXPECT_EQ(map.bucket_count(), 0u);
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(map.cbegin(), map.cend());
    EXPECT_EQ(map.find(1), map.end());
    EXPECT_EQ(map.count(1), 0u);
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(map.erase(1), 0u);
    EXPECT_TRUE(map.extract(1).empty());
    auto [first, last] = map.equal_range(1);
    EXPECT_EQ(first, last);
    EXPECT_EQ(map.load_factor(), 0.0f);
    EXPECT_EQ(map.max_load_factor(), 1.0f);
    EXPECT_GT(map.max_size(), 0u);
    EXPECT_GT(map.max_bucket_count(), 0u);
    EXPECT_EQ(map.bucket_size(0), 0u);
    EXPECT_EQ(map.begin(0), map.end(0));
    EXPECT_THROW(map.at(1), std::out_of_range);
    EXPECT_EQ(std::ranges::distance(map), 0);
}

TEST(Map_Test, BucketCountConstructor) {
    sgcl::map<int, int> map(10);
    EXPECT_EQ(collector::get_live_object_count(), 2u);   // the bucket array and the sentinel node
    EXPECT_EQ(map.bucket_count(), 16u);
    EXPECT_TRUE(map.empty());
    sgcl::map<int, int, SeededHash, SeededEqual> seeded(4, SeededHash{7}, SeededEqual{3});
    EXPECT_EQ(seeded.bucket_count(), 4u);
    EXPECT_EQ(seeded.hash_function().seed, 7u);
    EXPECT_EQ(seeded.key_eq().tag, 3);
    sgcl::map<int, int> zero(0);
    EXPECT_EQ(zero.bucket_count(), 0u);
}

TEST(Map_Test, RangeAndInitializerListConstructors) {
    std::vector<std::pair<const std::string, int>> source = {{"a", 1}, {"b", 2}, {"c", 3}, {"a", 4}};
    sgcl::map<std::string, int> map(source.begin(), source.end());
    EXPECT_EQ(map.size(), 3u);
    EXPECT_EQ(map.at("a"), 1);
    EXPECT_EQ(map.at("b"), 2);
    EXPECT_EQ(map.at("c"), 3);
    EXPECT_GE(map.bucket_count(), 4u);

    sgcl::map<std::string, int> list = {{"x", 10}, {"y", 20}, {"x", 30}};
    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list.at("x"), 10);
    EXPECT_EQ(list.at("y"), 20);

    sgcl::map<int, int, SeededHash> seeded({{1, 1}}, 32, SeededHash{5});
    EXPECT_EQ(seeded.bucket_count(), 32u);
    EXPECT_EQ(seeded.hash_function().seed, 5u);
    EXPECT_EQ(seeded.at(1), 1);

    sgcl::map deduced(source.begin(), source.end());
    static_assert(std::is_same_v<decltype(deduced), sgcl::map<std::string, int>>);
    EXPECT_EQ(deduced.size(), 3u);
}

TEST(Map_Test, CopyConstructor) {
    sgcl::map<int, Int, SeededHash> other({{1, 10}, {2, 20}, {3, 30}}, 0, SeededHash{9});
    other.max_load_factor(0.5f);
    sgcl::map<int, Int, SeededHash> map(other);
    EXPECT_EQ(collector::get_live_object_count(), 10u);   // 2 x (3 nodes, the buckets, the sentinel)
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(map.size(), 3u);
    EXPECT_EQ(map.bucket_count(), other.bucket_count());
    EXPECT_EQ(map.max_load_factor(), 0.5f);
    EXPECT_EQ(map.hash_function().seed, 9u);
    EXPECT_EQ(map.at(1), 10);
    EXPECT_EQ(map.at(2), 20);
    EXPECT_EQ(map.at(3), 30);
    EXPECT_TRUE(map == other);
    map.at(1) = 11;
    EXPECT_EQ(other.at(1), 10);
    EXPECT_FALSE(map == other);
    std::vector<int> a, b;
    for (auto& [k, v] : map) {
        a.push_back(k);
    }
    for (auto& [k, v] : other) {
        b.push_back(k);
    }
    EXPECT_EQ(a, b);   // the chain is copied in order
}

TEST(Map_Test, MoveConstructor) {
    sgcl::map<int, Int> other = {{1, 10}, {2, 20}, {3, 30}};
    auto it = other.find(2);
    sgcl::map<int, Int> map(std::move(other));
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.size(), 0u);
    EXPECT_EQ(other.bucket_count(), 0u);
    EXPECT_EQ(other.begin(), other.end());
    EXPECT_EQ(map.size(), 3u);
    EXPECT_EQ(map.at(2), 20);
    EXPECT_EQ(it->second, 20);   // iterators follow the elements
    EXPECT_EQ(std::distance(map.begin(), map.end()), 3);
    other[4] = 40;   // the moved-from map is usable
    EXPECT_EQ(other.size(), 1u);
}

TEST(Map_Test, CopyAssignment) {
    sgcl::map<int, Int> other = {{1, 10}, {2, 20}};
    sgcl::map<int, Int> map = {{5, 50}, {6, 60}, {7, 70}};
    map = other;
    EXPECT_EQ(Int::counter, 4u);   // the three old elements died at once
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map.at(1), 10);
    EXPECT_EQ(map.at(2), 20);
    EXPECT_FALSE(map.contains(5));
    EXPECT_TRUE(map == other);
    map = map;
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(Int::counter, 4u);
}

TEST(Map_Test, MoveAssignment) {
    sgcl::map<int, Int> other = {{1, 10}, {2, 20}};
    sgcl::map<int, Int> map = {{5, 50}, {6, 60}, {7, 70}};
    map = std::move(other);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map.at(1), 10);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.bucket_count(), 0u);
}

TEST(Map_Test, InitializerListAssignment) {
    sgcl::map<int, Int, SeededHash> map({{5, 50}}, 0, SeededHash{4});
    map.max_load_factor(0.75f);
    map = {{1, 10}, {2, 20}};
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map.at(1), 10);
    EXPECT_EQ(map.at(2), 20);
    EXPECT_FALSE(map.contains(5));
    EXPECT_EQ(map.hash_function().seed, 4u);
    EXPECT_EQ(map.max_load_factor(), 0.75f);
}

TEST(Map_Test, InsertVariants) {
    sgcl::map<std::string, int> map;
    std::pair<const std::string, int> value("a", 1);
    auto [it, inserted] = map.insert(value);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, "a");
    EXPECT_EQ(it->second, 1);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // the node, the buckets, the sentinel

    auto [again, inserted_again] = map.insert(value);
    EXPECT_FALSE(inserted_again);
    EXPECT_EQ(again, it);
    EXPECT_EQ(map.size(), 1u);

    auto [rit, rinserted] = map.insert(std::pair<const std::string, int>("b", 2));
    EXPECT_TRUE(rinserted);
    EXPECT_EQ(rit->second, 2);

    auto [pit, pinserted] = map.insert(std::pair<std::string, int>("c", 3));   // insert(P&&)
    EXPECT_TRUE(pinserted);
    EXPECT_EQ(pit->second, 3);

    auto [cit, cinserted] = map.insert(std::pair<const char*, int>("d", 4));
    EXPECT_TRUE(cinserted);
    EXPECT_EQ(cit->second, 4);

    auto hinted = map.insert(map.cbegin(), std::pair<const std::string, int>("e", 5));
    EXPECT_EQ(hinted->second, 5);
    auto hinted2 = map.insert(map.cbegin(), std::pair<std::string, int>("f", 6));
    EXPECT_EQ(hinted2->second, 6);
    auto hinted3 = map.insert(map.cbegin(), value);
    EXPECT_EQ(hinted3, it);

    std::vector<std::pair<const std::string, int>> more = {{"g", 7}, {"a", 100}, {"h", 8}};
    map.insert(more.begin(), more.end());
    EXPECT_EQ(map.size(), 8u);
    EXPECT_EQ(map.at("a"), 1);
    EXPECT_EQ(map.at("g"), 7);

    map.insert({{"i", 9}, {"j", 10}, {"b", 200}});
    EXPECT_EQ(map.size(), 10u);
    EXPECT_EQ(map.at("b"), 2);
    EXPECT_EQ(map.at("j"), 10);

    std::set<std::string> keys;
    for (const auto& [k, v] : map) {
        keys.insert(k);
    }
    EXPECT_EQ(keys.size(), 10u);
}

TEST(Map_Test, Emplace) {
    sgcl::map<std::string, Int> map;
    auto [it, inserted] = map.emplace("a", 1);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->second, 1);
    EXPECT_EQ(Int::counter, 1u);

    auto [dup, dup_inserted] = map.emplace("a", 2);
    EXPECT_FALSE(dup_inserted);
    EXPECT_EQ(dup, it);
    EXPECT_EQ(dup->second, 1);
    EXPECT_EQ(Int::counter, 1u);   // the element built for the duplicate died at once
    EXPECT_EQ(map.size(), 1u);

    auto [pw, pw_inserted] = map.emplace(std::piecewise_construct, std::forward_as_tuple("b"), std::forward_as_tuple(2));
    EXPECT_TRUE(pw_inserted);
    EXPECT_EQ(pw->second, 2);

    auto [vt, vt_inserted] = map.emplace(std::pair<const std::string, Int>("c", 3));
    EXPECT_TRUE(vt_inserted);
    EXPECT_EQ(vt->second, 3);

    auto hint = map.emplace_hint(map.cend(), "d", 4);
    EXPECT_EQ(hint->second, 4);
    auto hint_dup = map.emplace_hint(map.cbegin(), "d", 5);
    EXPECT_EQ(hint_dup, hint);
    EXPECT_EQ(map.size(), 4u);
    EXPECT_EQ(Int::counter, 4u);
}

TEST(Map_Test, TryEmplaceConstructsInPlace) {
    sgcl::map<std::string, NonMovable> map;
    auto [it, inserted] = map.try_emplace("a", 1);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->second.value, 1);
    auto [dup, dup_inserted] = map.try_emplace("a", 2);
    EXPECT_FALSE(dup_inserted);
    EXPECT_EQ(dup, it);
    EXPECT_EQ(dup->second.value, 1);
    std::string key = "b";
    auto [bit, binserted] = map.try_emplace(std::move(key), 2);
    EXPECT_TRUE(binserted);
    EXPECT_EQ(bit->second.value, 2);
    auto hint = map.try_emplace(map.cbegin(), "c", 3);
    EXPECT_EQ(hint->second.value, 3);
    auto hint_dup = map.try_emplace(map.cbegin(), std::string("c"), 4);
    EXPECT_EQ(hint_dup, hint);
    EXPECT_EQ(map.size(), 3u);

    sgcl::map<int, std::unique_ptr<int>> owners;
    auto [uit, uinserted] = owners.try_emplace(1, std::make_unique<int>(10));
    EXPECT_TRUE(uinserted);
    EXPECT_EQ(*uit->second, 10);
    auto keep = std::make_unique<int>(20);
    auto [kit, kinserted] = owners.try_emplace(1, std::move(keep));
    EXPECT_FALSE(kinserted);
    EXPECT_NE(keep, nullptr);   // not moved from when the key exists
    EXPECT_EQ(*kit->second, 10);
    owners[2] = std::make_unique<int>(30);
    EXPECT_EQ(*owners[2], 30);
    EXPECT_EQ(owners[3], nullptr);
    EXPECT_EQ(owners.size(), 3u);
}

TEST(Map_Test, InsertOrAssign) {
    sgcl::map<std::string, Int> map;
    auto [it, inserted] = map.insert_or_assign("a", 1);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->second, 1);
    auto [ait, assigned] = map.insert_or_assign("a", 2);
    EXPECT_FALSE(assigned);
    EXPECT_EQ(ait, it);
    EXPECT_EQ(map.at("a"), 2);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(Int::counter, 1u);
    std::string key = "b";
    auto [bit, binserted] = map.insert_or_assign(std::move(key), 3);
    EXPECT_TRUE(binserted);
    EXPECT_EQ(bit->second, 3);
    auto hint = map.insert_or_assign(map.cbegin(), "b", 4);
    EXPECT_EQ(hint, bit);
    EXPECT_EQ(map.at("b"), 4);
    auto hint2 = map.insert_or_assign(map.cbegin(), std::string("c"), 5);
    EXPECT_EQ(hint2->second, 5);
    EXPECT_EQ(map.size(), 3u);
}

TEST(Map_Test, SubscriptAndAt) {
    sgcl::map<std::string, Int> map;
    map["x"] = 42;
    EXPECT_EQ(map["x"], 42);
    EXPECT_EQ(map.size(), 1u);
    map["x"] = 100;
    EXPECT_EQ(map["x"], 100);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map["y"], 0);   // constructed in place
    EXPECT_EQ(map.size(), 2u);
    std::string key = "z";
    map[std::move(key)] = 7;
    EXPECT_EQ(map.at("z"), 7);
    EXPECT_EQ(Int::counter, 3u);

    EXPECT_EQ(map.at("x"), 100);
    map.at("x") = 5;
    EXPECT_EQ(map.at("x"), 5);
    EXPECT_THROW(map.at("missing"), std::out_of_range);
    const auto& cmap = map;
    EXPECT_EQ(cmap.at("z"), 7);
    EXPECT_THROW(cmap.at("missing"), std::out_of_range);
    EXPECT_EQ(map.size(), 3u);
}

TEST(Map_Test, Lookup) {
    sgcl::map<int, int> map = {{1, 10}, {2, 20}, {3, 30}};
    EXPECT_NE(map.find(2), map.end());
    EXPECT_EQ(map.find(2)->second, 20);
    EXPECT_EQ(map.find(4), map.end());
    EXPECT_EQ(map.count(1), 1u);
    EXPECT_EQ(map.count(4), 0u);
    EXPECT_TRUE(map.contains(3));
    EXPECT_FALSE(map.contains(0));
    auto [first, last] = map.equal_range(1);
    ASSERT_NE(first, last);
    EXPECT_EQ(first->second, 10);
    EXPECT_EQ(std::distance(first, last), 1);
    auto [none, none_end] = map.equal_range(4);
    EXPECT_EQ(none, none_end);
    EXPECT_EQ(none, map.end());
    const auto& cmap = map;
    EXPECT_EQ(cmap.find(3)->second, 30);
    auto [cfirst, clast] = cmap.equal_range(3);
    EXPECT_EQ(std::distance(cfirst, clast), 1);
    static_assert(std::is_same_v<decltype(cfirst), sgcl::map<int, int>::const_iterator>);
}

TEST(Map_Test, EraseByIterator) {
    sgcl::map<int, Int> map = {{1, 10}, {2, 20}, {3, 30}, {4, 40}};
    off_frame([&] {   // a raw iterator left in a frame would keep the erased node's memory
        auto it = map.find(2);
        auto next = std::next(it);
        auto result = map.erase(it);
        EXPECT_EQ(result, next);
        EXPECT_EQ(map.size(), 3u);
        EXPECT_EQ(Int::counter, 3u);   // destroyed at once
        EXPECT_FALSE(map.contains(2));
        sgcl::map<int, Int>::const_iterator cit = map.find(3);
        map.erase(cit);
        EXPECT_EQ(map.size(), 2u);
        EXPECT_EQ(Int::counter, 2u);
    });
    EXPECT_EQ(collector::get_live_object_count(), 4u);   // 2 nodes, the buckets, the sentinel: the erased nodes are gone with the frame
    EXPECT_EQ(map.erase(map.end()), map.end());
    EXPECT_EQ(map.size(), 2u);
    while (!map.empty()) {
        map.erase(map.begin());
    }
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(map.begin(), map.end());
}

TEST(Map_Test, EraseRange) {
    sgcl::map<int, Int> map;
    for (int i = 0; i < 20; ++i) {
        map.emplace(i, i);
    }
    auto first = std::next(map.begin(), 5);
    auto last = std::next(first, 7);
    std::vector<int> kept;
    for (auto it = map.begin(); it != first; ++it) {
        kept.push_back(it->first);
    }
    for (auto it = last; it != map.end(); ++it) {
        kept.push_back(it->first);
    }
    auto result = map.erase(first, last);
    EXPECT_EQ(result, last);
    EXPECT_EQ(map.size(), 13u);
    EXPECT_EQ(Int::counter, 13u);
    for (int k : kept) {
        EXPECT_TRUE(map.contains(k));
    }
    EXPECT_EQ(map.erase(map.begin(), map.begin()), map.begin());
    EXPECT_EQ(map.size(), 13u);
    EXPECT_EQ(map.erase(map.begin(), map.end()), map.end());
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(Int::counter, 0u);
}

TEST(Map_Test, EraseByKey) {
    sgcl::map<std::string, Int> map = {{"one", 1}, {"two", 2}};
    EXPECT_EQ(map.erase("one"), 1u);
    EXPECT_EQ(Int::counter, 1u);
    EXPECT_FALSE(map.contains("one"));
    EXPECT_EQ(map.erase("not-there"), 0u);
    EXPECT_EQ(map.erase("one"), 0u);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.at("two"), 2);
}

TEST(Map_Test, EraseWhileIterating) {
    sgcl::map<int, int> map;
    for (int i = 0; i < 100; ++i) {
        map.emplace(i, i);
    }
    size_t visited = 0;
    for (auto it = map.begin(); it != map.end(); ++visited) {
        if (it->first % 3 == 0) {
            it = map.erase(it);
        } else {
            ++it;
        }
    }
    EXPECT_EQ(visited, 100u);
    EXPECT_EQ(map.size(), 66u);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(map.contains(i), i % 3 != 0);
    }
}

TEST(Map_Test, Clear) {
    sgcl::map<int, Int, SeededHash, SeededEqual> map({{1, 1}, {2, 2}, {3, 3}}, 0, SeededHash{2}, SeededEqual{5});
    map.max_load_factor(0.5f);
    auto buckets = map.bucket_count();
    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(map.bucket_count(), buckets);
    EXPECT_EQ(map.hash_function().seed, 2u);
    EXPECT_EQ(map.key_eq().tag, 5);
    EXPECT_EQ(map.max_load_factor(), 0.5f);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(sum_of_bucket_sizes(map), 0u);
    map.emplace(4, 4);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.at(4), 4);
    map.clear();
    map.clear();
    EXPECT_TRUE(map.empty());
}

TEST(Map_Test, Destructor) {
    {
        sgcl::map<int, Int> map = {{1, 1}, {2, 2}};
        EXPECT_EQ(Int::counter, 2u);
    }
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(Map_Test, Swap) {
    sgcl::map<int, int, SeededHash> a({{1, 10}}, 0, SeededHash{1});
    sgcl::map<int, int, SeededHash> b({{2, 20}, {3, 30}}, 64, SeededHash{2});
    a.max_load_factor(0.5f);
    auto it = a.find(1);
    a.swap(b);
    EXPECT_EQ(a.size(), 2u);
    EXPECT_EQ(b.size(), 1u);
    EXPECT_TRUE(a.contains(2));
    EXPECT_TRUE(b.contains(1));
    EXPECT_EQ(a.bucket_count(), 64u);
    EXPECT_EQ(a.hash_function().seed, 2u);
    EXPECT_EQ(b.hash_function().seed, 1u);
    EXPECT_EQ(b.max_load_factor(), 0.5f);
    EXPECT_EQ(it->second, 10);   // still valid: it iterates b now
    EXPECT_EQ(++it, b.end());
    swap(a, b);
    EXPECT_TRUE(a.contains(1));
    EXPECT_TRUE(b.contains(3));
    std::swap(a, b);
    EXPECT_TRUE(a.contains(3));
}

TEST(Map_Test, IteratorsSurviveErasingOthers) {
    sgcl::map<int, int> map;
    for (int i = 0; i < 50; ++i) {
        map.emplace(i, i * 10);
    }
    auto it7 = map.find(7);
    auto it30 = map.find(30);
    for (int i = 0; i < 50; ++i) {
        if (i != 7 && i != 30) {
            map.erase(i);
        }
    }
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(it7->second, 70);
    EXPECT_EQ(it30->second, 300);
    size_t seen = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        ++seen;
        EXPECT_TRUE(it == it7 || it == it30);
    }
    EXPECT_EQ(seen, 2u);
    map.erase(it7);
    EXPECT_EQ(map.begin(), it30);
    EXPECT_EQ(std::next(it30), map.end());
}

TEST(Map_Test, IteratorConcept) {
    using Map = sgcl::map<int, int>;
    static_assert(std::forward_iterator<Map::iterator>);
    static_assert(std::forward_iterator<Map::const_iterator>);
    static_assert(std::forward_iterator<Map::local_iterator>);
    static_assert(std::forward_iterator<Map::const_local_iterator>);
    static_assert(std::ranges::forward_range<Map>);
    static_assert(std::ranges::forward_range<const Map>);
    static_assert(std::is_same_v<Map::iterator::reference, std::pair<const int, int>&>);
    static_assert(std::is_same_v<Map::const_iterator::reference, const std::pair<const int, int>&>);
    static_assert(std::is_same_v<Map::iterator::value_type, std::pair<const int, int>>);
    static_assert(std::is_convertible_v<Map::iterator, Map::const_iterator>);
    static_assert(!std::is_convertible_v<Map::const_iterator, Map::iterator>);
    // an iterator is one raw node pointer: it may live anywhere
    static_assert(std::is_trivially_copyable_v<Map::iterator>);
    static_assert(std::is_trivially_copyable_v<Map::const_iterator>);
    static_assert(std::is_trivially_copyable_v<Map::local_iterator>);
    static_assert(std::is_trivially_copyable_v<Map::const_local_iterator>);
    static_assert(std::is_nothrow_default_constructible_v<Map::iterator>);
    static_assert(std::is_nothrow_default_constructible_v<Map::local_iterator>);
    static_assert(sizeof(Map::iterator) == sizeof(void*));

    Map map = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};
    EXPECT_EQ(std::ranges::count_if(map, [](const auto& p) { return p.second % 2 == 0; }), 2);
    EXPECT_EQ(std::ranges::distance(map), 4);
    Map::iterator it = map.begin();
    Map::const_iterator cit = it;
    EXPECT_TRUE(it == cit);
    EXPECT_TRUE(cit == it);
    auto old = it++;
    EXPECT_EQ(old, cit);
    EXPECT_EQ(it, std::next(cit));
    Map::iterator empty1, empty2;
    EXPECT_EQ(empty1, empty2);
    EXPECT_EQ(empty1, map.end());
    const Map& cmap = map;
    EXPECT_EQ(std::distance(cmap.begin(), cmap.end()), 4);
    int sum = 0;
    for (auto& [k, v] : cmap) {
        sum += v;
    }
    EXPECT_EQ(sum, 10);
    for (auto& [k, v] : map) {
        v *= 2;
    }
    EXPECT_EQ(map.at(4), 8);
}

// Iterators are raw pointers: a std::vector of them is legal, and each one
// stays valid across a collection while its element is in the map.
TEST(Map_Test, IteratorsInUnmanagedMemory) {
    using Map = sgcl::map<int, int>;
    Map map;
    for (int i = 0; i < 100; ++i) {
        map.emplace(i, i * 2);
    }
    std::vector<Map::iterator> its;
    std::vector<Map::const_iterator> cits;
    std::vector<Map::local_iterator> lits;
    for (int i = 0; i < 100; ++i) {
        its.push_back(map.find(i));
        cits.push_back(std::as_const(map).find(i));
        lits.push_back(map.begin(map.bucket(i)));
    }
    auto* heap_it = new Map::iterator(map.find(50));
    collector::force_collect(true);
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(its[i]->first, i);
        ASSERT_EQ(its[i]->second, i * 2);
        ASSERT_EQ(cits[i], its[i]);
        ASSERT_EQ(map.bucket(lits[i]->first), map.bucket(i));
    }
    EXPECT_EQ((*heap_it)->second, 100);
    delete heap_it;
    map.rehash(1024);
    collector::force_collect(true);
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(its[i], map.find(i));
        ASSERT_EQ(its[i]->second, i * 2);
    }
    std::vector<Map::iterator> copy = its;
    EXPECT_TRUE(std::equal(copy.begin(), copy.end(), its.begin()));
}

TEST(Map_Test, RawIteratorAcrossCollection) {
    sgcl::map<int, tracked_ptr<Baz>> map;
    off_frame([&] {
        for (int i = 0; i < 50; ++i) {
            map[i] = make_tracked<Baz>(i);
        }
    });
    // The iterators live in this frame only: after it returns, no stale
    // word keeps the node erased at its end.
    off_frame([&] {
        auto it = map.find(7);
        auto cit = std::as_const(map).find(8);
        auto lit = map.begin(map.bucket(9));
        auto [first, last] = map.equal_range(10);
        ASSERT_NE(it, map.end());
        ASSERT_NE(first, last);
        off_frame([&] {
            for (int i = 50; i < 200; ++i) {   // grows the table: the nodes are relinked
                map[i] = make_tracked<Baz>(i);
            }
            map.erase(6);
            map.erase(11);
        });
        collector::force_collect(true);
        collector::force_collect(true);
        EXPECT_EQ(it->first, 7);
        EXPECT_EQ(it->second->value, 7);
        EXPECT_EQ(it, map.find(7));
        EXPECT_EQ(cit->second->value, 8);
        EXPECT_EQ(lit->second->value, lit->first);
        EXPECT_EQ(first->second->value, 10);
        EXPECT_EQ(std::next(first), last);
        auto next = map.erase(it);
        EXPECT_EQ(map.count(7), 0u);
        EXPECT_TRUE(next == map.end() || map.contains(next->first));
    });
    collector::force_collect(true);
    EXPECT_EQ(map.size(), 197u);
    EXPECT_EQ(collector::get_live_object_count(), 2 * 197 + 2);   // the nodes, the Baz, the buckets, the sentinel
}

TEST(Map_Test, NodeHandles) {
    sgcl::map<std::string, Int> map = {{"x", 10}, {"y", 20}};
    auto node = map.extract("x");
    ASSERT_FALSE(node.empty());
    EXPECT_TRUE(node);
    EXPECT_EQ(node.key(), "x");
    EXPECT_EQ(node.mapped(), 10);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_FALSE(map.contains("x"));
    EXPECT_EQ(Int::counter, 2u);   // the element lives in the handle
    static_assert(!std::is_copy_constructible_v<decltype(node)>);

    node.key() = "w";
    node.mapped() = 15;
    sgcl::map<std::string, Int> empty;
    EXPECT_EQ(empty.bucket_count(), 0u);
    auto result = empty.insert(std::move(node));
    EXPECT_TRUE(result.inserted);
    EXPECT_TRUE(result.node.empty());
    EXPECT_EQ(result.position->first, "w");
    EXPECT_EQ(result.position->second, 15);
    EXPECT_TRUE(node.empty());
    EXPECT_FALSE(node);
    EXPECT_EQ(empty.size(), 1u);
    EXPECT_EQ(empty.at("w"), 15);

    auto again = empty.insert(std::move(node));   // the handle is empty now
    EXPECT_FALSE(again.inserted);
    EXPECT_EQ(again.position, empty.end());
    EXPECT_TRUE(again.node.empty());
    EXPECT_EQ(empty.size(), 1u);

    auto y = map.extract(map.find("y"));
    EXPECT_EQ(y.key(), "y");
    EXPECT_TRUE(map.empty());
    empty.emplace("y", 99);
    auto blocked = empty.insert(std::move(y));   // the key exists: the handle comes back
    EXPECT_FALSE(blocked.inserted);
    EXPECT_EQ(blocked.position->second, 99);
    ASSERT_FALSE(blocked.node.empty());
    EXPECT_EQ(blocked.node.mapped(), 20);
    EXPECT_TRUE(y.empty());
    EXPECT_EQ(Int::counter, 3u);

    auto hinted = map.insert(map.cend(), std::move(blocked.node));
    EXPECT_EQ(hinted->first, "y");
    EXPECT_EQ(map.size(), 1u);
    EXPECT_TRUE(blocked.node.empty());

    EXPECT_TRUE(map.extract("absent").empty());
    auto dropped = map.extract("y");
    EXPECT_EQ(Int::counter, 3u);
    dropped = decltype(dropped)();   // move-assigning over a full handle destroys its element
    EXPECT_EQ(Int::counter, 2u);
    {
        auto scoped = empty.extract("w");
        EXPECT_EQ(Int::counter, 2u);
    }
    EXPECT_EQ(Int::counter, 1u);
    EXPECT_EQ(empty.size(), 1u);
}

// A hinted insertion of a node whose key is taken leaves the element in the
// handle, as std does: only the iterator to the element there comes back.
TEST(Map_Test, AHintedNodeInsertOfATakenKeyKeepsTheHandle) {
    sgcl::map<std::string, Int> map = {{"x", 10}};
    sgcl::map<std::string, Int> other = {{"x", 20}};
    auto nh = other.extract("x");
    auto it = map.insert(map.cend(), std::move(nh));
    EXPECT_EQ(it->second, 10);
    ASSERT_FALSE(nh.empty());
    EXPECT_EQ(nh.key(), "x");
    EXPECT_EQ(nh.mapped(), 20);
    EXPECT_EQ(Int::counter, 2u);

    sgcl::ordered_map<std::string, Int> ordered = {{"y", 1}};
    sgcl::ordered_map<std::string, Int> source = {{"y", 2}};
    auto onh = source.extract("y");
    auto oit = ordered.insert(ordered.cend(), std::move(onh));
    EXPECT_EQ(oit->second, 1);
    ASSERT_FALSE(onh.empty());
    EXPECT_EQ(onh.mapped(), 2);

    sgcl::set<std::string> set = {"z"};
    sgcl::set<std::string> from = {"z"};
    auto snh = from.extract("z");
    EXPECT_EQ(*set.insert(set.cend(), std::move(snh)), "z");
    ASSERT_FALSE(snh.empty());
    EXPECT_EQ(snh.value(), "z");
}

TEST(Map_Test, Merge) {
    sgcl::map<int, Int> a = {{1, 1}, {2, 2}};
    sgcl::map<int, Int> b = {{2, 20}, {3, 30}, {4, 40}};
    auto it3 = b.find(3);
    const auto* address = &*it3;
    a.merge(b);
    EXPECT_EQ(a.size(), 4u);
    EXPECT_EQ(b.size(), 1u);
    EXPECT_EQ(a.at(2), 2);
    EXPECT_EQ(b.at(2), 20);
    EXPECT_EQ(a.at(3), 30);
    EXPECT_EQ(a.at(4), 40);
    EXPECT_EQ(&*a.find(3), address);   // relinked, not copied
    EXPECT_EQ(it3->second, 30);
    EXPECT_EQ(Int::counter, 5u);
    a.merge(a);
    EXPECT_EQ(a.size(), 4u);

    sgcl::multimap<int, Int> multi = {{5, 50}, {5, 51}, {1, 100}};
    a.merge(std::move(multi));
    EXPECT_EQ(a.size(), 5u);
    EXPECT_EQ(multi.size(), 2u);   // one 5 and the duplicate 1 stay
    EXPECT_EQ(multi.count(5), 1u);
    EXPECT_EQ(multi.count(1), 1u);
    EXPECT_EQ(a.at(1), 1);
    EXPECT_TRUE(a.at(5) == 50 || a.at(5) == 51);

    sgcl::map<int, Int> big;
    for (int i = 100; i < 200; ++i) {
        big.emplace(i, i);
    }
    a.merge(big);   // grows while merging
    EXPECT_EQ(a.size(), 105u);
    EXPECT_TRUE(big.empty());
    EXPECT_GE(a.bucket_count(), 105u);
    for (int i = 100; i < 200; ++i) {
        EXPECT_EQ(a.at(i), i);
    }
    EXPECT_EQ(Int::counter, 108u);
}

TEST(Map_Test, RehashAndReserve) {
    sgcl::map<int, int> map;
    for (int i = 0; i < 100; ++i) {
        map.emplace(i, i);
    }
    auto it = map.find(42);
    map.rehash(1000);
    EXPECT_EQ(map.bucket_count(), 1024u);
    EXPECT_EQ(map.size(), 100u);
    EXPECT_EQ(it->second, 42);   // valid across a rehash
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(map.contains(i));
        EXPECT_EQ(map.at(i), i);
    }
    EXPECT_EQ(std::ranges::distance(map), 100);
    EXPECT_EQ(sum_of_bucket_sizes(map), 100u);

    map.rehash(1);   // shrinks to what the elements need
    EXPECT_EQ(map.bucket_count(), 128u);
    EXPECT_EQ(sum_of_bucket_sizes(map), 100u);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(map.at(i), i);
    }

    map.reserve(5000);
    EXPECT_GE(map.bucket_count(), 5000u);
    EXPECT_EQ(map.bucket_count(), 8192u);
    auto buckets = map.bucket_count();
    for (int i = 100; i < 5000; ++i) {
        map.emplace(i, i);
    }
    EXPECT_EQ(map.bucket_count(), buckets);   // reserve() made rehashing unnecessary
    EXPECT_EQ(map.size(), 5000u);
    EXPECT_EQ(sum_of_bucket_sizes(map), 5000u);

    sgcl::map<int, int> fresh;
    fresh.rehash(0);
    EXPECT_EQ(fresh.bucket_count(), 0u);
    fresh.rehash(50);
    EXPECT_EQ(fresh.bucket_count(), 64u);
    EXPECT_TRUE(fresh.empty());
    fresh.reserve(100);
    EXPECT_EQ(fresh.bucket_count(), 128u);
    fresh.max_load_factor(0.5f);
    fresh.reserve(100);
    EXPECT_EQ(fresh.bucket_count(), 256u);
}

TEST(Map_Test, LoadFactorAndGrowth) {
    sgcl::map<int, int> map;
    EXPECT_EQ(map.max_load_factor(), 1.0f);
    for (int i = 0; i < 8; ++i) {
        map.emplace(i, i);
    }
    EXPECT_EQ(map.bucket_count(), 8u);
    EXPECT_EQ(map.load_factor(), 1.0f);
    auto it = map.find(3);
    map.insert({3, 100});   // an existing key never rehashes
    map[5] = 5;
    EXPECT_EQ(map.bucket_count(), 8u);
    EXPECT_EQ(it, map.find(3));
    map.emplace(8, 8);
    EXPECT_EQ(map.bucket_count(), 16u);
    EXPECT_LE(map.load_factor(), map.max_load_factor());
    for (int i = 9; i < 1000; ++i) {
        map.emplace(i, i);
        EXPECT_LE(map.load_factor(), map.max_load_factor());
    }
    EXPECT_EQ(map.bucket_count(), 1024u);

    map.max_load_factor(0.25f);
    EXPECT_EQ(map.max_load_factor(), 0.25f);
    map.emplace(1000, 1000);
    EXPECT_LE(map.load_factor(), 0.25f);
    EXPECT_EQ(map.bucket_count(), 4096u);
    map.max_load_factor(0.0f);
    map.max_load_factor(-1.0f);
    map.max_load_factor(std::numeric_limits<float>::quiet_NaN());
    EXPECT_EQ(map.max_load_factor(), 0.25f);
    map.max_load_factor(4.0f);
    for (int i = 1001; i < 4000; ++i) {
        map.emplace(i, i);
    }
    EXPECT_EQ(map.bucket_count(), 4096u);
    EXPECT_EQ(map.size(), 4000u);
    for (int i = 0; i < 4000; ++i) {
        ASSERT_EQ(map.at(i), i);
    }
}

TEST(Map_Test, BucketInterface) {
    sgcl::map<int, int> map;
    for (int i = 0; i < 100; ++i) {
        map.emplace(i * 7, i);
    }
    EXPECT_EQ(sum_of_bucket_sizes(map), 100u);
    size_t through_local = 0;
    for (size_t n = 0; n < map.bucket_count(); ++n) {
        size_t here = 0;
        for (auto it = map.begin(n); it != map.end(n); ++it) {
            EXPECT_EQ(map.bucket(it->first), n);
            ++here;
        }
        EXPECT_EQ(here, map.bucket_size(n));
        EXPECT_EQ((size_t)std::distance(map.cbegin(n), map.cend(n)), here);
        through_local += here;
    }
    EXPECT_EQ(through_local, 100u);
    for (int i = 0; i < 100; ++i) {
        auto n = map.bucket(i * 7);
        EXPECT_LT(n, map.bucket_count());
        bool found = false;
        for (auto it = map.begin(n); it != map.end(n); ++it) {
            found |= it->first == i * 7;
        }
        EXPECT_TRUE(found);
    }
    const auto& cmap = map;
    auto n = cmap.bucket(0);
    EXPECT_NE(cmap.begin(n), cmap.end(n));
    auto lit = map.begin(n);
    sgcl::map<int, int>::const_local_iterator clit = lit;
    EXPECT_EQ(clit, cmap.begin(n));
    auto post = lit++;
    EXPECT_EQ(post, clit);
    EXPECT_EQ(map.begin(map.bucket_count() + 5), map.end(map.bucket_count() + 5));
    EXPECT_EQ(map.bucket_size(map.bucket_count() + 5), 0u);
}

TEST(Map_Test, TransparentLookup) {
    using Transparent = sgcl::map<std::string, int, StringViewHash, StringViewEqual>;
    using Opaque = sgcl::map<std::string, int>;
    static_assert(FindsStringView<Transparent>);
    static_assert(!FindsStringView<Opaque>);   // no conversion from string_view to the key

    Transparent map = {{"alpha", 1}, {"beta", 2}};
    std::string_view alpha = "alpha";
    EXPECT_NE(map.find(alpha), map.end());
    EXPECT_EQ(map.find(alpha)->second, 1);
    EXPECT_EQ(map.find("gamma"), map.end());
    EXPECT_EQ(map.count(alpha), 1u);
    EXPECT_TRUE(map.contains("beta"));
    EXPECT_FALSE(map.contains(std::string_view("delta")));
    EXPECT_EQ(map.at(alpha), 1);
    EXPECT_THROW(map.at(std::string_view("delta")), std::out_of_range);
    auto [first, last] = map.equal_range(std::string_view("beta"));
    EXPECT_EQ(std::distance(first, last), 1);
    EXPECT_EQ(map.bucket(alpha), map.bucket(std::string("alpha")));
    const Transparent& cmap = map;
    EXPECT_EQ(cmap.find(alpha)->second, 1);
    EXPECT_EQ(cmap.at(alpha), 1);
    auto node = map.extract(std::string_view("beta"));
    EXPECT_EQ(node.key(), "beta");
    map.insert(std::move(node));
    EXPECT_EQ(map.erase(std::string_view("beta")), 1u);
    EXPECT_EQ(map.erase("nothing"), 0u);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.erase(map.begin()), map.end());   // erase(iterator) is still preferred

    // A non-transparent hasher converts the argument to a key.
    using CountedTransparent = sgcl::map<CountedKey, int, CountedKeyHash, CountedKeyEqual>;
    using CountedOpaque = sgcl::map<CountedKey, int, OpaqueKeyHash>;
    static_assert(FindsInt<CountedTransparent>);
    static_assert(FindsInt<CountedOpaque>);
    CountedTransparent transparent = {{CountedKey(1), 1}};
    CountedOpaque opaque = {{CountedKey(1), 1}};
    auto before = CountedKey::constructions;
    EXPECT_TRUE(transparent.contains(1));
    EXPECT_EQ(transparent.count(2), 0u);
    EXPECT_EQ(transparent.find(1)->second, 1);
    EXPECT_EQ(CountedKey::constructions, before);
    EXPECT_TRUE(opaque.contains(1));
    EXPECT_EQ(CountedKey::constructions, before + 1);
}

TEST(Map_Test, EagerDestruction) {
    sgcl::map<Int, Int, IntHash, IntEqual> map;
    for (int i = 0; i < 10; ++i) {
        map.emplace(i, i);
    }
    EXPECT_EQ(Int::counter, 20u);
    map.erase(3);
    EXPECT_EQ(Int::counter, 18u);
    map.erase(map.find(4));
    EXPECT_EQ(Int::counter, 16u);
    auto node = map.extract(5);
    EXPECT_EQ(Int::counter, 16u);
    node = {};
    EXPECT_EQ(Int::counter, 14u);
    map.emplace(0, 100);   // a duplicate: the temporary element dies now
    EXPECT_EQ(Int::counter, 14u);
    map.insert_or_assign(0, 100);
    EXPECT_EQ(Int::counter, 14u);
    map = {{20, 20}};
    EXPECT_EQ(Int::counter, 2u);
    sgcl::map<Int, Int, IntHash, IntEqual> other = {{30, 30}, {31, 31}};
    map = other;
    EXPECT_EQ(Int::counter, 8u);
    map = std::move(other);
    EXPECT_EQ(Int::counter, 4u);
    map.clear();
    EXPECT_EQ(Int::counter, 0u);
    map.emplace(1, 1);
    {
        auto copy = map;
        EXPECT_EQ(Int::counter, 4u);
    }
    EXPECT_EQ(Int::counter, 2u);
}

TEST(Map_Test, MappedTrackedPtr) {
    sgcl::map<int, tracked_ptr<Baz>> map;
    off_frame([&] {
        for (int i = 0; i < 3; ++i) {
            map[i] = make_tracked<Baz>(i * 10);
        }
    });
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 8u);   // 3 nodes, 3 Baz, the buckets, the sentinel
    off_frame([&] {
        for (int i = 0; i < 3; ++i) {
            EXPECT_EQ(map.at(i)->value, i * 10);
        }
        map.erase(1);
    });
    EXPECT_EQ(collector::get_live_object_count(), 6u);   // the node and its Baz are gone
    off_frame([&] {
        map.at(0) = nullptr;
    });
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    off_frame([&] {
        map.clear();
    });
    EXPECT_EQ(collector::get_live_object_count(), 2u);
}

TEST(Map_Test, MapInsideManagedObject) {
    tracked_ptr<Holder> holder = make_tracked<Holder>();
    off_frame([&] {
        for (int i = 0; i < 20; ++i) {
            holder->map[i] = make_tracked<Baz>(i);
        }
    });
    collector::force_collect(true);
    off_frame([&] {
        EXPECT_EQ(holder->map.size(), 20u);
        for (int i = 0; i < 20; ++i) {
            EXPECT_EQ(holder->map.at(i)->value, i);
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), 43u);   // the holder, 20 nodes, 20 Baz, the buckets, the sentinel
    off_frame([&] {
        holder->map.erase(7);
    });
    EXPECT_EQ(collector::get_live_object_count(), 41u);
    holder = nullptr;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(Map_Test, ThrowingConstructorLeavesMapUnchanged) {
    sgcl::map<int, Throwing> map;
    map.try_emplace(1, 1);
    auto buckets = map.bucket_count();
    off_frame([&] {   // nodes built for the failed inserts must not linger in this frame
    EXPECT_THROW(map.try_emplace(2, -1), std::runtime_error);
    EXPECT_THROW(map.emplace(3, -1), std::runtime_error);
    EXPECT_THROW(map.emplace(std::piecewise_construct, std::forward_as_tuple(4), std::forward_as_tuple(-1)), std::runtime_error);
    EXPECT_THROW(map.insert_or_assign(5, Throwing(-1)), std::runtime_error);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(map.bucket_count(), buckets);
    EXPECT_FALSE(map.contains(2));
    EXPECT_FALSE(map.contains(3));
    EXPECT_FALSE(map.contains(4));
    EXPECT_EQ(map.at(1).value, 1);
    map.try_emplace(2, 2);
    EXPECT_EQ(map.size(), 2u);
    });
    EXPECT_EQ(collector::get_live_object_count(), 4u);
}

// The hash and the equality are noexcept (a static_assert of the map) and
// running out of memory ends the program: an insertion throws only what
// the element's construction throws, the lookups and erasures nothing
// (DESIGN 356)
static_assert(noexcept(std::declval<sgcl::map<int, int>&>().emplace(1, 1)));
static_assert(noexcept(std::declval<sgcl::map<int, int>&>().insert(std::pair<const int, int>(1, 1))));
static_assert(noexcept(std::declval<sgcl::map<int, int>&>().try_emplace(1, 1)));
static_assert(noexcept(std::declval<sgcl::map<int, int>&>()[1]));
static_assert(noexcept(std::declval<sgcl::map<int, int>&>().insert_or_assign(1, 1)));
static_assert(noexcept(std::declval<const sgcl::map<int, int>&>().find(1)));
static_assert(noexcept(std::declval<const sgcl::map<int, int>&>().contains(1)));
static_assert(noexcept(std::declval<const sgcl::map<int, int>&>().count(1)));
static_assert(noexcept(std::declval<sgcl::map<int, int>&>().erase(1)));
static_assert(noexcept(std::declval<sgcl::map<int, int>&>().take(1)));
static_assert(noexcept(std::declval<sgcl::map<int, int>&>().reserve(100)));
static_assert(noexcept(std::declval<sgcl::multimap<int, int>&>().emplace(1, 1)));
static_assert(noexcept(std::declval<sgcl::ordered_map<int, int>&>().try_emplace(1, 1)));
static_assert(!noexcept(std::declval<sgcl::map<int, Throwing>&>().try_emplace(1, 1)));
static_assert(!noexcept(std::declval<sgcl::map<std::string, int>&>()[std::declval<const std::string&>()]));
static_assert(noexcept(std::declval<sgcl::map<std::string, int>&>()[std::string()]));
static_assert(noexcept(sgcl::map<int, int>()));
static_assert(noexcept(sgcl::map<int, int>(16)));
static_assert(noexcept(std::declval<const sgcl::map<int, int>&>().hash_function()));
static_assert(noexcept(std::declval<const sgcl::map<int, int>&>().key_eq()));

TEST(Map_Test, Equality) {
    sgcl::map<int, int> a;
    sgcl::map<int, int> b(1000);
    for (int i = 0; i < 100; ++i) {
        a.emplace(i, i);
        b.emplace(99 - i, 99 - i);
    }
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    b[50] = 0;
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
    b[50] = 50;
    b.erase(99);
    EXPECT_FALSE(a == b);
    b.emplace(100, 100);
    EXPECT_FALSE(a == b);
    sgcl::map<int, int> empty1, empty2;
    EXPECT_TRUE(empty1 == empty2);
    EXPECT_FALSE(empty1 == a);
}

TEST(Map_Test, EraseIf) {
    sgcl::map<int, Int> map;
    for (int i = 0; i < 100; ++i) {
        map.emplace(i, i);
    }
    auto erased = erase_if(map, [](const auto& p) { return p.first % 2 == 0; });
    EXPECT_EQ(erased, 50u);
    EXPECT_EQ(map.size(), 50u);
    EXPECT_EQ(Int::counter, 50u);
    for (auto& [k, v] : map) {
        EXPECT_EQ(k % 2, 1);
    }
    EXPECT_EQ(erase_if(map, [](const auto&) { return false; }), 0u);
    EXPECT_EQ(erase_if(map, [](const auto&) { return true; }), 50u);
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(Int::counter, 0u);
}

TEST(Map_Test, StringKeys) {
    sgcl::map<std::string, std::string> map;
    for (int i = 0; i < 1000; ++i) {
        auto key = "key-" + std::to_string(i);
        auto [it, inserted] = map.emplace(key, std::to_string(i));
        EXPECT_TRUE(inserted);
        EXPECT_EQ(it->first, key);
    }
    EXPECT_EQ(map.size(), 1000u);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(map.at("key-" + std::to_string(i)), std::to_string(i));
    }
    EXPECT_FALSE(map.contains("key-1000"));
    for (int i = 0; i < 1000; i += 2) {
        EXPECT_EQ(map.erase("key-" + std::to_string(i)), 1u);
    }
    EXPECT_EQ(map.size(), 500u);
    map.rehash(8);
    for (int i = 1; i < 1000; i += 2) {
        EXPECT_EQ(map.at("key-" + std::to_string(i)), std::to_string(i));
    }
    sgcl::map<std::string, std::string> copy = map;
    EXPECT_TRUE(copy == map);
    copy["key-1"] += "!";
    EXPECT_FALSE(copy == map);
}

TEST(Map_Test, StressAgainstStd) {
    sgcl::map<int, int> map;
    std::unordered_map<int, int> oracle;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> key(0, 20000);
    std::uniform_int_distribution<int> op(0, 99);
    for (int i = 0; i < 200000; ++i) {
        auto k = key(rng);
        auto o = op(rng);
        if (o < 40) {
            auto [it, inserted] = map.insert({k, i});
            auto [oit, oinserted] = oracle.insert({k, i});
            ASSERT_EQ(inserted, oinserted);
            ASSERT_EQ(it->second, oit->second);
        } else if (o < 55) {
            map[k] = i;
            oracle[k] = i;
        } else if (o < 80) {
            ASSERT_EQ(map.erase(k), oracle.erase(k));
        } else if (o < 85) {
            auto it = map.find(k);
            auto oit = oracle.find(k);
            ASSERT_EQ(it != map.end(), oit != oracle.end());
            if (it != map.end()) {
                auto next = map.erase(it);
                oracle.erase(oit);
                ASSERT_EQ(next == map.end() || oracle.count(next->first) == 1, true);
            }
        } else {
            auto it = map.find(k);
            auto oit = oracle.find(k);
            ASSERT_EQ(it != map.end(), oit != oracle.end());
            if (it != map.end()) {
                ASSERT_EQ(it->second, oit->second);
            }
        }
        ASSERT_EQ(map.size(), oracle.size());
        if (i % 10000 == 9999) {
            collector::force_collect();
        }
        if (i % 50000 == 49999) {
            size_t seen = 0;
            for (const auto& [ok, ov] : oracle) {
                auto it = map.find(ok);
                ASSERT_NE(it, map.end());
                ASSERT_EQ(it->second, ov);
            }
            for (const auto& [mk, mv] : map) {
                ASSERT_EQ(oracle.at(mk), mv);
                ++seen;
            }
            ASSERT_EQ(seen, oracle.size());
            ASSERT_EQ(sum_of_bucket_sizes(map), map.size());
            ASSERT_LE(map.load_factor(), map.max_load_factor());
        }
    }
    collector::force_collect(true);
    EXPECT_EQ(map.size(), oracle.size());
    for (const auto& [ok, ov] : oracle) {
        ASSERT_EQ(map.at(ok), ov);
    }
}

namespace {
    // A key whose copies and moves are counted: a range of std::pairs is
    // hashed and looked up where it is and copied once, into the node
    struct CountedString {
        std::string s;
        inline static int copies = 0;
        inline static int moves = 0;

        CountedString(const char* v)
        : s(v) {
        }

        CountedString(const CountedString& other)
        : s(other.s) {
            ++copies;
        }

        CountedString(CountedString&& other) noexcept
        : s(std::move(other.s)) {
            ++moves;
        }

        bool operator==(const CountedString& other) const noexcept {
            return s == other.s;
        }
    };

    struct CountedStringHash {
        size_t operator()(const CountedString& k) const noexcept {
            return std::hash<std::string>{}(k.s);
        }
    };
}

TEST(Map_Test, ARangeOfPairsCopiesEachKeyOnce) {
    std::vector<std::pair<CountedString, int>> src;
    src.emplace_back("a", 1);
    src.emplace_back("b", 2);
    src.emplace_back("c", 3);
    CountedString::copies = 0;
    CountedString::moves = 0;
    sgcl::map<CountedString, int, CountedStringHash> map(src.begin(), src.end());
    EXPECT_EQ(map.size(), 3u);
    EXPECT_EQ(CountedString::copies, 3);   // one per element, into its node
    EXPECT_EQ(CountedString::moves, 0);
    map.insert(src.begin(), src.end());    // duplicates: found where they are, nothing built
    EXPECT_EQ(map.size(), 3u);
    EXPECT_EQ(CountedString::copies, 3);
    EXPECT_EQ(CountedString::moves, 0);
    std::vector<std::pair<CountedString, int>> more;
    more.emplace_back("d", 4);
    map.insert(std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
    EXPECT_EQ(map.size(), 4u);
    EXPECT_EQ(CountedString::copies, 3);
    EXPECT_EQ(CountedString::moves, 1);    // moved into the node
    EXPECT_EQ(map.at("d"), 4);
    std::vector<std::pair<const char*, int>> other = {{"e", 5}, {"a", 0}};
    map.insert(other.begin(), other.end());   // another pair type: converted to a value_type once (no copy), whose const key the node then copies, as from any value_type&&
    EXPECT_EQ(map.size(), 5u);
    EXPECT_EQ(map.at("a"), 1);
    EXPECT_EQ(CountedString::copies, 4);   // "e"; the duplicate "a" is found before any copy
    EXPECT_EQ(CountedString::moves, 1);
    sgcl::multimap<CountedString, int, CountedStringHash> multi(src.begin(), src.end());
    multi.insert(src.begin(), src.end());
    EXPECT_EQ(multi.size(), 6u);
    EXPECT_EQ(CountedString::copies, 10);
}

namespace {
    // A managed object holding a node handle: the handle dies in the sweep
    // that frees the object, with the node it holds garbage of the same
    // sweep. Two holder types, so that one's page is swept before the
    // nodes' and the other's after: in one order the sweep destroys the
    // element through the node first, and the handle must leave it alone.
    template<int>
    struct HandleHolder {
        sgcl::map<int, Int>::node_type node;
    };

    template<int Tag>
    void drop_a_handle_in_a_managed_object(bool holder_first) {
        off_frame([&] {
            tracked_ptr<HandleHolder<Tag>> holder;
            if (holder_first) {
                holder = make_tracked<HandleHolder<Tag>>();
            }
            sgcl::map<int, Int> map = {{1, 10}, {2, 20}};
            if (!holder_first) {
                holder = make_tracked<HandleHolder<Tag>>();
            }
            holder->node = map.extract(1);
        });
        collector::clear_stack();
        for (int k = 0; k < 3; ++k) {
            collector::force_collect(true);
        }
    }
}

TEST(Map_Test, ANodeHandleDyingInASweepDestroysItsElementOnce) {
    collector::clear_stack();
    collector::force_collect(true);
    const auto before = Int::counter;
    for (int i = 0; i < 10; ++i) {
        drop_a_handle_in_a_managed_object<1>(true);
        ASSERT_EQ(Int::counter, before);   // destroyed once: by the node's sweep or by the handle, never both
        drop_a_handle_in_a_managed_object<2>(false);
        ASSERT_EQ(Int::counter, before);
    }
}

TEST(Map_Test, KeysAStrideApartSpreadOverTheBuckets) {
    // an integer's hash is itself: keys a power of two apart differ only
    // above the mask, and each would share one bucket with the rest
    // (43 ns a find became 18 us); the bucket takes the higher bits too
    for (unsigned shift : {4u, 12u, 20u, 32u, 44u}) {
        sgcl::map<uint64_t, int> m;
        for (uint64_t i = 0; i < 4096; ++i) {
            m.emplace(i << shift, int(i));
        }
        size_t longest = 0;
        for (size_t n = 0; n < m.bucket_count(); ++n) {
            longest = std::max(longest, m.bucket_size(n));
        }
        EXPECT_LE(longest, 8u) << "a stride of 2^" << shift;
        EXPECT_EQ(m.find(uint64_t(4095) << shift)->second, 4095);
    }
    // consecutive keys stay in consecutive buckets, one each
    sgcl::map<uint64_t, int> seq;
    for (uint64_t i = 0; i < 1000; ++i) {
        seq.emplace(i, int(i));
    }
    for (uint64_t i = 0; i < 1000; ++i) {
        EXPECT_EQ(seq.bucket(i), i);
    }
}

// value_or(key, fallback): the value under the key, or the fallback, by value;
// a key of another type through the transparent lookup; a tracked value
// comes back as the same object
TEST(Map_Test, ValueOrWithAFallback) {
    sgcl::map<sgcl::string, int> m = {{"a", 1}, {"b", 2}};
    EXPECT_EQ(m.value_or("a", 0), 1);
    EXPECT_EQ(m.value_or("z", -1), -1);
    EXPECT_EQ(m.value_or(std::string_view("b"), 0), 2);
    EXPECT_EQ(m.value_or(std::string_view("zz"), 7), 7);
    const auto& c = m;
    EXPECT_EQ(c.value_or("b", 0), 2);
    sgcl::map<int, sgcl::tracked_ptr<int>> p;
    sgcl::tracked_ptr<int> one = sgcl::make_tracked<int>(1);
    sgcl::tracked_ptr<int> none = sgcl::make_tracked<int>(0);
    p.try_emplace(1, one);
    EXPECT_EQ(p.value_or(1, none), one);
    EXPECT_EQ(p.value_or(2, none), none);
    static_assert(std::is_same_v<decltype(m.value_or("a", 0)), int>);
}

// Boundaries (DESIGN 408)

// A count of buckets past what an address space holds (the rounding up of
// a count past the largest power of two was undefined: map(SIZE_MAX) made
// a map of one bucket) is the count no memory gives: the program ends as
// at any refused managed allocation. So do rehash and reserve of such a
// count (reserve's division by the load factor made a double past size_t,
// whose conversion was undefined), on every table and with a load factor
// so small that one element wants more buckets than there can be
TEST(Map_Test, ABucketCountNoMemoryHoldsEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    EXPECT_DEATH((sgcl::map<int, int>(SIZE_MAX)), "sgcl: out of managed memory");
    EXPECT_DEATH((sgcl::map<int, int>((size_t(1) << 63) + 1)), "sgcl: out of managed memory");
    EXPECT_DEATH((sgcl::set<int>(SIZE_MAX)), "sgcl: out of managed memory");
    EXPECT_DEATH((sgcl::multimap<int, int>(SIZE_MAX)), "sgcl: out of managed memory");
    EXPECT_DEATH((sgcl::ordered_map<int, int>(SIZE_MAX)), "sgcl: out of managed memory");
    std::vector<int> one = {1};
    EXPECT_DEATH((sgcl::set<int>(one.begin(), one.end(), SIZE_MAX)), "sgcl: out of managed memory");
    auto rehash_empty = [] {
        sgcl::map<int, int> m;
        m.rehash(SIZE_MAX);
    };
    auto rehash_one = [] {
        sgcl::map<int, int> m = {{1, 1}};
        m.rehash((size_t(1) << 63) + 1);
    };
    auto reserve = [] {
        sgcl::map<int, int> m;
        m.reserve(SIZE_MAX);
    };
    auto grow = [] {
        sgcl::map<int, int> m;
        m.max_load_factor(1e-30f);
        m.emplace(1, 1);
    };
    auto reserve_one = [] {
        sgcl::set<int> s;
        s.max_load_factor(std::numeric_limits<float>::denorm_min());
        s.reserve(1);
    };
    EXPECT_DEATH(rehash_empty(), "sgcl: out of managed memory");
    EXPECT_DEATH(rehash_one(), "sgcl: out of managed memory");
    EXPECT_DEATH(reserve(), "sgcl: out of managed memory");
    EXPECT_DEATH(grow(), "sgcl: out of managed memory");
    EXPECT_DEATH(reserve_one(), "sgcl: out of managed memory");
}

// The counts of buckets at their ends: 0 makes no array, 1, 2 and 3 round
// up to a power of two; rehash(0) and reserve(0) of an empty table leave
// it without one, and of a full one shrink it to what the elements need;
// the limits are the largest difference_type
TEST(Map_Test, BucketCountsAtTheirEnds) {
    EXPECT_EQ((sgcl::map<int, int>(0).bucket_count()), 0u);
    EXPECT_EQ((sgcl::map<int, int>(1).bucket_count()), 1u);
    EXPECT_EQ((sgcl::map<int, int>(2).bucket_count()), 2u);
    EXPECT_EQ((sgcl::map<int, int>(3).bucket_count()), 4u);
    EXPECT_EQ((sgcl::ordered_set<int>(3).bucket_count()), 4u);
    std::vector<int> none;
    EXPECT_EQ((sgcl::set<int>(none.begin(), none.end(), 0).bucket_count()), 0u);
    sgcl::map<int, int> m;
    m.rehash(0);
    m.reserve(0);
    EXPECT_EQ(m.bucket_count(), 0u);
    EXPECT_EQ(m.load_factor(), 0.0f);
    EXPECT_EQ(m.bucket_size(0), 0u);
    EXPECT_EQ(m.begin(0), m.end(0));
    sgcl::map<int, int> one(1);   // a table of one bucket: everything in it, and growing from it
    one.emplace(INT_MIN, 1);
    EXPECT_EQ(one.bucket(INT_MIN), 0u);
    for (int i = 0; i < 100; ++i) {
        one.emplace(i, i);
    }
    EXPECT_EQ(one.size(), 101u);
    EXPECT_EQ(one.at(INT_MIN), 1);
    one.rehash(0);
    EXPECT_EQ(one.bucket_count(), 128u);   // the least power of two the elements fit at a load factor of 1
    one.reserve(0);
    EXPECT_EQ(one.bucket_count(), 128u);
    EXPECT_EQ(m.max_size(), size_t(PTRDIFF_MAX));
    EXPECT_EQ(m.max_bucket_count(), size_t(PTRDIFF_MAX));
    EXPECT_EQ((sgcl::multiset<char>().max_size()), size_t(PTRDIFF_MAX));
}

// max_load_factor at its ends: 0, negative values and NaN are ignored; an
// infinite one keeps the first array of eight buckets for good, every
// lookup still finding its key, and needs no bucket for any count
TEST(Map_Test, MaxLoadFactorAtItsEnds) {
    sgcl::map<int, int> m;
    m.max_load_factor(0.0f);
    m.max_load_factor(-1.0f);
    m.max_load_factor(std::numeric_limits<float>::quiet_NaN());
    m.max_load_factor(-std::numeric_limits<float>::infinity());
    EXPECT_EQ(m.max_load_factor(), 1.0f);
    m.max_load_factor(std::numeric_limits<float>::infinity());
    for (int i = 0; i < 1000; ++i) {
        m.emplace(i, -i);
    }
    EXPECT_EQ(m.bucket_count(), 8u);
    for (int i = 0; i < 1000; ++i) {
        ASSERT_EQ(m.at(i), -i);
    }
    m.reserve(SIZE_MAX);
    EXPECT_EQ(m.bucket_count(), 8u);
    m.max_load_factor(1.0f);
    m.rehash(0);
    EXPECT_EQ(m.bucket_count(), 1024u);
    EXPECT_EQ(m.size(), 1000u);
}

namespace {
    // The key as its own hash: the bucket's mixing of the high bits at the
    // ends of size_t
    struct IdentityHash {
        size_t operator()(size_t k) const noexcept {
            return k;
        }
    };
}

// Hashes at the ends of size_t: 0, all ones, the top bit alone, keys apart
// in the top bits alone; found across every growth from one bucket and at
// each power of two of buckets from 1 to 1024, erased one by one
TEST(Map_Test, HashesAtTheEndsOfTheirRange) {
    std::vector<size_t> keys = {0, 1, SIZE_MAX, SIZE_MAX - 1, size_t(1) << 63, (size_t(1) << 63) - 1, (size_t(1) << 63) + 1};
    for (int bit = 40; bit < 64; ++bit) {
        keys.push_back((size_t(1) << bit) | 5);
    }
    sgcl::map<size_t, size_t, IdentityHash> m(1);
    sgcl::ordered_map<size_t, size_t, IdentityHash> o(1);
    sgcl::multimap<size_t, size_t, IdentityHash> mm(1);
    for (auto k : keys) {
        EXPECT_TRUE(m.emplace(k, ~k).second);
        EXPECT_TRUE(o.emplace(k, ~k).second);
        mm.emplace(k, 1);
        mm.emplace(k, 2);
    }
    m.max_load_factor(1e9f);   // so that a rehash may go down to one bucket
    for (size_t buckets = 1; buckets <= 1024; buckets *= 2) {
        m.rehash(buckets);
        ASSERT_EQ(m.bucket_count(), buckets);
        for (auto k : keys) {
            ASSERT_EQ(m.at(k), ~k);
            ASSERT_LT(m.bucket(k), m.bucket_count());
            ASSERT_EQ(mm.count(k), 2u);
        }
    }
    EXPECT_TRUE(std::equal(o.begin(), o.end(), keys.begin(), keys.end(), [](auto& e, size_t k) { return e.first == k; }));
    for (auto k : keys) {
        EXPECT_EQ(m.erase(k), 1u);
        EXPECT_EQ(o.erase(k), 1u);
        EXPECT_EQ(mm.erase(k), 2u);
        EXPECT_FALSE(m.contains(k));
    }
    EXPECT_TRUE(m.empty());
    EXPECT_TRUE(o.empty());
    EXPECT_TRUE(mm.empty());
}

namespace {
    // Every member a test of a moved-from or default-constructed table
    // calls: it is empty and without buckets, and works as an empty one
    template<class M, class K, class E>
    void expect_works_empty(M& m, const K& key, const E& element) {
        EXPECT_TRUE(m.empty());
        EXPECT_EQ(m.size(), 0u);
        EXPECT_EQ(m.begin(), m.end());
        EXPECT_EQ(m.cbegin(), m.cend());
        EXPECT_EQ(m.bucket_count(), 0u);
        EXPECT_EQ(m.load_factor(), 0.0f);
        EXPECT_EQ(m.find(key), m.end());
        EXPECT_FALSE(m.contains(key));
        EXPECT_EQ(m.count(key), 0u);
        auto [first, last] = m.equal_range(key);
        EXPECT_EQ(first, m.end());
        EXPECT_EQ(last, m.end());
        EXPECT_EQ(m.erase(key), 0u);
        EXPECT_EQ(m.erase(m.begin(), m.end()), m.end());
        EXPECT_TRUE(m.extract(key).empty());
        EXPECT_EQ(sgcl::erase_if(m, [](auto&) { return true; }), 0u);
        EXPECT_TRUE(m == M());
        M copy(m);
        EXPECT_TRUE(copy.empty());
        EXPECT_EQ(copy.bucket_count(), 0u);
        M other;
        m.swap(other);
        m.swap(other);
        m.merge(other);
        other.merge(m);
        EXPECT_TRUE(m.empty());
        m.clear();
        m.rehash(0);
        EXPECT_EQ(m.bucket_count(), 0u);
        m.insert(element);
        EXPECT_EQ(m.size(), 1u);
        EXPECT_TRUE(m.contains(key));
        m.clear();
        EXPECT_TRUE(m.empty());
        EXPECT_NE(m.bucket_count(), 0u);   // clear keeps the buckets
    }
}

// A table moved from (by construction and by assignment) and a default one:
// empty, without buckets, every member working as on an empty table; the
// moved-from one keeps its load factor
TEST(Map_Test, MovedFromAndDefaultWorkAsEmpty) {
    sgcl::map<sgcl::string, int> m = {{"a", 1}, {"b", 2}};
    m.max_load_factor(0.5f);
    sgcl::map<sgcl::string, int> to(std::move(m));
    EXPECT_EQ(to.size(), 2u);
    EXPECT_EQ(m.max_load_factor(), 0.5f);
    EXPECT_FALSE(m.take("a").has_value());
    EXPECT_EQ(m.value_or("a", -1), -1);
    EXPECT_THROW(m.at("a"), std::out_of_range);
    expect_works_empty(m, sgcl::string("a"), std::pair<const sgcl::string, int>("a", 1));
    sgcl::map<sgcl::string, int> assigned = {{"c", 3}};
    assigned = std::move(to);
    EXPECT_EQ(assigned.size(), 2u);
    expect_works_empty(to, sgcl::string("a"), std::pair<const sgcl::string, int>("a", 1));
    sgcl::map<sgcl::string, int> d;
    expect_works_empty(d, sgcl::string("z"), std::pair<const sgcl::string, int>("z", 0));

    sgcl::multimap<int, int> mm = {{1, 1}, {1, 2}};
    auto mm2 = std::move(mm);
    expect_works_empty(mm, 1, std::pair<const int, int>(1, 1));
    sgcl::set<int> s = {1, 2};
    auto s2 = std::move(s);
    expect_works_empty(s, 1, 1);
    sgcl::multiset<int> ms = {1, 1};
    auto ms2 = std::move(ms);
    expect_works_empty(ms, 1, 1);
    sgcl::ordered_map<int, int> om = {{1, 1}};
    auto om2 = std::move(om);
    EXPECT_EQ(om.rbegin(), om.rend());
    expect_works_empty(om, 1, std::pair<const int, int>(1, 1));
    sgcl::ordered_set<int> os = {1};
    auto os2 = std::move(os);
    expect_works_empty(os, 1, 1);
    EXPECT_EQ(os2.size(), 1u);
}

namespace {
    // A table on both sides: a copy and a move assignment to itself, swap
    // with itself and a merge of itself change nothing
    template<class M>
    void expect_self_operations_keep(M m) {
        const M before = m;
        auto first = m.begin();
        auto& self = m;
        m = self;
        EXPECT_TRUE(m == before);
        EXPECT_EQ(m.begin(), first);   // nothing was rebuilt
        m = std::move(self);
        EXPECT_TRUE(m == before);
        EXPECT_EQ(m.begin(), first);
        m.swap(self);
        swap(m, self);
        EXPECT_TRUE(m == before);
        m.merge(self);
        m.merge(std::move(self));
        EXPECT_TRUE(m == before);
        EXPECT_EQ(m.size(), before.size());
        EXPECT_TRUE(m == self);
        EXPECT_FALSE(m != self);
    }
}

TEST(Map_Test, ATableOnBothSidesKeepsItself) {
    expect_self_operations_keep(sgcl::map<sgcl::string, int>{{"a", 1}, {"b", 2}, {"c", 3}});
    expect_self_operations_keep(sgcl::multimap<int, int>{{1, 1}, {1, 2}, {2, 3}});
    expect_self_operations_keep(sgcl::set<int>{1, 2, 3});
    expect_self_operations_keep(sgcl::multiset<int>{1, 1, 2});
    expect_self_operations_keep(sgcl::ordered_map<int, int>{{3, 1}, {1, 2}, {2, 3}});
    expect_self_operations_keep(sgcl::ordered_set<int>{3, 1, 2});
    expect_self_operations_keep(sgcl::map<int, int>());
    expect_self_operations_keep(sgcl::ordered_set<int>());
}

// The table's own element, key or value as the argument: an insertion of
// an element it holds (nothing in a unique table, a copy next to it in a
// multi one, across growths), operator[], try_emplace, insert_or_assign
// of the value under the key, take, extract and erase by the key of the
// element they take. An erasure of a multi table by the key of an element
// of the run read that key after the element was destroyed and stopped
// there: the run is now found before anything is erased
TEST(Map_Test, TheTablesOwnElementAsTheArgument) {
    using boundary::Poisoned;
    sgcl::map<Poisoned, Poisoned> m;
    for (int i = 0; i < 8; ++i) {
        m.emplace(i, i * 10);
    }
    auto it = m.find(3);
    EXPECT_FALSE(m.insert(*it).second);
    EXPECT_FALSE(m.emplace(*it).second);
    EXPECT_FALSE(m.try_emplace(it->first, 0).second);
    EXPECT_EQ(&m[it->first], &it->second);
    EXPECT_FALSE(m.insert_or_assign(it->first, it->second).second);
    EXPECT_EQ(it->second, Poisoned(30));
    EXPECT_EQ(m.size(), 8u);
    EXPECT_EQ(m.erase(m.find(5)->first), 1u);
    EXPECT_FALSE(m.contains(5));
    auto taken = m.take(m.find(6)->first);
    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(*taken, Poisoned(60));
    auto nh = m.extract(m.find(7)->first);
    ASSERT_FALSE(nh.empty());
    EXPECT_EQ(nh.key(), Poisoned(7));
    EXPECT_EQ(m.size(), 5u);

    sgcl::multimap<Poisoned, int> mm;
    for (int i = 0; i < 3; ++i) {
        mm.emplace(1, i);
        mm.emplace(2, i);
    }
    for (int i = 0; i < 10; ++i) {   // across the growths: a copy of a node of the run
        mm.insert(*mm.find(1));
        mm.emplace(*mm.find(2));
    }
    EXPECT_EQ(mm.count(1), 13u);
    EXPECT_EQ(mm.count(2), 13u);
    EXPECT_EQ(mm.erase(mm.find(1)->first), 13u);   // the key of the first of the run
    EXPECT_EQ(mm.count(1), 0u);
    auto run = mm.equal_range(2);
    EXPECT_EQ(mm.erase(std::next(run.first, 5)->first), 13u);   // of one in the middle
    EXPECT_TRUE(mm.empty());

    sgcl::multiset<Poisoned> ms = {4, 4, 4, 5};
    EXPECT_EQ(ms.erase(*ms.find(4)), 3u);
    EXPECT_EQ(ms.size(), 1u);
    sgcl::set<Poisoned> s = {1, 2};
    EXPECT_EQ(s.erase(*s.find(1)), 1u);
    EXPECT_FALSE(s.insert(*s.begin()).second);
    EXPECT_EQ(s.size(), 1u);
    sgcl::ordered_map<Poisoned, int> om = {{1, 1}, {2, 2}};
    EXPECT_EQ(om.erase(om.front().first), 1u);
    EXPECT_FALSE(om.insert(om.back()).second);
    EXPECT_EQ(om.size(), 1u);
}

// An empty and a one-element table: an empty range erased anywhere, the
// element erased by iterator back to an empty table that keeps its
// buckets, an ordered table's end() stepping back to its one element
TEST(Map_Test, EmptyRangesAndOneElement) {
    sgcl::map<int, int> m = {{1, 1}};
    EXPECT_EQ(m.erase(m.begin(), m.begin()), m.begin());
    EXPECT_EQ(m.erase(m.end(), m.end()), m.end());
    EXPECT_EQ(m.size(), 1u);
    auto buckets = m.bucket_count();
    EXPECT_EQ(m.erase(m.begin()), m.end());
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.bucket_count(), buckets);
    EXPECT_EQ(m.erase(m.begin(), m.end()), m.end());

    sgcl::ordered_map<int, int> o = {{1, 1}};
    EXPECT_EQ(std::prev(o.end()), o.begin());
    EXPECT_EQ(&o.front(), &o.back());
    o.to_back(o.begin());
    o.to_front(o.begin());
    EXPECT_EQ(o.erase(o.begin(), o.begin()), o.begin());
    EXPECT_EQ(o.erase(o.begin()), o.end());
    EXPECT_EQ(o.begin(), o.end());
    EXPECT_EQ(o.rbegin(), o.rend());

    sgcl::multiset<int> ms = {7};
    EXPECT_EQ(ms.erase(7), 1u);
    EXPECT_EQ(ms.erase(7), 0u);
    std::vector<int> none;
    ms.insert(none.begin(), none.end());
    ms.insert(std::initializer_list<int>{});
    EXPECT_TRUE(ms.empty());
}

// The iterators as the pages state them: an unordered table's end() is null
// and never changes, an iterator follows its node across growth, rehash,
// swap, merge and move. An ordered table's end() is its sentinel, made
// with the first array, by an insertion or by rehash or reserve of a table
// that had none (an end() taken before is not end() after either), kept by
// clear and growth, and taken along by swap and the move
TEST(Map_Test, IteratorsAsThePagesStateThem) {
    sgcl::map<int, int> m;
    auto end = m.end();
    m.emplace(0, 0);
    auto zero = m.find(0);
    for (int i = 1; i < 1000; ++i) {
        m.emplace(i, i);
    }
    m.rehash(4096);
    EXPECT_EQ(m.end(), end);
    EXPECT_EQ(m.find(0), zero);
    sgcl::map<int, int> other = {{-1, -1}};
    m.swap(other);
    EXPECT_EQ(other.find(0), zero);
    m.merge(other);
    EXPECT_EQ(m.find(0), zero);
    sgcl::map<int, int> moved(std::move(m));
    EXPECT_EQ(moved.find(0), zero);
    EXPECT_EQ(moved.end(), end);

    sgcl::ordered_map<int, int> r;
    auto no_array = r.end();
    r.reserve(10);
    EXPECT_NE(r.end(), no_array);
    auto r_end = r.end();
    r.emplace(1, 1);
    EXPECT_EQ(std::prev(r.end()), r.begin());
    EXPECT_EQ(r.end(), r_end);
    r.clear();
    EXPECT_EQ(r.end(), r_end);
    sgcl::ordered_set<int> rs;
    auto rs_end = rs.end();
    rs.rehash(1);
    EXPECT_NE(rs.end(), rs_end);

    sgcl::ordered_map<int, int> o;
    o.emplace(1, 1);
    auto o_end = o.end();
    auto one = o.begin();
    for (int i = 2; i < 100; ++i) {
        o.emplace(i, i);
    }
    o.rehash(1024);
    EXPECT_EQ(o.end(), o_end);
    EXPECT_EQ(o.begin(), one);
    o.swap(r);
    EXPECT_EQ(r.end(), o_end);   // the sentinel goes with the elements
    EXPECT_EQ(r.begin(), one);
    sgcl::ordered_map<int, int> moved_o(std::move(r));
    EXPECT_EQ(moved_o.end(), o_end);
    EXPECT_EQ(moved_o.begin(), one);
}

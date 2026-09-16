//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// variant: std::variant's interface, the tracked pointers kept apart from
// the data, so that the collector keeps following them.
#include "types.h"

#include <string>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Pair {
        tracked_ptr<Node> node;
        int count;
    };

    using Value = variant<int, tracked_ptr<Node>, weak_ptr<Node>, Pair, double, std::string>;

    // A managed object holding a variant next to some data: what
    // std::variant would get wrong, the pointer sharing its word with the
    // ints of other objects
    struct Holder {
        Value value;
        tracked_ptr<Holder> next;
    };

    // Inlined into the test's frame: a frame of its own would sit where
    // the dead frames were and keep their words (root_ptr.cpp: live_after_collect)
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Variant_Tests, TheAlternativesAreLaidOutApart) {
    using V = variant<int, tracked_ptr<Node>, weak_ptr<Node>>;
    static_assert(sizeof(V) == 16);         // one pointer word, the int, the index
    static_assert(variant_size_v<Value> == 6);
    static_assert(std::variant_size_v<Value> == 6);
    static_assert(std::is_same_v<variant_alternative_t<1, Value>, tracked_ptr<Node>>);
    static_assert(std::is_same_v<std::variant_alternative_t<3, const Value>, const Pair>);
    static_assert(detail::MayContainTracked<V>::value);
    V a = 1;
    V b = make_tracked<Node>(2);
    V c = weak_ptr<Node>(get<1>(b));
    // the pointer alternatives share one word, at another offset than the int
    auto offset = [](const auto& v, const void* p) { return (const char*)p - (const char*)&v; };
    EXPECT_EQ(offset(b, get_if<1>(&b)), offset(c, get_if<2>(&c)));
    EXPECT_NE(offset(a, get_if<0>(&a)), offset(b, get_if<1>(&b)));
}

TEST(Variant_Tests, TheInterfaceOfStdVariant) {
    Value v;
    EXPECT_EQ(v.index(), 0u);
    EXPECT_TRUE(holds_alternative<int>(v));
    EXPECT_EQ(get<int>(v), 0);
    v = 2.5;
    EXPECT_EQ(v.index(), 4u);
    EXPECT_EQ(get<double>(v), 2.5);
    v = "text";                             // the converting constructor picks the string
    EXPECT_TRUE(holds_alternative<std::string>(v));
    EXPECT_EQ(get<5>(v), "text");
    v.emplace<Pair>(make_tracked<Node>(1), 7);
    EXPECT_EQ(get<Pair>(v).node->value, 1);
    EXPECT_EQ(get<Pair>(v).count, 7);
    EXPECT_EQ(get_if<Pair>(&v)->count, 7);
    EXPECT_EQ(get_if<int>(&v), nullptr);
    EXPECT_THROW(get<int>(v), bad_variant_access);
    Value in_place(std::in_place_index<0>, 3);
    EXPECT_EQ(get<0>(in_place), 3);
    Value list(std::in_place_type<std::string>, {'a', 'b'});
    EXPECT_EQ(get<std::string>(list), "ab");
    Value copy = v;
    EXPECT_EQ(get<Pair>(copy).node, get<Pair>(v).node);
    Value moved = std::move(copy);
    EXPECT_EQ(get<Pair>(moved).count, 7);
    moved = in_place;
    EXPECT_EQ(get<int>(moved), 3);
    swap(moved, v);
    EXPECT_TRUE(holds_alternative<Pair>(moved));
    EXPECT_EQ(get<int>(v), 3);
    EXPECT_FALSE(v.valueless_by_exception());
    auto described = visit([](const auto& x) -> std::string {
        using T = std::remove_cvref_t<decltype(x)>;
        if constexpr(std::is_same_v<T, int>) {
            return "int " + std::to_string(x);
        } else if constexpr(std::is_same_v<T, Pair>) {
            return "pair " + std::to_string(x.count);
        } else {
            return "other";
        }
    }, v);
    EXPECT_EQ(described, "int 3");
    EXPECT_EQ(visit<int>([](const auto& x, const auto& y) { return (int)sizeof(x) + (int)sizeof(y); }, v, moved), (int)sizeof(int) + (int)sizeof(Pair));
}

TEST(Variant_Tests, ComparisonsAndHash) {
    using V = variant<int, tracked_ptr<Node>, std::string>;
    V a = 1;
    V b = 2;
    V c = make_tracked<Node>(3);
    V d = c;
    EXPECT_TRUE(a == a && a != b && a < b && b > a && a <= a && b >= a);
    EXPECT_TRUE(c == d && a < c && (a <=> c) == std::strong_ordering::less);
    EXPECT_TRUE((a <=> b) == std::strong_ordering::less && (c <=> d) == std::strong_ordering::equal);
    EXPECT_NE(std::hash<V>()(a), std::hash<V>()(b));
    EXPECT_EQ(std::hash<V>()(c), std::hash<V>()(d));
    unordered_map<V, int> counts;
    counts[a] = 1;
    counts[c] = 2;
    EXPECT_EQ(counts[d], 2);
}

TEST(Variant_Tests, ThePointerAlternativeIsFollowedNextToInts) {
    settle();
    const int before = Node::alive.load();
    constexpr int Count = 2000;
    tracked_ptr<Holder> list;
    off_frame([&] {
        for (int i = 0; i < Count; ++i) {
            tracked_ptr holder = make_tracked<Holder>();
            // every other holder an int, so that the data alternative is
            // seen in the same type's objects; the ints big enough to look
            // like nothing in the heap
            if (i % 2) {
                holder->value = 0x10000 + i;
            } else {
                holder->value = make_tracked<Node>(i);
            }
            holder->next = list;
            list = holder;
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + Count / 2);
    int seen = 0;
    off_frame([&] {                         // the walk leaves pointers into the holders in its frame
        for (auto h = list; h; h = h->next) {
            if (auto p = get_if<tracked_ptr<Node>>(&h->value)) {
                EXPECT_EQ((*p)->value % 2, 0);
                ++seen;
            }
        }
    });
    EXPECT_EQ(seen, Count / 2);
    list = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Variant_Tests, ThePairAlternativeIsFollowedAndTheWeakOneIsNot) {
    settle();
    const int before = Node::alive.load();
    tracked_ptr<Holder> pair, weak;
    off_frame([&] {
        pair = make_tracked<Holder>();
        weak = make_tracked<Holder>();
        pair->value = Pair{make_tracked<Node>(1), 0x20000};
        weak->value = weak_ptr<Node>(tracked_ptr(make_tracked<Node>(2)));
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    off_frame([&] {                         // the reads in a frame of their own: what they spill is cleared
        EXPECT_EQ(get<Pair>(pair->value).node->value, 1);
        EXPECT_TRUE(get<weak_ptr<Node>>(weak->value).expired());
        pair->value = 5;                    // the pointer alternative destroyed: the Node goes
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Variant_Tests, InsideTheContainers) {
    settle();
    const int before = Node::alive.load();
    vector<Value> values;
    map<int, Value> by_key;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            values.push_back(i % 2 ? Value(0x30000 + i) : Value(make_tracked<Node>(i)));
            by_key[i] = i % 2 ? Value(std::string("s")) : Value(make_tracked<Node>(i));
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 100);
    for (int i = 0; i < 100; i += 2) {
        EXPECT_EQ(get<tracked_ptr<Node>>(values[i])->value, i);
        EXPECT_EQ(get<tracked_ptr<Node>>(by_key[i])->value, i);
    }
    values.clear();
    by_key.clear();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

namespace {
    struct Throws {
        Throws() = default;
        Throws(int) { throw 1; }
        Throws(const Throws&) = default;
        Throws& operator=(const Throws&) = default;
        Throws(Throws&&) noexcept = default;
        Throws& operator=(Throws&&) noexcept = default;
    };
}

TEST(Variant_Tests, ValuelessAfterAThrowingEmplace) {
    variant<tracked_ptr<Node>, Throws> v = make_tracked<Node>(1);
    EXPECT_THROW(v.emplace<Throws>(1), int);
    EXPECT_TRUE(v.valueless_by_exception());
    EXPECT_EQ(v.index(), variant_npos);
    EXPECT_THROW(visit([](auto&) {}, v), bad_variant_access);
    v = make_tracked<Node>(2);
    EXPECT_EQ(get<0>(v)->value, 2);
}

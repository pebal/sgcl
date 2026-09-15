//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// expected: std::expected's interface over a variant, the tracked pointer
// value or error kept apart from the other's data.
#include "types.h"

#include <string>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static std::atomic<int> alive = {0};
    };

    using Result = expected<tracked_ptr<Node>, std::string>;

    Result parse(int x) {
        if (x < 0) {
            return unexpected("negative");
        }
        return make_tracked<Node>(x);
    }

    struct Holder {
        expected<tracked_ptr<Node>, int> result;
        tracked_ptr<Holder> next;
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Expected_Tests, TheInterfaceOfStdExpected) {
    static_assert(std::is_same_v<Result::value_type, tracked_ptr<Node>> && std::is_same_v<Result::error_type, std::string>);
    static_assert(std::is_same_v<Result::rebind<int>, expected<int, std::string>>);
    Result a = parse(1);
    Result b = parse(-1);
    ASSERT_TRUE(a);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(b);
    EXPECT_EQ((*a)->value, 1);
    EXPECT_EQ(a->get()->value, 1);
    EXPECT_EQ(a.value()->value, 1);
    EXPECT_EQ(b.error(), "negative");
    EXPECT_THROW(b.value(), bad_expected_access<std::string>);
    try {
        b.value();
    } catch (const bad_expected_access<std::string>& e) {
        EXPECT_EQ(e.error(), "negative");
        EXPECT_NE(std::string(e.what()).find("expected"), std::string::npos);
    }
    EXPECT_EQ(b.value_or(nullptr), nullptr);
    EXPECT_EQ(a.value_or(nullptr), *a);
    EXPECT_EQ(a.error_or("ok"), "ok");
    EXPECT_EQ(b.error_or("ok"), "negative");
    Result c(std::in_place, make_tracked<Node>(2));
    EXPECT_EQ((*c)->value, 2);
    Result d(unexpect, "x");
    EXPECT_EQ(d.error(), "x");
    Result e(std::in_place);
    EXPECT_EQ(*e, nullptr);
    Result copy = a;
    EXPECT_TRUE(copy == a);
    EXPECT_TRUE(copy != b);
    EXPECT_TRUE(b == unexpected(std::string("negative")));
    EXPECT_TRUE(a == *a);
    Result moved = std::move(copy);
    EXPECT_EQ((*moved)->value, 1);
    moved = unexpected(std::string("y"));
    EXPECT_EQ(moved.error(), "y");
    moved = make_tracked<Node>(3);
    EXPECT_EQ((*moved)->value, 3);
    EXPECT_EQ(moved.emplace(make_tracked<Node>(4))->value, 4);
    swap(moved, d);
    EXPECT_EQ((*d)->value, 4);
    EXPECT_EQ(moved.error(), "x");
    expected<int, std::string> converted = expected<short, const char*>(5);
    EXPECT_EQ(*converted, 5);
    expected<int, std::string> converted_error = expected<short, const char*>(unexpect, "z");
    EXPECT_EQ(converted_error.error(), "z");
}

TEST(Expected_Tests, TheMonadicOperations) {
    Result a = parse(1);
    Result b = parse(-1);
    auto plus = [](const tracked_ptr<Node>& n) -> expected<int, std::string> { return n->value + 1; };
    EXPECT_EQ(a.and_then(plus), 2);
    EXPECT_EQ(b.and_then(plus).error(), "negative");
    auto recover = [](const std::string& s) -> Result { return make_tracked<Node>((int)s.size()); };
    EXPECT_EQ((*b.or_else(recover))->value, 8);
    EXPECT_EQ(a.or_else(recover), a);
    auto tenfold = a.transform([](const tracked_ptr<Node>& n) { return n->value * 10; });
    static_assert(std::is_same_v<decltype(tenfold), expected<int, std::string>>);
    EXPECT_EQ(*tenfold, 10);
    auto nothing = a.transform([](const tracked_ptr<Node>&) {});
    static_assert(std::is_same_v<decltype(nothing), expected<void, std::string>>);
    EXPECT_TRUE(nothing);
    auto length = b.transform_error([](const std::string& s) { return (int)s.size(); });
    static_assert(std::is_same_v<decltype(length), expected<tracked_ptr<Node>, int>>);
    EXPECT_EQ(length.error(), 8);
    EXPECT_EQ(std::move(a).and_then(plus), 2);
}

TEST(Expected_Tests, TheVoidValue) {
    expected<void, int> ok;
    EXPECT_TRUE(ok);
    ok.value();
    *ok;
    expected<void, int> failed = unexpected(3);
    EXPECT_FALSE(failed);
    EXPECT_EQ(failed.error(), 3);
    EXPECT_THROW(failed.value(), bad_expected_access<int>);
    EXPECT_EQ(failed.error_or(0), 3);
    EXPECT_EQ(ok.error_or(0), 0);
    failed.emplace();
    EXPECT_TRUE(failed);
    failed = unexpected(4);
    EXPECT_TRUE(failed == unexpected(4));
    EXPECT_TRUE(ok != failed);
    auto next = failed.or_else([](int e) -> expected<void, int> { return e == 4 ? expected<void, int>() : unexpected(e); });
    EXPECT_TRUE(next);
    auto value = ok.transform([] { return 5; });
    EXPECT_EQ(*value, 5);
    auto chained = ok.and_then([]() -> expected<int, int> { return 6; });
    EXPECT_EQ(*chained, 6);
    auto error = failed.transform_error([](int e) { return std::to_string(e); });
    EXPECT_EQ(error.error(), "4");
    expected<void, std::string> widened = failed.transform_error([](int e) { return std::to_string(e); });
    EXPECT_EQ(widened.error(), "4");
    swap(ok, failed);
    EXPECT_FALSE(ok);
}

TEST(Expected_Tests, ThePointerValueIsFollowedNextToErrors) {
    settle();
    const int before = Node::alive.load();
    constexpr int Count = 1000;
    tracked_ptr<Holder> list;
    off_frame([&] {
        for (int i = 0; i < Count; ++i) {
            tracked_ptr holder = make_tracked<Holder>();
            if (i % 2) {
                holder->result = unexpected(0x10000 + i);
            } else {
                holder->result = make_tracked<Node>(i);
            }
            holder->next = list;
            list = holder;
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + Count / 2);
    int seen = 0;
    off_frame([&] {
        for (auto h = list; h; h = h->next) {
            if (h->result) {
                EXPECT_EQ((*h->result)->value % 2, 0);
                ++seen;
            }
        }
    });
    EXPECT_EQ(seen, Count / 2);
    list = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Expected_Tests, TheGcKindLivesAnywhere) {
    settle();
    const int before = Node::alive.load();
    auto* results = new std::vector<gc::expected<gc::tracked_ptr<Node>, std::string>>();
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            if (i % 2) {
                results->push_back(gc::unexpected(std::string("odd")));
            } else {
                results->push_back(gc::make_tracked<Node>(i));
            }
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 50);
    for (int i = 0; i < 100; i += 2) {
        EXPECT_EQ((*(*results)[i])->value, i);
    }
    delete results;
    detail::cell_allocator.release();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Aliases_Tests, TheStandardTypesSafeWithAPointerUnderTheLibrarysNames) {
    static_assert(std::is_same_v<sgcl::optional<int>, std::optional<int>>);
    static_assert(std::is_same_v<gc::optional<gc::tracked_ptr<Node>>, std::optional<gc::tracked_ptr<Node>>>);
    static_assert(std::is_same_v<gc::pair<int, gc::tracked_ptr<Node>>, std::pair<int, gc::tracked_ptr<Node>>>);
    static_assert(std::is_same_v<gc::tuple<gc::tracked_ptr<Node>>, std::tuple<gc::tracked_ptr<Node>>>);
    settle();
    const int before = Node::alive.load();
    struct Bag {
        optional<tracked_ptr<Node>> maybe;
        pair<int, tracked_ptr<Node>> pair;
        tuple<tracked_ptr<Node>, int> tuple;
    };
    tracked_ptr<Bag> bag;
    off_frame([&] {
        bag = make_tracked<Bag>();
        bag->maybe = make_tracked<Node>(1);
        bag->pair = {2, make_tracked<Node>(2)};
        bag->tuple = {make_tracked<Node>(3), 3};
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 3);
    off_frame([&] {
        EXPECT_EQ((*bag->maybe)->value, 1);
        EXPECT_EQ(bag->pair.second->value, 2);
        EXPECT_EQ(std::get<0>(bag->tuple)->value, 3);
        bag->maybe = nullopt;
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 2);
    bag = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

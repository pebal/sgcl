//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// expected: std::expected's interface over a variant, the tracked pointer
// value or error kept apart from the other's data.
// The accesses of an expected are what this file tests (a value read
// where an error is, on purpose, among them): the rule of
// tests/expected_access.h, a check before a read, does not apply here.
#include "tests/types.h"

#include <string>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
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
    ASSERT_FALSE(b);
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
    // an error holding a tracked pointer: thrown and caught, the string alive through the exception's root
    sgcl::expected<int, sgcl::string> tracked(sgcl::unexpect, "held");
    try {
        tracked.value();
        FAIL();
    } catch (const bad_expected_access<sgcl::string>& e) {
        collector::force_collect(true);
        EXPECT_EQ(e.error(), "held");
        auto copy = e;                                  // a copy of the exception: its own root to the same string
        EXPECT_EQ(copy.error(), "held");
    }
    EXPECT_EQ(b.value_or(nullptr), nullptr);
    EXPECT_EQ(a.value_or(nullptr), *a);
    EXPECT_EQ(a.error_or("ok"), "ok");
    EXPECT_EQ(b.error_or("ok"), "negative");
    Result c(std::in_place, make_tracked<Node>(2));
    EXPECT_EQ((*c)->value, 2);
    Result d(unexpect, "x");
    ASSERT_FALSE(d);
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
    ASSERT_FALSE(moved);
    EXPECT_EQ(moved.error(), "y");
    moved = make_tracked<Node>(3);
    EXPECT_EQ((*moved)->value, 3);
    EXPECT_EQ(moved.emplace(make_tracked<Node>(4))->value, 4);
    swap(moved, d);
    EXPECT_EQ((*d)->value, 4);
    ASSERT_FALSE(moved);
    EXPECT_EQ(moved.error(), "x");
    expected<int, std::string> converted = expected<short, const char*>(5);
    EXPECT_EQ(*converted, 5);
    expected<int, std::string> converted_error = expected<short, const char*>(unexpect, "z");
    ASSERT_FALSE(converted_error);
    EXPECT_EQ(converted_error.error(), "z");
}

TEST(Expected_Tests, TheMonadicOperations) {
    Result a = parse(1);
    Result b = parse(-1);
    auto plus = [](const tracked_ptr<Node>& n) -> expected<int, std::string> { return n->value + 1; };
    EXPECT_EQ(a.and_then(plus), 2);
    EXPECT_EQ(error_of(b.and_then(plus)), "negative");
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
    ASSERT_FALSE(length);
    EXPECT_EQ(length.error(), 8);
    EXPECT_EQ(std::move(a).and_then(plus), 2);
}

TEST(Expected_Tests, TheVoidValue) {
    expected<void, int> ok;
    EXPECT_TRUE(ok);
    ok.value();
    *ok;
    expected<void, int> failed = unexpected(3);
    ASSERT_FALSE(failed);
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
    ASSERT_FALSE(error);
    EXPECT_EQ(error.error(), "4");
    expected<void, std::string> widened = failed.transform_error([](int e) { return std::to_string(e); });
    ASSERT_FALSE(widened);
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

namespace {
    // A value whose copy and move throw while armed; its move is not
    // noexcept, so the int on the other side is what moves out and back
    struct Thrower {
        Thrower() = default;
        explicit Thrower(int v) noexcept : value(v) {}
        Thrower(const Thrower& o) : value(o.value) { if (armed) throw 1; }
        Thrower(Thrower&& o) : value(o.value) { if (armed) throw 2; }
        Thrower& operator=(const Thrower&) = default;
        Thrower& operator=(Thrower&&) = default;
        int value = 0;
        inline static bool armed = false;
    };

    // The same, moving without throwing: the new value is made in a
    // temporary first
    struct NothrowMovable {
        NothrowMovable() = default;
        explicit NothrowMovable(int v) noexcept : value(v) {}
        NothrowMovable(const NothrowMovable& o) : value(o.value) { if (armed) throw 3; }
        NothrowMovable(NothrowMovable&&) noexcept = default;
        NothrowMovable& operator=(const NothrowMovable&) = default;
        NothrowMovable& operator=(NothrowMovable&&) noexcept = default;
        int value = 0;
        inline static bool armed = false;
    };

    struct Arm {
        explicit Arm(bool& flag) : _flag(flag) { _flag = true; }
        ~Arm() { _flag = false; }
        bool& _flag;
    };

    // Whether e.emplace(a...) is offered (a requires-expression tells only
    // inside a template)
    template<class Ex, class... A>
    constexpr bool can_emplace = requires(Ex& e, A... a) { e.emplace(std::forward<A>(a)...); };
}

TEST(Expected_Tests, AThrowingAssignmentLeavesTheOldErrorNotNothing) {
    // the value's construction throws after the error moved out: the error put back
    expected<Thrower, int> e(unexpect, 7);
    Thrower t(3);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(e = t, int);
    }
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), 7);
    static_assert(noexcept(e.error()));
    e = t;                                                   // disarmed: the value takes the error's place
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->value, 3);
    // the new value is made in a temporary when it moves without throwing: nothing touched by the throw
    expected<NothrowMovable, int> n(unexpect, 8);
    NothrowMovable m(4);
    {
        Arm arm(NothrowMovable::armed);
        EXPECT_THROW(n = m, int);
    }
    ASSERT_FALSE(n.has_value());
    EXPECT_EQ(n.error(), 8);
    n = m;
    EXPECT_EQ(n->value, 4);
}

TEST(Expected_Tests, AThrowingAssignmentLeavesTheOldValueNotNothing) {
    // the error's construction throws after the value moved out: the value put back
    expected<int, Thrower> e(5);
    unexpected<Thrower> u(Thrower(9));
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(e = u, int);
    }
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(*e, 5);
    e = u;
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().value, 9);
    // between two expected: the copy of the other's value throws, the error stays
    expected<Thrower, int> a(unexpect, 1);
    expected<Thrower, int> b(std::in_place, 2);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(a = b, int);
    }
    ASSERT_FALSE(a.has_value());
    EXPECT_EQ(a.error(), 1);
    a = b;
    EXPECT_EQ(a->value, 2);
    // and the other way: the copy of the other's error throws, the value stays
    expected<int, Thrower> c(6);
    expected<int, Thrower> d(unexpect, 7);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(c = d, int);
    }
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*c, 6);
    // an assignment that could leave nothing behind is not offered: neither side moves without throwing
    struct Both {
        Both(const Both&) {}
        Both(Both&&) {}
        Both& operator=(const Both&) = default;
        Both& operator=(Both&&) = default;
    };
    static_assert(!std::is_copy_assignable_v<expected<Both, Both>>);
    static_assert(!std::is_move_assignable_v<expected<Both, Both>>);
    static_assert(std::is_copy_assignable_v<expected<Both, int>>);
    // the void expected: an error whose construction throws leaves the success
    expected<void, Thrower> v;
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(v = u, int);
    }
    EXPECT_TRUE(v.has_value());
    v = u;
    ASSERT_FALSE(v);
    EXPECT_EQ(v.error().value, 9);
}

TEST(Expected_Tests, EmplaceTakesOnlyAConstructionThatCannotThrow) {
    expected<Thrower, int> e(unexpect, 7);
    Thrower t(3);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(e = t, int);
    }
    EXPECT_EQ(e.emplace(4).value, 4);                        // after the throw: the int constructor is noexcept
    static_assert(noexcept(e.emplace(4)));
    static_assert(!can_emplace<expected<Thrower, int>, const Thrower&>);                  // a copy may throw
    static_assert(can_emplace<expected<NothrowMovable, int>, NothrowMovable&&>);
}

TEST(Expected_Tests, AThrowingSwapLeavesBothSidesAsTheyWere) {
    // a value against an error: the error moves out without throwing, the
    // value's move into its place throws, the error put back
    expected<Thrower, int> value(std::in_place, 3);
    expected<Thrower, int> error(unexpect, 7);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(value.swap(error), int);
    }
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->value, 3);
    ASSERT_FALSE(error.has_value());
    EXPECT_EQ(error.error(), 7);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(error.swap(value), int);                // the other way round: the same case
    }
    EXPECT_EQ(value->value, 3);
    EXPECT_EQ(error.error(), 7);
    swap(value, error);
    EXPECT_EQ(value.error(), 7);
    EXPECT_EQ(error->value, 3);
    // the value the one that moves without throwing: the error's move throws, the value put back
    expected<int, Thrower> v(5);
    expected<int, Thrower> e(unexpect, 9);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(v.swap(e), int);
    }
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 5);
    EXPECT_EQ(e.error().value, 9);
    v.swap(e);
    EXPECT_EQ(v.error().value, 9);
    EXPECT_EQ(*e, 5);
    // the void expected: the error's move throws, the success stays
    expected<void, Thrower> ok;
    expected<void, Thrower> failed(unexpect, 2);
    {
        Arm arm(Thrower::armed);
        EXPECT_THROW(ok.swap(failed), int);
    }
    EXPECT_TRUE(ok.has_value());
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().value, 2);
    ok.swap(failed);
    EXPECT_FALSE(ok.has_value());
    EXPECT_TRUE(failed.has_value());
}

TEST(Expected_Tests, ABoolValueIsNotMadeFromAnotherExpectedAsAWhole) {
    // LWG 3836: expected<int, int>(0) converts to true as a whole; the value is what converts
    expected<bool, int> direct(expected<int, int>(0));
    EXPECT_FALSE(*direct);
    expected<bool, int> copy_initialized = expected<int, int>(1);   // and the conversion is implicit
    EXPECT_TRUE(*copy_initialized);
    expected<bool, int> error = expected<int, int>(unexpect, 3);
    ASSERT_FALSE(error.has_value());
    EXPECT_EQ(error.error(), 3);
    expected<bool, int> from_value = false;
    EXPECT_FALSE(*from_value);
}

namespace {
    struct Message {
        std::string text;
        std::string message() const { return text; }
    };

    // A handle that takes any "stream" through a constrained template, as
    // io::reader does: expected<Source> reaches it only through the
    // conversion to any U the value converts to
    struct Source {
        int id;
    };

    struct Handle {
        template<class S>
        requires std::same_as<std::remove_cvref_t<S>, Source>
        Handle(S&& s) : id(s.id) {}
        int id;
    };

    int take_handle(const Handle& h) {
        return h.id;
    }

    // A type with a constructor of its own from an expected
    struct FromExpected {
        FromExpected(const expected<int, int>& e) : had_value(e.has_value()) {}
        bool had_value;
    };

    struct MoveOnly {
        explicit MoveOnly(int v) : value(v) {}
        MoveOnly(MoveOnly&&) = default;
        MoveOnly(const MoveOnly&) = delete;
        int value;
    };
}

TEST(Expected_Tests, TheValueGoesWhereATIsWanted) {
    expected<std::string, int> text = std::string("abc");
    std::string s = text;                          // copy-initialized
    std::string t(text);                           // direct
    EXPECT_EQ(s, "abc");
    EXPECT_EQ(t, "abc");
    // through a converting template the value alone reaches (io::reader's shape)
    expected<Source, int> source = Source{7};
    EXPECT_EQ(take_handle(source), 7);
    const Handle& bound = source;                  // a temporary, its life extended
    EXPECT_EQ(bound.id, 7);
    // comparisons stay the expected's own
    expected<int, int> n = 7;
    EXPECT_TRUE(n == 7);
    EXPECT_TRUE(7 == n);
    EXPECT_TRUE(text == "abc");
    // optional of the value; optional of the expected keeps it whole
    optional<std::string> o = text;
    EXPECT_EQ(*o, "abc");
    optional<expected<std::string, int>> whole = text;
    EXPECT_TRUE(whole->has_value());
    // a move-only value, moved out of an rvalue
    MoveOnly m = expected<MoveOnly, int>(std::in_place, 5);
    EXPECT_EQ(m.value, 5);
}

TEST(Expected_Tests, AnErrorUsedAsTheValueIsThrownWithItsMessage) {
    expected<std::string, Message> failed = unexpected(Message{"open log.gz: No such file or directory"});
    try {
        std::string s = failed;
        ADD_FAILURE() << "no exception";
    } catch (const bad_expected_access<Message>& e) {
        EXPECT_EQ(e.error().text, "open log.gz: No such file or directory");
        EXPECT_STREQ(e.what(), "open log.gz: No such file or directory");
    }
    EXPECT_THROW((void)*failed, bad_expected_access<Message>);
    EXPECT_THROW((void)failed->size(), bad_expected_access<Message>);
    expected<Source, int> none = unexpected(3);
    EXPECT_THROW((void)take_handle(none), bad_expected_access<int>);
    // an error without message(): the general text
    try {
        (void)*none;
    } catch (const bad_expected_access<int>& e) {
        EXPECT_STREQ(e.what(), "bad access to sgcl::expected without a value");
    }
}

// The exception makes the error's message when what() asks for it, not
// when it is made: a throw nobody reads the text of allocates the rooted
// error and no string (message() made a managed string at every throw)
TEST(Expected_Tests, TheExceptionMakesItsMessageOnlyWhenAsked) {
    const number_error e(number_error::reason::not_a_number, 3);
    const size_t rooted_error = managed_bytes_of(20000, [&] {
        rooted<number_error> r(e);
        EXPECT_EQ(r->offset(), 3u);
    });
    const size_t exception = managed_bytes_of(20000, [&] {
        bad_expected_access<number_error> x(e);
        EXPECT_EQ(x.error().offset(), 3u);
    });
    EXPECT_LE(exception, rooted_error + config::page_size);   // a page of slack for where the pages of either start
    bad_expected_access<number_error> x(e);
    auto copy = x;                                               // a copy asks on its own
    EXPECT_STREQ(x.what(), "not a number");
    EXPECT_EQ(x.what(), x.what());                               // made once: the same text again
    EXPECT_STREQ(copy.what(), "not a number");
    bad_expected_access<number_error> assigned(number_error(number_error::reason::empty, 0));
    EXPECT_STREQ(assigned.what(), "an empty text");
    assigned = x;                                                // the text follows the error it now holds
    EXPECT_STREQ(assigned.what(), "not a number");
    expected<int, number_error> failed = unexpected(e);
    try {
        (void)failed.value();
        ADD_FAILURE() << "no exception";
    } catch (const std::exception& thrown) {
        EXPECT_STREQ(thrown.what(), "not a number");
    }
}

TEST(Expected_Tests, TheConversionLeavesBoolAndOtherExpectedsAlone) {
    // `if (e)` asks whether there is a value, for expected<bool> too
    expected<bool, int> no = false;
    EXPECT_TRUE(static_cast<bool>(no));
    static_assert(!std::is_convertible_v<expected<int, int>, bool>);
    static_assert(!std::is_convertible_v<expected<bool, int>, bool>);
    static_assert(!std::is_convertible_v<expected<bool, int>, int>);     // one meaning in a number's place: none
    static_assert(std::is_convertible_v<expected<int, int>, long>);
    // an expected converted to another carries its error, never throws it
    expected<int, int> e = unexpected(4);
    expected<long, int> l = e;
    ASSERT_FALSE(l.has_value());
    EXPECT_EQ(l.error(), 4);
    expected<long, int> lv = expected<int, int>(9);
    EXPECT_EQ(*lv, 9);
    expected<std::string, int> from_chars = expected<const char*, int>("xyz");
    EXPECT_EQ(*from_chars, "xyz");
    // an unexpected is never made of an expected whole
    static_assert(!std::is_constructible_v<unexpected<int>, expected<int, int>>);
    // a type with its own constructor from the other expected still uses it
    expected<int, int> err = unexpected(1);
    FromExpected fe = err;
    EXPECT_FALSE(fe.had_value);
}

TEST(Aliases_Tests, TheStandardTypesSafeWithAPointerUnderTheLibrarysNames) {
    static_assert(std::is_same_v<sgcl::optional<int>, std::optional<int>>);
    static_assert(std::is_same_v<sgcl::pair<int, tracked_ptr<Node>>, std::pair<int, tracked_ptr<Node>>>);
    static_assert(std::is_same_v<sgcl::tuple<tracked_ptr<Node>>, std::tuple<tracked_ptr<Node>>>);
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

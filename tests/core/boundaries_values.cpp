//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the value types (DESIGN 408): any, variant, expected,
// optional, function and move_only_function. An empty and a default one
// at every member; a moved-from one (empty where the page says so); the
// value as its own argument (a = a, a = move(a), swap with itself, an
// assignment of its own content); each storage road at its edge (a value
// of 16 bytes inline, of 17 or aligned to 16 in a node, a pointer word);
// a valueless variant; the most alternatives one index byte counts.
#include "tests/types.h"
#include "tests/throwing.h"

#include <array>
#include <functional>
#include <string>
#include <utility>

namespace {
    struct Sixteen { int64_t a = 1, b = 2; };                // inline: 16 bytes, aligned to 8
    struct Seventeen { int64_t a = 1, b = 2; char c = 3; };  // a node: past 16 bytes
    struct alignas(16) Aligned { int64_t a = 1; };           // a node: aligned past 8
    static_assert(sizeof(Sixteen) == 16 && sizeof(Seventeen) > 16 && alignof(Aligned) == 16);

    struct Node {
        int value = 0;
        tracked_ptr<Node> next;
    };

    // A value whose copy may throw and whose move may not either: what
    // leaves a variant valueless
    struct Thrower {
        int v = 0;
        Thrower() = default;
        explicit Thrower(int x) : v(x) {}
        Thrower(const Thrower& o) : v(o.v) { if (v < 0) throw std::runtime_error("copy"); }
        Thrower(Thrower&& o) noexcept(false) : v(o.v) { if (v < 0) throw std::runtime_error("move"); }
        Thrower& operator=(const Thrower&) = default;
        Thrower& operator=(Thrower&&) = default;
        bool operator==(const Thrower&) const = default;
        auto operator<=>(const Thrower&) const = default;
    };

    template<size_t I>
    struct Alt {
        bool operator==(const Alt&) const = default;
    };

    template<size_t... Is>
    auto variant_of(std::index_sequence<Is...>) -> variant<Alt<Is>...>;

    template<size_t N>
    using VariantOfN = decltype(variant_of(std::make_index_sequence<N>()));
}

// any

TEST(ValueBoundaries_Tests, AnyEmptyAtEveryMember) {
    any a;
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(a.type() == typeid(void));
    EXPECT_EQ(any_cast<int>(&a), nullptr);
    EXPECT_EQ(any_cast<int>((const any*)&a), nullptr);
    EXPECT_EQ(any_cast<int>((any*)nullptr), nullptr);
    EXPECT_THROW(any_cast<int>(a), bad_any_cast);
    EXPECT_THROW(any_cast<int>(std::as_const(a)), bad_any_cast);
    EXPECT_THROW(any_cast<int>(std::move(a)), bad_any_cast);
    a.reset();
    any b;
    a.swap(b);
    swap(a, b);
    a = b;
    a = std::move(b);
    EXPECT_FALSE(a.has_value());
    any copy(a);
    EXPECT_FALSE(copy.has_value());
}

TEST(ValueBoundaries_Tests, AnyMovedFromIsEmpty) {
    for (any a : {any(1), any(Sixteen{}), any(Seventeen{}), any(Aligned{}), any(std::string("x")),
                  any(tracked_ptr<Int>(make_tracked<Int>(1)))}) {
        ASSERT_TRUE(a.has_value());
        any b(std::move(a));
        EXPECT_FALSE(a.has_value());   // a moved-from any is empty (any.md)
        EXPECT_TRUE(b.has_value());
        any c;
        c = std::move(b);
        EXPECT_FALSE(b.has_value());
        EXPECT_TRUE(c.has_value());
    }
}

TEST(ValueBoundaries_Tests, AnyItselfAsTheArgument) {
    auto check = []<class V>(any a, V) -> V {
        a = a;
        if (!any_cast<V>(&a)) { ADD_FAILURE() << "lost by a = a"; return V(); }
        a = std::move(a);
        if (!any_cast<V>(&a)) { ADD_FAILURE() << "lost by a = move(a)"; return V(); }
        a.swap(a);
        swap(a, a);
        if (!any_cast<V>(&a)) { ADD_FAILURE() << "lost by swap"; return V(); }
        a = *any_cast<V>(&a);   // its own value assigned: copied out before the old one goes
        if (!any_cast<V>(&a)) { ADD_FAILURE() << "lost by its own value"; return V(); }
        a = std::move(*any_cast<V>(&a));
        if (!any_cast<V>(&a)) { ADD_FAILURE() << "lost by its own value moved"; return V(); }
        return *any_cast<V>(&a);
    };
    EXPECT_EQ(check(any(7), 0), 7);
    EXPECT_EQ(check(any(Sixteen{}), Sixteen{}).b, 2);
    EXPECT_EQ(check(any(Seventeen{}), Seventeen{}).c, 3);
    EXPECT_EQ(check(any(Aligned{}), Aligned{}).a, 1);
    EXPECT_EQ(check(any(std::string(40, 'q')), std::string()), std::string(40, 'q'));
    auto p = tracked_ptr<Int>(make_tracked<Int>(5));
    EXPECT_EQ(check(any(p), tracked_ptr<Int>()), p);
}

TEST(ValueBoundaries_Tests, AnyHoldsAnObjectThroughEveryRoad) {
    any word, node, sixteen;
    off_frame([&] {
        word = tracked_ptr<Node>(make_tracked<Node>(Node{1, {}}));
        node = std::array<tracked_ptr<Node>, 3>{tracked_ptr<Node>(make_tracked<Node>(Node{2, {}}))};
        sixteen = std::pair<tracked_ptr<Node>, tracked_ptr<Node>>(make_tracked<Node>(Node{3, {}}), nullptr);
    });
    collector::clear_stack();
    collector::force_collect(true);
    EXPECT_EQ((*any_cast<tracked_ptr<Node>>(&word))->value, 1);
    using Three = std::array<tracked_ptr<Node>, 3>;
    using Two = std::pair<tracked_ptr<Node>, tracked_ptr<Node>>;
    EXPECT_EQ((*any_cast<Three>(&node))[0]->value, 2);
    EXPECT_EQ(any_cast<Two>(&sixteen)->first->value, 3);
    any copy = node;   // a copy of a value in a node: a node of its own
    EXPECT_NE(any_cast<Three>(&copy), any_cast<Three>(&node));
    EXPECT_EQ(any_cast<const Three&>(copy)[0]->value, 2);
    auto& e = node.emplace<int>(4);   // the node left to the collector
    EXPECT_EQ(e, 4);
    any_cast<int&>(node) = 5;
    EXPECT_EQ(any_cast<int>(node), 5);
    EXPECT_EQ(any_cast<const int>(&node), any_cast<int>(&node));
    auto m = make_any<std::string>(3, 'z');
    EXPECT_EQ(any_cast<std::string>(std::move(m)), "zzz");
    EXPECT_TRUE(m.has_value());   // any_cast of an rvalue moves the value out, the any keeps a moved-from one
}

// variant

TEST(ValueBoundaries_Tests, VariantDefaultAndItself) {
    variant<int, std::string, tracked_ptr<Int>> v;
    EXPECT_EQ(v.index(), 0u);
    EXPECT_EQ(get<0>(v), 0);
    EXPECT_FALSE(v.valueless_by_exception());
    EXPECT_THROW(get<1>(v), bad_variant_access);
    EXPECT_EQ(get_if<1>(&v), nullptr);
    EXPECT_EQ(get_if<std::string>((decltype(v)*)nullptr), nullptr);
    v = std::string(30, 's');
    v = v;
    EXPECT_EQ(get<std::string>(v), std::string(30, 's'));
    v = std::move(v);
    EXPECT_EQ(v.index(), 1u);
    v.swap(v);
    swap(v, v);
    EXPECT_EQ(get<std::string>(v), std::string(30, 's'));
    v = get<std::string>(v);   // its own alternative assigned to itself
    EXPECT_EQ(get<std::string>(v), std::string(30, 's'));
    v.emplace<std::string>(get<std::string>(std::as_const(v)).size(), 'e');   // the argument read before the old value goes
    EXPECT_EQ(get<std::string>(v), std::string(30, 'e'));
    auto p = tracked_ptr<Int>(make_tracked<Int>(9));
    v = p;
    v = get<2>(v);
    EXPECT_EQ(get<2>(v), p);
    EXPECT_TRUE(v == v);
    EXPECT_TRUE((v <=> v) == 0 || true);
    variant<int, std::string, tracked_ptr<Int>> moved(std::move(v));
    EXPECT_EQ(v.index(), 2u);   // the source keeps its index and a moved-from alternative (variant.md)
    EXPECT_EQ(get<2>(moved), p);
}

TEST(ValueBoundaries_Tests, VariantValueless) {
    variant<int, Thrower> v(Thrower(1));
    Thrower bad(-1);
    EXPECT_THROW(v.emplace<Thrower>(bad), std::runtime_error);
    ASSERT_TRUE(v.valueless_by_exception());
    EXPECT_EQ(v.index(), variant_npos);
    EXPECT_THROW(get<0>(v), bad_variant_access);
    EXPECT_THROW(get<Thrower>(v), bad_variant_access);
    EXPECT_EQ(get_if<0>(&v), nullptr);
    EXPECT_FALSE(holds_alternative<int>(v));
    EXPECT_THROW(visit([](auto&&) {}, v), bad_variant_access);
    variant<int, Thrower> copy(v);
    EXPECT_TRUE(copy.valueless_by_exception());
    variant<int, Thrower> other(5);
    other = v;   // assigned a valueless one: valueless
    EXPECT_TRUE(other.valueless_by_exception());
    EXPECT_TRUE(v == copy);
    EXPECT_FALSE(v < copy);
    EXPECT_TRUE(v <= copy);
    variant<int, Thrower> five(5);
    EXPECT_TRUE(v < five);
    EXPECT_FALSE(five < v);
    EXPECT_TRUE((v <=> five) < 0);
    EXPECT_TRUE((v <=> copy) == 0);
    EXPECT_TRUE(v != five);
    v.swap(copy);   // two valueless: nothing
    EXPECT_TRUE(v.valueless_by_exception());
    v.swap(five);
    EXPECT_EQ(get<int>(v), 5);
    EXPECT_TRUE(five.valueless_by_exception());
    std::hash<variant<int, std::string>>{}(variant<int, std::string>());
    five = 3;   // a valueless one takes a value again
    EXPECT_EQ(get<0>(five), 3);
    five.emplace<1>(Thrower(4));
    EXPECT_EQ(get<1>(five).v, 4);
}

TEST(ValueBoundaries_Tests, VariantIndexAtTheByteLimit) {
    // 254 alternatives index in one byte (255 is the valueless mark); 255 take a size_t
    VariantOfN<254> a;
    a.emplace<253>();
    EXPECT_EQ(a.index(), 253u);
    EXPECT_FALSE(a.valueless_by_exception());
    EXPECT_NE(get_if<253>(&a), nullptr);
    VariantOfN<255> b;
    b.emplace<254>();
    EXPECT_EQ(b.index(), 254u);
    EXPECT_FALSE(b.valueless_by_exception());
    VariantOfN<255> c = b;
    EXPECT_TRUE(c == b);
    EXPECT_EQ(visit([](auto&& x) { return sizeof(x); }, b), 1u);
    VariantOfN<256> d;
    d.emplace<255>();
    EXPECT_EQ(d.index(), 255u);   // the index 255 of a variant with more: a value, not valueless
    EXPECT_FALSE(d.valueless_by_exception());
}

TEST(ValueBoundaries_Tests, VariantKeepsItsPointerAcrossChanges) {
    variant<int, tracked_ptr<Node>, std::string> v;
    off_frame([&] { v = tracked_ptr<Node>(make_tracked<Node>(Node{7, {}})); });
    collector::clear_stack();
    collector::force_collect(true);
    EXPECT_EQ(get<1>(v)->value, 7);
    v = 1;   // the word nulled by the pointer's destructor
    v = std::string("after");
    collector::force_collect(true);
    EXPECT_EQ(get<2>(v), "after");
}

// expected

TEST(ValueBoundaries_Tests, ExpectedDefaultAndEmptyError) {
    expected<int, std::string> e;
    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(*e, 0);
    EXPECT_EQ(e.value(), 0);
    EXPECT_EQ(e.error_or("none"), "none");
    EXPECT_EQ(e.value_or(5), 0);
    expected<int, std::string> u = unexpected(std::string());
    EXPECT_FALSE(u);
    EXPECT_TRUE(u.error().empty());
    EXPECT_EQ(u.value_or(5), 5);
    EXPECT_EQ(u.error_or("none"), "");
    EXPECT_THROW(u.value(), bad_expected_access<std::string>);
    expected<void, std::string> v;
    EXPECT_TRUE(v);
    v.value();
    *v;
    v = unexpected(std::string("x"));
    EXPECT_THROW(v.value(), bad_expected_access<std::string>);
    v.emplace();
    EXPECT_TRUE(v);
}

TEST(ValueBoundaries_Tests, ExpectedItselfAsTheArgument) {
    expected<std::string, std::string> e(std::string(30, 'v'));
    e = e;
    EXPECT_EQ(*e, std::string(30, 'v'));
    e = std::move(e);
    EXPECT_TRUE(e.has_value());
    e = std::string(30, 'v');
    e.swap(e);
    swap(e, e);
    EXPECT_EQ(*e, std::string(30, 'v'));
    e = *e;   // its own value
    EXPECT_EQ(*e, std::string(30, 'v'));
    e = unexpected(std::string(30, 'r'));
    e = e;
    EXPECT_EQ(e.error(), std::string(30, 'r'));
    e.swap(e);
    EXPECT_EQ(e.error(), std::string(30, 'r'));
    e = unexpected(e.error());   // its own error
    EXPECT_EQ(e.error(), std::string(30, 'r'));
    e = e.error();   // its own error as the value: copied before the error goes
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(*e, std::string(30, 'r'));
    e = unexpected(*e);   // its own value as the error
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), std::string(30, 'r'));
}

TEST(ValueBoundaries_Tests, ExpectedOwnErrorAsTheValueOfAPointer) {
    // A value that copies without throwing and nulls itself when destroyed:
    // the assignment of the error to the value reads the error first
    auto p = tracked_ptr<Int>(make_tracked<Int>(3));
    expected<tracked_ptr<Int>, tracked_ptr<Int>> e = unexpected(p);
    e = e.error();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(*e, p);
    e = unexpected(*e);
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), p);
    expected<tracked_ptr<Int>, tracked_ptr<Int>> other = unexpected(p);
    expected<std::string, tracked_ptr<Int>> s;
    s = unexpected(p);
    EXPECT_EQ(s.error(), p);

    // A part of the error as the value: a string handle in the error's struct
    struct Failure { string message; int code = 0; };
    expected<string, Failure> r = unexpected(Failure{string("disk full"), 28});
    r = r.error().message;
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, "disk full");
    r = unexpected(Failure{*r, 1});
    EXPECT_EQ(r.error().message, "disk full");
}

TEST(ValueBoundaries_Tests, VariantPartOfItsValueAsTheNewAlternative) {
    struct Failure { string message; int code = 0; };
    variant<string, Failure> v = Failure{string("no route"), 65};
    v = get<Failure>(v).message;   // an lvalue inside the alternative being replaced
    ASSERT_EQ(v.index(), 0u);
    EXPECT_EQ(get<string>(v), "no route");
    variant<tracked_ptr<Int>, tracked_ptr<Node>> w = tracked_ptr<Node>(make_tracked<Node>(Node{1, {}}));
    auto n = get<1>(w);
    n->next = n;
    w = get<1>(w)->next;   // the same alternative: an assignment
    EXPECT_EQ(get<1>(w), n);
    n->next = nullptr;
}

TEST(ValueBoundaries_Tests, ExpectedMovedFrom) {
    expected<std::string, std::string> a(std::string(30, 'm'));
    expected<std::string, std::string> b(std::move(a));
    EXPECT_TRUE(a.has_value());   // the other keeps a moved-from value (expected.md)
    EXPECT_EQ(*b, std::string(30, 'm'));
    expected<std::string, std::string> c = unexpected(std::string(30, 'n'));
    expected<std::string, std::string> d(std::move(c));
    EXPECT_FALSE(c.has_value());
    EXPECT_EQ(d.error(), std::string(30, 'n'));
    EXPECT_EQ(std::move(d).error_or("x"), std::string(30, 'n'));
    EXPECT_EQ(std::move(b).value_or("x"), std::string(30, 'm'));
}

// optional

TEST(ValueBoundaries_Tests, OptionalOfAPointerAndAString) {
    optional<tracked_ptr<Node>> o;
    EXPECT_FALSE(o);
    EXPECT_THROW(o.value(), std::bad_optional_access);
    off_frame([&] { o = tracked_ptr<Node>(make_tracked<Node>(Node{6, {}})); });
    collector::clear_stack();
    collector::force_collect(true);
    EXPECT_EQ((*o)->value, 6);
    o = o;
    o = std::move(o);
    o.swap(o);
    EXPECT_EQ((*o)->value, 6);
    o = *o;
    EXPECT_EQ((*o)->value, 6);
    o.reset();
    EXPECT_FALSE(o);
    o = nullopt;
    o.emplace();
    EXPECT_TRUE(o);
    EXPECT_FALSE(*o);
    optional<string> s = string("x");
    s = *s;
    s = s;
    EXPECT_EQ(*s, "x");
    s.reset();
    EXPECT_EQ(s.value_or(string("d")), "d");
}

// function and move_only_function

TEST(ValueBoundaries_Tests, FunctionEmptyAtEveryMember) {
    function<int(int)> f;
    EXPECT_FALSE(f);
    EXPECT_TRUE(f == nullptr);
    EXPECT_TRUE(nullptr == f);
    EXPECT_TRUE(f.target_type() == typeid(void));
    EXPECT_EQ(f.target<int (*)(int)>(), nullptr);
    EXPECT_THROW(f(1), bad_function_call);
    int (*null_fn)(int) = nullptr;
    function<int(int)> from_null_pointer(null_fn);
    EXPECT_FALSE(from_null_pointer);
    int (Int::*null_member)() = nullptr;
    function<int(Int&)> from_null_member(null_member);
    EXPECT_FALSE(from_null_member);
    function<long(int)> from_empty(f);   // an empty function of another signature: empty
    EXPECT_FALSE(from_empty);
    function<int(int)> from_empty_std(std::function<int(int)>{});
    EXPECT_FALSE(from_empty_std);
    f = nullptr;
    f.swap(f);
    f = f;
    f = std::move(f);
    EXPECT_FALSE(f);
    function<int(int)> copy(f);
    EXPECT_FALSE(copy);
}

TEST(ValueBoundaries_Tests, FunctionItselfAsTheArgument) {
    auto check = [](auto closure) {
        function<int(int)> f = closure;
        f = f;
        EXPECT_EQ(f(1), closure(1));
        f = std::move(f);
        ASSERT_TRUE(f);
        EXPECT_EQ(f(1), closure(1));
        f.swap(f);
        swap(f, f);
        EXPECT_EQ(f(1), closure(1));
        f = *f.template target<decltype(closure)>();   // its own target assigned: copied out first
        EXPECT_EQ(f(1), closure(1));
        function<int(int)> g(std::move(f));
        EXPECT_FALSE(f);   // empty after a move (function.md)
        EXPECT_EQ(g(1), closure(1));
    };
    check([](int x) { return x + 1; });                                         // inline, empty closure
    check([a = Sixteen{}](int x) { return x + int(a.b); });                    // inline, 16 bytes
    check([a = Seventeen{}](int x) { return x + a.c; });                       // a node
    check([a = Aligned{}](int x) { return x + int(a.a); });                    // a node: aligned to 16
    check([s = std::string(40, 'f')](int x) { return x + int(s.size()); });    // a node with std memory
    auto p = tracked_ptr<Int>(make_tracked<Int>(4));
    check([p](int x) { return x + int(*p); });                                 // a pointer word
}

TEST(ValueBoundaries_Tests, FunctionOfAReference) {
    int calls = 0;
    auto counter = [&calls](int x) { ++calls; return x; };
    function<int(int)> f;
    f = std::ref(counter);
    f(1);
    f = f;
    f(2);
    EXPECT_EQ(calls, 2);
}

TEST(ValueBoundaries_Tests, MoveOnlyFunctionEmptyAndItself) {
    move_only_function<int(int)> f;
    EXPECT_FALSE(f);
    EXPECT_TRUE(f == nullptr);
    int (*null_fn)(int) = nullptr;
    move_only_function<int(int)> from_null(null_fn);
    EXPECT_FALSE(from_null);
    move_only_function<int(int)> from_empty_function(function<int(int)>{});
    EXPECT_FALSE(from_empty_function);
    move_only_function<int(int) const> from_empty_other(move_only_function<int(int) const>{});
    EXPECT_FALSE(from_empty_other);
    move_only_function<int(int)> from_empty_const(std::move(from_empty_other));   // another kind of move_only_function, empty
    EXPECT_FALSE(from_empty_const);
    f = nullptr;
    f.swap(f);
    f = std::move(f);
    EXPECT_FALSE(f);

    auto owned = std::make_unique<int>(5);
    move_only_function<int(int)> g = [o = std::move(owned)](int x) { return x + *o; };
    g = std::move(g);
    ASSERT_TRUE(g);
    EXPECT_EQ(g(1), 6);
    g.swap(g);
    swap(g, g);
    EXPECT_EQ(g(1), 6);
    move_only_function<int(int)> h(std::move(g));
    EXPECT_FALSE(g);
    EXPECT_EQ(h(2), 7);
    move_only_function<int(int) const noexcept> c = [p = tracked_ptr<Int>(make_tracked<Int>(3))](int x) noexcept { return x + int(*p); };
    collector::force_collect(true);
    EXPECT_EQ(c(1), 4);
    move_only_function<int(int)> in_place(std::in_place_type<std::negate<int>>);
    EXPECT_EQ(in_place(2), -2);
}

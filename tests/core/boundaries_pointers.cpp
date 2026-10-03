//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the pointers (DESIGN 408): tracked_ptr, root_ptr,
// rooted, weak_ptr, unique_ptr, make_tracked and the casts. A null and a
// default one at every member, a moved-from one, the pointer as its own
// argument (a = a, a = move(a), swap with itself, reset to its own
// target), a base subobject at a nonzero offset (Foo's Bar: Far comes
// first), and what each page promises at those points.
#include "tests/types.h"

#include <functional>
#include <memory>

namespace {
    // A base at a nonzero offset of the object: the case a conversion that
    // only copies a word gets wrong
    bool bar_is_offset() {
        auto foo = make_tracked<Foo>(1);
        return static_cast<const void*>(static_cast<Bar*>(foo.get())) != static_cast<const void*>(foo.get());
    }

    struct Holder {
        int value = 5;
        tracked_ptr<Holder> self;
    };
}

// tracked_ptr

TEST(PointerBoundaries_Tests, TrackedNullAtEveryMember) {
    tracked_ptr<Foo> p;
    EXPECT_FALSE(p);
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_TRUE(p == nullptr);
    EXPECT_TRUE(nullptr == p);
    EXPECT_FALSE(p != nullptr);
    EXPECT_TRUE((p <=> nullptr) == 0);
    EXPECT_TRUE((nullptr <=> p) == 0);
    EXPECT_TRUE(p.type() == typeid(Foo));   // typeid(T) for a null pointer (type.md)
    EXPECT_FALSE(p.is<Bar>());
    EXPECT_FALSE(p.as<Bar>());
    EXPECT_FALSE(p.as<Foo>());
    EXPECT_FALSE(p.if_alive());
    EXPECT_FALSE(p.to_shared());
    EXPECT_EQ(std::hash<tracked_ptr<Foo>>{}(p), std::hash<Foo*>{}(nullptr));
    EXPECT_FALSE(static_pointer_cast<Bar>(p));
    EXPECT_FALSE(dynamic_pointer_cast<Baz>(tracked_ptr<Bar>(p)));
    EXPECT_FALSE(const_pointer_cast<const Foo>(p));
    p.reset();
    p.reset(nullptr);
    p.shade();
    EXPECT_FALSE(p);
    tracked_ptr<void> v = p;
    EXPECT_FALSE(v);
    EXPECT_TRUE(v.type() == typeid(void));
    tracked_ptr<Foo> q(nullptr);
    p.swap(q);
    EXPECT_FALSE(p);
    EXPECT_FALSE(q);
    tracked_ptr<Foo> r((Foo*)nullptr);
    EXPECT_FALSE(r);
    tracked_ptr<Foo> s(unique_ptr<Foo>{});
    EXPECT_FALSE(s);
    s = unique_ptr<Foo>{};
    EXPECT_FALSE(s);
    root_ptr<Foo> null_root;
    tracked_ptr<const Foo> t(null_root);
    EXPECT_FALSE(t);
}

// is<U>() of a null pointer is false for every U, the element type too:
// there is no object, so none was created as a U (DESIGN 418); type()
// stays typeid(T) (type.md), as<U>() null
TEST(PointerBoundaries_Tests, TrackedNullIsNoType) {
    tracked_ptr<Foo> def;
    tracked_ptr<Foo> assigned = make_tracked<Foo>(1);
    EXPECT_TRUE(assigned.is<Foo>());
    assigned = nullptr;
    tracked_ptr<Foo> null_source;
    tracked_ptr<Foo> moved_into(std::move(null_source));   // a move is a copy: null stays null
    for (const tracked_ptr<Foo>* p : {&def, &assigned, &null_source, &moved_into}) {
        EXPECT_FALSE(p->is<Foo>());
        EXPECT_FALSE(p->is<const Foo>());
        EXPECT_FALSE(p->is<Bar>());
        EXPECT_FALSE(p->is<Far>());
        EXPECT_FALSE(p->is<void>());
        EXPECT_TRUE(p->type() == typeid(Foo));
        EXPECT_FALSE(p->as<Foo>());
        EXPECT_FALSE(p->as<const Foo>());
        EXPECT_FALSE(p->as<Bar>());
    }
    tracked_ptr<const Foo> c;
    EXPECT_FALSE(c.is<Foo>());
    EXPECT_FALSE(c.is<const Foo>());
    EXPECT_FALSE(c.as<const Foo>());
    tracked_ptr<Bar> base = def;
    EXPECT_FALSE(base.is<Bar>());
    EXPECT_FALSE(base.is<Foo>());
    tracked_ptr<void> v;
    EXPECT_FALSE(v.is<void>());
    EXPECT_FALSE(v.is<Foo>());
    EXPECT_FALSE(v.as<Foo>());
    tracked_ptr<const void> cv;
    EXPECT_FALSE(cv.is<void>());
    EXPECT_FALSE(cv.is<const void>());
    tracked_ptr<int> i;
    EXPECT_FALSE(i.is<int>());
    static_assert(noexcept(def.is<Foo>()));

    // Non-null: unchanged, cv-qualifiers ignored as typeid ignores them
    tracked_ptr<Foo> foo = make_tracked<Foo>(2);
    tracked_ptr<Foo> moved_from = foo;
    tracked_ptr<Foo> target(std::move(moved_from));   // a move is a copy: the object stays
    EXPECT_TRUE(moved_from.is<Foo>());
    EXPECT_TRUE(target.is<Foo>());
    EXPECT_TRUE(foo.is<const Foo>());
    EXPECT_FALSE(foo.is<Bar>());
    EXPECT_FALSE(foo.is<void>());
    EXPECT_EQ(foo.as<const Foo>().get(), foo.get());
    tracked_ptr<const void> any = foo;
    EXPECT_TRUE(any.is<Foo>());
    EXPECT_FALSE(any.is<void>());
}

TEST(PointerBoundaries_Tests, TrackedItselfAsTheArgument) {
    tracked_ptr<Foo> p = make_tracked<Foo>(7);
    Foo* raw = p.get();
    p = p;
    EXPECT_EQ(p.get(), raw);
    p = std::move(p);   // a move is a copy: the value stays
    EXPECT_EQ(p.get(), raw);
    p.swap(p);
    EXPECT_EQ(p.get(), raw);
    std::swap(p, p);
    EXPECT_EQ(p.get(), raw);
    p.reset(p.get());
    EXPECT_EQ(p.get(), raw);
    p.store(p, barrier::off);
    EXPECT_EQ(p.get(), raw);
    tracked_ptr<Foo> q(p, barrier::off);
    EXPECT_EQ(q.get(), raw);
    p.shade();
    EXPECT_EQ(p->get_value(), 7);

    // A pointer stored into the object it points at, through itself
    auto h = tracked_ptr<Holder>(make_tracked<Holder>());
    h->self = h;
    h->self = h->self;
    h->self = std::move(h->self);
    EXPECT_EQ(h->self, h);
    h->self.swap(h->self);
    EXPECT_EQ(h->self, h);
    h = h->self;   // the last other holder of the object replaced by its own member
    EXPECT_EQ(h->value, 5);
    collector::force_collect(true);
    EXPECT_EQ(h->self, h);
    h->self = nullptr;
}

TEST(PointerBoundaries_Tests, TrackedMovedFromKeepsItsValue) {
    tracked_ptr<Foo> p = make_tracked<Foo>(3);
    tracked_ptr<Foo> q(std::move(p));
    EXPECT_EQ(p, q);
    tracked_ptr<Bar> b(std::move(p));
    EXPECT_EQ(p, b);
    tracked_ptr<Foo> r;
    r = std::move(p);
    EXPECT_EQ(p, r);
    tracked_ptr<Bar> c;
    c = std::move(p);
    EXPECT_EQ(p, c);
    EXPECT_EQ(p->get_value(), 3);
}

TEST(PointerBoundaries_Tests, TrackedThroughABaseAtAnOffset) {
    ASSERT_TRUE(bar_is_offset());
    tracked_ptr<Foo> foo = make_tracked<Foo>(4);
    tracked_ptr<Bar> bar = foo;
    EXPECT_EQ(bar->get_value(), 4);
    EXPECT_TRUE(bar == foo);   // == compares as const void*: the two addresses differ
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar>());
    EXPECT_EQ(bar.as<Foo>(), foo);
    EXPECT_EQ(bar.as<Foo>().get(), foo.get());
    EXPECT_EQ(static_pointer_cast<Foo>(bar).get(), foo.get());
    EXPECT_EQ(dynamic_pointer_cast<Foo>(bar).get(), foo.get());
    EXPECT_FALSE(dynamic_pointer_cast<Baz>(bar));
    tracked_ptr<void> v = bar;
    EXPECT_TRUE(v.is<Foo>());
    EXPECT_EQ(v.as<Foo>().get(), foo.get());

    // An alias into a member: the whole object from as<>, a shared_ptr to the member
    tracked_ptr<int> member(&foo->value);
    EXPECT_TRUE(member.is<Foo>());
    EXPECT_EQ(member.as<Foo>().get(), foo.get());
    auto shared = member.to_shared();
    EXPECT_EQ(shared.get(), &foo->value);
    EXPECT_EQ(*shared, 4);
}

TEST(PointerBoundaries_Tests, TrackedComparisonsAtNullAndEqual) {
    tracked_ptr<Foo> a = make_tracked<Foo>(1);
    tracked_ptr<Foo> n;
    EXPECT_TRUE(n < a);
    EXPECT_TRUE(a > nullptr);
    EXPECT_TRUE(nullptr < a);
    EXPECT_TRUE((a <=> a) == 0);
    EXPECT_TRUE(a <= a);
    EXPECT_FALSE(a < a);
    tracked_ptr<const void> cv = a;
    EXPECT_TRUE(cv == a);
}

// root_ptr

TEST(PointerBoundaries_Tests, RootNullAtEveryMember) {
    root_ptr<Foo> r;
    EXPECT_FALSE(r);
    EXPECT_EQ(r.get(), nullptr);
    EXPECT_TRUE(r == nullptr);
    EXPECT_TRUE(nullptr == r);
    EXPECT_FALSE(r != nullptr);
    EXPECT_FALSE(r.ptr());
    EXPECT_TRUE(r.type() == typeid(Foo));
    EXPECT_FALSE(r.is<Bar>());
    EXPECT_FALSE(r.as<Bar>());
    EXPECT_FALSE(r.as<Foo>());
    EXPECT_EQ(std::hash<root_ptr<Foo>>{}(r), std::hash<const void*>{}(nullptr));
    r.reset();
    r = nullptr;
    EXPECT_FALSE(r);
    root_ptr<Foo> from_null_tracked{tracked_ptr<Foo>()};
    EXPECT_FALSE(from_null_tracked);
    root_ptr<Foo> from_empty_unique{unique_ptr<Foo>()};
    EXPECT_FALSE(from_empty_unique);
    root_ptr<Bar> from_null_root(r);
    EXPECT_FALSE(from_null_root);
    EXPECT_TRUE(r == from_null_root);
    EXPECT_TRUE((r <=> from_null_root) == 0);
    root_ptr<void> v(r);
    EXPECT_FALSE(v);
    EXPECT_TRUE(v.type() == typeid(void));
}

TEST(PointerBoundaries_Tests, RootNullIsNoType) {
    root_ptr<Foo> def;
    root_ptr<Foo> assigned = make_tracked<Foo>(1);
    EXPECT_TRUE(assigned.is<Foo>());
    assigned = nullptr;
    root_ptr<Foo> reset = make_tracked<Foo>(2);
    reset.reset();
    root_ptr<Foo> moved_from = make_tracked<Foo>(3);
    root_ptr<Foo> moved_into(std::move(moved_from));
    root_ptr<Foo> moved_assigned_from = make_tracked<Foo>(4);
    root_ptr<Foo> moved_assigned;
    moved_assigned = std::move(moved_assigned_from);
    for (const root_ptr<Foo>* r : {&def, &assigned, &reset, &moved_from, &moved_assigned_from}) {
        EXPECT_FALSE(*r);
        EXPECT_FALSE(r->is<Foo>());
        EXPECT_FALSE(r->is<const Foo>());
        EXPECT_FALSE(r->is<Bar>());
        EXPECT_FALSE(r->is<void>());
        EXPECT_TRUE(r->type() == typeid(Foo));
        EXPECT_FALSE(r->as<Foo>());
        EXPECT_FALSE(r->as<Bar>());
    }
    root_ptr<Bar> base;
    EXPECT_FALSE(base.is<Bar>());
    root_ptr<const Foo> c;
    EXPECT_FALSE(c.is<Foo>());
    root_ptr<void> v;
    EXPECT_FALSE(v.is<void>());
    EXPECT_FALSE(v.as<Foo>());

    // Non-null: unchanged
    EXPECT_TRUE(moved_into.is<Foo>());
    EXPECT_TRUE(moved_into.is<const Foo>());
    EXPECT_FALSE(moved_into.is<Bar>());
    EXPECT_TRUE(moved_assigned.is<Foo>());
    EXPECT_EQ(moved_assigned.as<Foo>().get(), moved_assigned.get());
}

TEST(PointerBoundaries_Tests, RootItselfAsTheArgument) {
    root_ptr<Foo> r = make_tracked<Foo>(9);
    Foo* raw = r.get();
    r = r;
    EXPECT_EQ(r.get(), raw);
    r = std::move(r);   // a move into itself changes nothing (operator_assign.md)
    EXPECT_EQ(r.get(), raw);
    r.swap(r);
    EXPECT_EQ(r.get(), raw);
    swap(r, r);
    EXPECT_EQ(r.get(), raw);
    r = r.ptr();
    EXPECT_EQ(r.get(), raw);
    r.reset(r.ptr());
    EXPECT_EQ(r.get(), raw);
    r.ptr() = r.ptr();
    EXPECT_EQ(r.get(), raw);
    collector::force_collect(true);
    EXPECT_EQ(r->get_value(), 9);
}

TEST(PointerBoundaries_Tests, RootMovedFromIsNullAndReusable) {
    root_ptr<Foo> a = make_tracked<Foo>(2);
    Foo* raw = a.get();
    root_ptr<Foo> b(std::move(a));
    EXPECT_FALSE(a);
    EXPECT_EQ(b.get(), raw);
    root_ptr<Foo> c;
    c = std::move(b);
    EXPECT_FALSE(b);
    EXPECT_EQ(c.get(), raw);
    a = c;   // a root moved from keeps its cell and takes a value again
    EXPECT_EQ(a.get(), raw);
    b = make_tracked<Foo>(3);
    EXPECT_EQ(b->get_value(), 3);
    // From a root of a derived type: a copy (the source keeps its value)
    root_ptr<Bar> base(std::move(c));
    EXPECT_EQ(c.get(), raw);
    EXPECT_EQ(base->get_value(), 2);
    EXPECT_TRUE(base == c);
    root_ptr<Bar> base2;
    base2 = std::move(c);
    EXPECT_EQ(c.get(), raw);
    EXPECT_TRUE(base2 == c);
}

TEST(PointerBoundaries_Tests, RootComparedWithATrackedOfAnotherType) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(1);
    root_ptr<Bar> bar(foo);
    EXPECT_TRUE(bar == foo);   // the addresses differ: Bar is at an offset
    EXPECT_TRUE(foo == bar);
    root_ptr<Foo> r(foo);
    EXPECT_TRUE((r <=> bar) == 0);
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_EQ(bar.as<Foo>().get(), foo.get());
}

// rooted

TEST(PointerBoundaries_Tests, RootedMovedFrom) {
    rooted<Int> a(std::in_place, 4);
    Int* raw = a.get();
    rooted<Int> b(std::move(a));
    EXPECT_EQ(a.get(), nullptr);   // null only for a rooted moved from (get.md)
    EXPECT_FALSE(a.ptr());
    EXPECT_EQ(b.get(), raw);
    rooted<Int> c(std::in_place);
    c = std::move(b);
    EXPECT_EQ(b.get(), nullptr);
    EXPECT_EQ(c.get(), raw);
    b = c;   // assigned to after the move: shares the value again
    EXPECT_EQ(b.get(), raw);
    a = rooted<Int>(Int(6));
    EXPECT_EQ(*a, 6);
    rooted<Int> moved_away(std::move(c));
    rooted<Int> copy_of_moved(c);   // a copy of a moved-from one holds nothing too
    EXPECT_EQ(copy_of_moved.get(), nullptr);
    a.swap(copy_of_moved);
    EXPECT_EQ(a.get(), nullptr);
    EXPECT_EQ(*copy_of_moved, 6);
}

TEST(PointerBoundaries_Tests, RootedItselfAsTheArgument) {
    rooted<Int> r(std::in_place, 8);
    Int* raw = r.get();
    r = r;
    EXPECT_EQ(r.get(), raw);
    r = std::move(r);
    EXPECT_EQ(r.get(), raw);
    r.swap(r);
    EXPECT_EQ(r.get(), raw);
    swap(r, r);
    EXPECT_EQ(r.get(), raw);
    rooted<Int> copy(*r);   // a copy of the value: another object
    EXPECT_NE(copy.get(), raw);
    EXPECT_EQ(*copy, 8);
    r = rooted<Int>(*r);
    EXPECT_NE(r.get(), raw);
    EXPECT_EQ(*r, 8);
    collector::force_collect(true);
    EXPECT_EQ(*r, 8);
}

TEST(PointerBoundaries_Tests, RootedOfAValueWithPointers) {
    rooted<string> s(std::in_place);
    EXPECT_TRUE(s->empty());
    rooted<string> t(string("text"));
    *s = *t;
    collector::force_collect(true);
    EXPECT_EQ(*s, "text");
    rooted<tracked_ptr<Int>> p(std::in_place);
    EXPECT_FALSE(*p);
    *p = make_tracked<Int>(1);
    collector::force_collect(true);
    EXPECT_EQ(**p, 1);
}

// weak_ptr

TEST(PointerBoundaries_Tests, WeakEmptyAtEveryMember) {
    weak_ptr<Foo> w;
    EXPECT_TRUE(w.expired());
    EXPECT_FALSE(w.lock());
    weak_ptr<Foo> n(nullptr);
    EXPECT_TRUE(n.expired());
    weak_ptr<Foo> from_null{tracked_ptr<Foo>()};
    EXPECT_TRUE(from_null.expired());
    EXPECT_FALSE(from_null.lock());
    root_ptr<Foo> null_root;
    weak_ptr<Foo> from_null_root(null_root);
    EXPECT_TRUE(from_null_root.expired());
    weak_ptr<const Foo> converted(w);
    EXPECT_TRUE(converted.expired());
    EXPECT_FALSE(converted.lock());
    converted = w;
    EXPECT_TRUE(converted.expired());
    weak_ptr<Bar> base(w.lock());
    EXPECT_TRUE(base.expired());
    w.reset();
    w = nullptr;
    w.swap(n);
    EXPECT_TRUE(w.expired());
    EXPECT_TRUE(n.expired());
    w = tracked_ptr<Foo>();
    EXPECT_TRUE(w.expired());
}

TEST(PointerBoundaries_Tests, WeakItselfAsTheArgument) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(5);
    weak_ptr<Foo> w(foo);
    w = w;
    EXPECT_EQ(w.lock(), foo);
    w = std::move(w);
    EXPECT_EQ(w.lock(), foo);
    w.swap(w);
    EXPECT_EQ(w.lock(), foo);
    swap(w, w);
    EXPECT_EQ(w.lock(), foo);
    w = w.lock();   // a new cell from its own target
    EXPECT_EQ(w.lock(), foo);
    weak_ptr<Foo> moved(std::move(w));   // a move shares the cell: the source keeps it
    EXPECT_EQ(w.lock(), foo);
    EXPECT_EQ(moved.lock(), foo);
}

// A weak_ptr converts only to its own type with const added (the address
// in the cell is the one lock() returns); a base or void is reached through
// a strong pointer, whose conversion has the live object
static_assert(std::is_constructible_v<weak_ptr<const Foo>, const weak_ptr<Foo>&>);
static_assert(std::is_assignable_v<weak_ptr<const Foo>&, const weak_ptr<Foo>&>);
static_assert(std::is_convertible_v<weak_ptr<Foo>, weak_ptr<const Foo>>);
static_assert(std::is_constructible_v<weak_ptr<const void>, const weak_ptr<void>&>);
static_assert(std::is_assignable_v<weak_ptr<const void>&, const weak_ptr<void>&>);
static_assert(!std::is_constructible_v<weak_ptr<Bar>, const weak_ptr<Foo>&>);
static_assert(!std::is_constructible_v<weak_ptr<Bar>, weak_ptr<Foo>&&>);
static_assert(!std::is_assignable_v<weak_ptr<Bar>&, const weak_ptr<Foo>&>);
static_assert(!std::is_assignable_v<weak_ptr<Bar>&, weak_ptr<Foo>&&>);
static_assert(!std::is_constructible_v<weak_ptr<Bar>, const weak_ptr<Baz>&>);   // a base at offset 0 too
static_assert(!std::is_assignable_v<weak_ptr<Bar>&, const weak_ptr<Baz>&>);
static_assert(!std::is_constructible_v<weak_ptr<void>, const weak_ptr<Foo>&>);
static_assert(!std::is_assignable_v<weak_ptr<void>&, const weak_ptr<Foo>&>);
static_assert(!std::is_constructible_v<weak_ptr<const void>, const weak_ptr<Foo>&>);
static_assert(!std::is_assignable_v<weak_ptr<const void>&, const weak_ptr<Foo>&>);
static_assert(!std::is_constructible_v<weak_ptr<Foo>, const weak_ptr<const Foo>&>);   // const not removed
static_assert(!std::is_assignable_v<weak_ptr<Foo>&, const weak_ptr<const Foo>&>);
static_assert(!std::is_constructible_v<weak_ptr<Foo>, const weak_ptr<Bar>&>);
static_assert(std::is_constructible_v<weak_ptr<Bar>, const tracked_ptr<Foo>&>);
static_assert(std::is_assignable_v<weak_ptr<Bar>&, const tracked_ptr<Foo>&>);
static_assert(std::is_constructible_v<weak_ptr<void>, const tracked_ptr<Foo>&>);
static_assert(std::is_assignable_v<weak_ptr<void>&, const tracked_ptr<Foo>&>);
static_assert(std::is_constructible_v<weak_ptr<const void>, const tracked_ptr<Foo>&>);
static_assert(std::is_constructible_v<weak_ptr<Bar>, const root_ptr<Foo>&>);

TEST(PointerBoundaries_Tests, WeakToABaseAtAnOffsetThroughLock) {
    ASSERT_TRUE(bar_is_offset());
    root_ptr<Foo> foo = make_tracked<Foo>(6);
    weak_ptr<Foo> wf;
    weak_ptr<Bar> direct, converted, assigned;
    weak_ptr<const void> v;
    off_frame([&] {
        tracked_ptr<Bar> bar = foo.ptr();
        wf = foo.ptr();
        direct = bar;   // from the strong pointer: converted when the cell is made
        EXPECT_EQ(direct.lock().get(), bar.get());
        converted = weak_ptr<Bar>(wf.lock());   // from the weak pointer of the derived type, locked
        EXPECT_EQ(converted.lock().get(), bar.get());
        EXPECT_EQ(converted.lock()->get_value(), 6);
        assigned = wf.lock();
        EXPECT_EQ(assigned.lock().get(), bar.get());
        v = wf.lock();
        EXPECT_EQ(v.lock().get(), static_cast<const void*>(foo.get()));
        weak_ptr<Foo> same(wf);   // the same type: the cell shared
        EXPECT_EQ(same.lock(), foo.ptr());
    });
    // Each stays weak: the object goes with its last strong pointer
    foo = nullptr;
    collector::clear_stack();
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_TRUE(wf.expired());
    EXPECT_TRUE(direct.expired());
    EXPECT_TRUE(converted.expired());
    EXPECT_TRUE(assigned.expired());
    EXPECT_TRUE(v.expired());
    EXPECT_FALSE(converted.lock());
    weak_ptr<Bar> of_expired(wf.lock());   // an expired source locks to null: an expired pointer
    EXPECT_TRUE(of_expired.expired());
}

TEST(PointerBoundaries_Tests, WeakOfAConstType) {
    // T may be const (weak_ptr.md, Template parameters): lock() of one compiles and works
    tracked_ptr<const Int> c = make_tracked<const Int>(7);
    weak_ptr<const Int> w(c);
    EXPECT_EQ(w.lock(), c);
    EXPECT_EQ(*w.lock(), 7);
    tracked_ptr<Int> m = make_tracked<Int>(8);
    weak_ptr<const Int> from_mutable(m);
    EXPECT_EQ(*from_mutable.lock(), 8);
    weak_ptr<Int> wm(m);
    weak_ptr<const Int> converted(wm);
    EXPECT_EQ(converted.lock(), m);
    weak_ptr<const void> v(c);
    EXPECT_EQ(v.lock().get(), static_cast<const void*>(c.get()));
    EXPECT_FALSE(weak_ptr<const Int>().lock());
}

// unique_ptr and make_tracked

TEST(PointerBoundaries_Tests, UniqueEmptyAtEveryMember) {
    unique_ptr<Foo> u;
    EXPECT_FALSE(u);
    EXPECT_TRUE(u.type() == typeid(Foo));
    EXPECT_FALSE(u.is<Bar>());
    EXPECT_FALSE(u.as<Bar>());
    EXPECT_FALSE(u.as<Foo>());   // empty: nothing to move, null back
    EXPECT_EQ(u.release(), nullptr);
    u.reset();
    u = nullptr;
    EXPECT_FALSE(static_pointer_cast<Bar>(std::move(u)));
    EXPECT_FALSE(const_pointer_cast<const Foo>(std::move(u)));
    EXPECT_FALSE(dynamic_pointer_cast<Bar>(std::move(u)));
    unique_ptr<void>& v = u;
    EXPECT_FALSE(v.as<Foo>());
    EXPECT_TRUE(v.type() == typeid(void));
    EXPECT_EQ(std::hash<unique_ptr<Foo>>{}(u), std::hash<Foo*>{}(nullptr));
    unique_ptr<Foo> other;
    u.swap(other);
    EXPECT_FALSE(u);
}

TEST(PointerBoundaries_Tests, UniqueEmptyIsNoType) {
    unique_ptr<Foo> def;
    unique_ptr<Foo> assigned = make_tracked<Foo>(1);
    EXPECT_TRUE(assigned.is<Foo>());
    assigned = nullptr;
    unique_ptr<Foo> reset = make_tracked<Foo>(2);
    reset.reset();
    unique_ptr<Foo> moved_from = make_tracked<Foo>(3);
    Foo* raw = moved_from.get();
    unique_ptr<Foo> moved_into(std::move(moved_from));
    unique_ptr<Foo> released = make_tracked<Foo>(4);
    tracked_ptr<Foo> kept(std::move(released));
    for (unique_ptr<Foo>* u : {&def, &assigned, &reset, &moved_from, &released}) {
        EXPECT_FALSE(*u);
        EXPECT_FALSE(u->is<Foo>());
        EXPECT_FALSE(u->is<const Foo>());
        EXPECT_FALSE(u->is<Bar>());
        EXPECT_FALSE(u->is<void>());
        EXPECT_TRUE(u->type() == typeid(Foo));
        EXPECT_FALSE(u->as<Foo>());
        EXPECT_FALSE(u->as<Bar>());
        EXPECT_FALSE(*u);
    }
    unique_ptr<Bar> base;
    EXPECT_FALSE(base.is<Bar>());
    unique_ptr<const Foo> c;
    EXPECT_FALSE(c.is<Foo>());
    EXPECT_FALSE(c.is<const Foo>());
    unique_ptr<void>& v = def;
    EXPECT_FALSE(v.is<void>());
    EXPECT_FALSE(v.is<Foo>());
    EXPECT_FALSE(v.as<Foo>());

    // Non-null: unchanged; as<U>() of the right type moves the object
    EXPECT_TRUE(moved_into.is<Foo>());
    EXPECT_TRUE(moved_into.is<const Foo>());
    EXPECT_FALSE(moved_into.is<Bar>());
    auto back = moved_into.as<Foo>();
    EXPECT_EQ(back.get(), raw);
    EXPECT_FALSE(moved_into);
    EXPECT_FALSE(moved_into.is<Foo>());
    EXPECT_TRUE(kept.is<Foo>());
}

TEST(PointerBoundaries_Tests, UniqueItselfAsTheArgument) {
    auto u = make_tracked<Int>(3);
    Int* raw = u.get();
    size_t before = Int::counter;
    u = std::move(u);   // std::unique_ptr's self-move: reset(release()), the object kept
    EXPECT_EQ(u.get(), raw);
    EXPECT_EQ(Int::counter, before);
    u.swap(u);
    EXPECT_EQ(u.get(), raw);
    u.reset(u.get() == raw ? u.release() : nullptr);
    EXPECT_EQ(u.get(), raw);
    EXPECT_EQ(Int::counter, before);
    EXPECT_EQ(*u, 3);
}

TEST(PointerBoundaries_Tests, UniqueThroughABaseAtAnOffset) {
    ASSERT_TRUE(bar_is_offset());
    auto foo = make_tracked<Foo>(2);
    Foo* raw = foo.get();
    unique_ptr<Bar> bar(std::move(foo));
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar>());
    auto back = bar.as<Foo>();
    EXPECT_FALSE(bar);
    EXPECT_EQ(back.get(), raw);
    unique_ptr<void>& v = back;
    EXPECT_TRUE(v.is<Foo>());
    auto again = v.as<Foo>();
    EXPECT_EQ(again.get(), raw);
    EXPECT_FALSE(back);
    auto kept = again.as<Baz>();   // the wrong type: null, and the object stays
    EXPECT_FALSE(kept);
    EXPECT_EQ(again.get(), raw);
    unique_ptr<Bar> b2 = static_pointer_cast<Bar>(std::move(again));
    auto d = dynamic_pointer_cast<Foo>(std::move(b2));
    EXPECT_EQ(d.get(), raw);
    EXPECT_EQ(d->get_value(), 2);
}

TEST(PointerBoundaries_Tests, MakeTrackedAtTheEdges) {
    struct Empty {};
    auto e = make_tracked<Empty>();
    auto e2 = make_tracked<Empty>();
    EXPECT_NE(e.get(), e2.get());   // two empty objects, two addresses
    auto c = make_tracked<const Int>(5);
    EXPECT_EQ(*c, 5);
    EXPECT_TRUE(c.is<Int>());
    tracked_ptr<const Int> t = std::move(c);
    EXPECT_EQ(*t, 5);
    struct Big { std::byte bytes[config::page_size - sizeof(sgcl::detail::ArrayBase)]; };   // the largest object a page takes
    auto big = make_tracked<Big>();
    EXPECT_EQ(big->bytes[0], std::byte{0});
    EXPECT_EQ(big->bytes[sizeof(Big) - 1], std::byte{0});
    static_assert(noexcept(make_tracked<Int>(1)));
    static_assert(!noexcept(make_tracked<std::string>("x")));
    auto s = make_tracked<std::string>(3, 'x');
    EXPECT_EQ(*s, "xxx");
}

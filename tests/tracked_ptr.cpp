//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

struct TrackedPtr_Tests : ::testing::Test {};

TEST_F(TrackedPtr_Tests, DefaultConstructor) {
    { // stack
        tracked_ptr<int> ptr;
        EXPECT_EQ(ptr, nullptr);
        EXPECT_TRUE(ptr.allocated_on_stack());
    }
    { // heap
        auto ptr = make_tracked<tracked_ptr<int>>();
        EXPECT_EQ(*ptr, nullptr);
        EXPECT_TRUE(ptr->allocated_on_heap());
    }
    { // external heap
        auto ptr = std::make_unique<tracked_ptr<int>>();
        EXPECT_EQ(*ptr, nullptr);
        EXPECT_TRUE(ptr->allocated_on_external_heap());
    }
}

TEST_F(TrackedPtr_Tests, NullConstructor) {
    { // stack
        tracked_ptr<int> ptr(nullptr);
        EXPECT_EQ(ptr, nullptr);
    }
    { // heap
        auto ptr = make_tracked<tracked_ptr<int>>(nullptr);
        EXPECT_EQ(*ptr, nullptr);
    }
    { // external heap
        auto ptr = std::make_unique<tracked_ptr<int>>(nullptr);
        EXPECT_EQ(*ptr, nullptr);
    }
}

TEST_F(TrackedPtr_Tests, UniqueConstructor) {
    { // stack
        tracked_ptr<Bar> ptr = make_tracked<Foo>(3);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(ptr->get_value(), 3);
    }
    { // heap
        auto ptr = make_tracked<tracked_ptr<Foo>>(make_tracked<Foo>(3));
        ASSERT_NE(*ptr, nullptr);
        EXPECT_EQ((*ptr)->get_value(), 3);
    }
    { // external heap
        auto ptr = std::make_unique<tracked_ptr<Foo>>(make_tracked<Foo>(3));
        ASSERT_NE(*ptr, nullptr);
        EXPECT_EQ((*ptr)->get_value(), 3);
    }
}

TEST_F(TrackedPtr_Tests, RawConstructor) {
    { // stack
        tracked_ptr<Foo> foo = make_tracked<Foo>(10);
        tracked_ptr<int> alias(&foo->value);
        ASSERT_NE(alias, nullptr);
        EXPECT_EQ(*alias, 10);
    }
    { // heap
        tracked_ptr<Foo> foo = make_tracked<Foo>(10);
        auto alias = make_tracked<tracked_ptr<int>>(&foo->value);
        ASSERT_NE(*alias, nullptr);
        EXPECT_EQ(**alias, 10);
    }
    { // external heap
        tracked_ptr<Foo> foo = make_tracked<Foo>(10);
        auto alias = std::make_unique<tracked_ptr<int>>(&foo->value);
        ASSERT_NE(*alias, nullptr);
        EXPECT_EQ(**alias, 10);
    }
}

TEST_F(TrackedPtr_Tests, CopyConstructor) {
    tracked_ptr<int> ptr1 = make_tracked<int>(8);
    { // stack
        tracked_ptr<int> ptr2(ptr1);
        ASSERT_NE(ptr2, nullptr);
        EXPECT_EQ(*ptr2, 8);
    }
    { // heap
        auto ptr2 = make_tracked<tracked_ptr<int>>(ptr1);
        ASSERT_NE(*ptr2, nullptr);
        EXPECT_EQ(**ptr2, 8);
    }
    { // external heap
        auto ptr2 = std::make_unique<tracked_ptr<int>>(ptr1);
        ASSERT_NE(*ptr2, nullptr);
        EXPECT_EQ(**ptr2, 8);
    }
}

TEST_F(TrackedPtr_Tests, CopyCastConstructor) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(4);
    { // stack
        tracked_ptr<Bar> bar = foo;
        ASSERT_NE(bar, nullptr);
        EXPECT_EQ(bar->get_value(), 4);
    }
    { // heap
        auto bar = make_tracked<tracked_ptr<Bar>>(foo);
        ASSERT_NE(*bar, nullptr);
        EXPECT_EQ((*bar)->get_value(), 4);
    }
    { // external heap
        auto bar = std::make_unique<tracked_ptr<Bar>>(foo);
        ASSERT_NE(*bar, nullptr);
        EXPECT_EQ((*bar)->get_value(), 4);
    }
}

TEST_F(TrackedPtr_Tests, MoveConstructor) {
    { // stack
        tracked_ptr<int> s = make_tracked<int>(8);
        auto h = make_tracked<tracked_ptr<int>>(make_tracked<int>(9));
        auto e = std::make_unique<tracked_ptr<int>>(make_tracked<int>(10));
        tracked_ptr<int> ss(std::move(s));
        ASSERT_NE(ss, nullptr);
        EXPECT_EQ(*ss, 8);
        ASSERT_NE(s, nullptr);
        tracked_ptr<int> sh(std::move(*h));
        ASSERT_NE(sh, nullptr);
        EXPECT_EQ(*sh, 9);
        ASSERT_NE(*h, nullptr);
        tracked_ptr<int> se(std::move(*e));
        ASSERT_NE(se, nullptr);
        EXPECT_EQ(*se, 10);
        ASSERT_NE(*e, nullptr);
    }
    { // heap
        tracked_ptr<int> s = make_tracked<int>(8);
        auto h = make_tracked<tracked_ptr<int>>(make_tracked<int>(9));
        auto e = std::make_unique<tracked_ptr<int>>(make_tracked<int>(10));
        auto hs = make_tracked<tracked_ptr<int>>(std::move(s));
        ASSERT_NE(*hs, nullptr);
        EXPECT_EQ(**hs, 8);
        ASSERT_NE(s, nullptr);
        auto hh = make_tracked<tracked_ptr<int>>(std::move(*h));
        ASSERT_NE(*hh, nullptr);
        EXPECT_EQ(**hh, 9);
        ASSERT_NE(*h, nullptr);
        auto he = make_tracked<tracked_ptr<int>>(std::move(*e));
        ASSERT_NE(*he, nullptr);
        EXPECT_EQ(**he, 10);
        ASSERT_NE(*e, nullptr);
    }
    { // external heap
        tracked_ptr<int> s = make_tracked<int>(8);
        auto h = make_tracked<tracked_ptr<int>>(make_tracked<int>(9));
        auto e = std::make_unique<tracked_ptr<int>>(make_tracked<int>(10));
        auto es = std::make_unique<tracked_ptr<int>>(std::move(s));
        ASSERT_NE(*es, nullptr);
        EXPECT_EQ(**es, 8);
        ASSERT_NE(s, nullptr);
        auto eh = std::make_unique<tracked_ptr<int>>(std::move(*h));
        ASSERT_NE(*eh, nullptr);
        EXPECT_EQ(**eh, 9);
        ASSERT_NE(*h, nullptr);
        auto ee = std::make_unique<tracked_ptr<int>>(std::move(*e));
        ASSERT_NE(*ee, nullptr);
        EXPECT_EQ(**ee, 10);
        *e = tracked_ptr<int>();
        ASSERT_EQ(*e, nullptr);
    }
}

TEST_F(TrackedPtr_Tests, MoveCastConstructor) {
    { // stack
        tracked_ptr<Foo> s = make_tracked<Foo>(4);
        auto h = make_tracked<tracked_ptr<Foo>>(make_tracked<Foo>(5));
        auto e = std::make_unique<tracked_ptr<Foo>>(make_tracked<Foo>(6));
        tracked_ptr<Bar> ss(std::move(s));
        ASSERT_NE(ss, nullptr);
        EXPECT_EQ(ss->get_value(), 4);
        ASSERT_NE(s, nullptr);
        tracked_ptr<Bar> sh(std::move(*h));
        ASSERT_NE(sh, nullptr);
        EXPECT_EQ(sh->get_value(), 5);
        ASSERT_NE(*h, nullptr);
        tracked_ptr<Bar> se(std::move(*e));
        ASSERT_NE(se, nullptr);
        EXPECT_EQ(se->get_value(), 6);
        ASSERT_NE(*e, nullptr);
    }
    { // heap
        tracked_ptr<Foo> s = make_tracked<Foo>(4);
        auto h = make_tracked<tracked_ptr<Foo>>(make_tracked<Foo>(5));
        auto e = std::make_unique<tracked_ptr<Foo>>(make_tracked<Foo>(6));
        auto hs = make_tracked<tracked_ptr<Bar>>(std::move(s));
        ASSERT_NE(*hs, nullptr);
        EXPECT_EQ((*hs)->get_value(), 4);
        ASSERT_NE(s, nullptr);
        auto hh = make_tracked<tracked_ptr<Bar>>(std::move(*h));
        ASSERT_NE(*hh, nullptr);
        EXPECT_EQ((*hh)->get_value(), 5);
        ASSERT_NE(*h, nullptr);
        auto he = make_tracked<tracked_ptr<Bar>>(std::move(*e));
        ASSERT_NE(*he, nullptr);
        EXPECT_EQ((*he)->get_value(), 6);
        ASSERT_NE(*e, nullptr);
    }
    { // external heap
        tracked_ptr<Foo> s = make_tracked<Foo>(4);
        auto h = make_tracked<tracked_ptr<Foo>>(make_tracked<Foo>(5));
        auto e = std::make_unique<tracked_ptr<Foo>>(make_tracked<Foo>(6));
        auto es = std::make_unique<tracked_ptr<Bar>>(std::move(s));
        ASSERT_NE(*es, nullptr);
        EXPECT_EQ((*es)->get_value(), 4);
        ASSERT_NE(s, nullptr);
        auto eh = std::make_unique<tracked_ptr<Bar>>(std::move(*h));
        ASSERT_NE(*eh, nullptr);
        EXPECT_EQ((*eh)->get_value(), 5);
        ASSERT_NE(*h, nullptr);
        auto ee = std::make_unique<tracked_ptr<Bar>>(std::move(*e));
        ASSERT_NE(*ee, nullptr);
        EXPECT_EQ((*ee)->get_value(), 6);
        e->reset();
        ASSERT_EQ(*e, nullptr);
    }
}

TEST_F(TrackedPtr_Tests, CopyAssignmentOperator) {
    tracked_ptr<int> ptr1 = make_tracked<int>(3);
    tracked_ptr<int> ptr2;
    ptr2 = ptr1;
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 3);
}

TEST_F(TrackedPtr_Tests, CopyCastAssignmentOperator) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(5);
    tracked_ptr<Bar> bar;
    bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 5);
}

TEST_F(TrackedPtr_Tests, NullAssignmentOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(8);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 8);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(TrackedPtr_Tests, UniqueCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(11);
    tracked_ptr<Bar> bar;
    bar = std::move(foo);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 11);
}

TEST_F(TrackedPtr_Tests, VoidRefOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(3);
    ASSERT_NE(ptr, nullptr);
    tracked_ptr<void>& ref = ptr;
    ASSERT_NE(ref, nullptr);
}

TEST_F(TrackedPtr_Tests, BoolOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST_F(TrackedPtr_Tests, IndirectionOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(15);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, *ptr.get());
}

TEST_F(TrackedPtr_Tests, StructureDereferenceOperator) {
    tracked_ptr<Foo> ptr = make_tracked<Foo>(3);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, ptr.get()->value);
}

TEST_F(TrackedPtr_Tests, reset) {
    tracked_ptr<int> ptr = make_tracked<int>(9);
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(TrackedPtr_Tests, swap) {
    tracked_ptr<int> ptr1 = make_tracked<int>(2);
    tracked_ptr<int> ptr2 = make_tracked<int>(5);
    ptr1.swap(ptr2);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr1, 5);
    EXPECT_EQ(*ptr2, 2);
}

TEST_F(TrackedPtr_Tests, is) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(6);
    tracked_ptr<Bar> bar = foo;
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar>());
    tracked_ptr<int> alias(&foo->value);
    EXPECT_TRUE(alias.is<Foo>());
    EXPECT_FALSE(alias.is<int>());
}

TEST_F(TrackedPtr_Tests, as) {
    tracked_ptr<Bar> bar = make_tracked<Foo>(8);
    tracked_ptr<Baz> baz = bar.as<Baz>();
    EXPECT_EQ(baz, nullptr);
    tracked_ptr<Foo> foo = bar.as<Foo>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 8);
    tracked_ptr<int> alias(&foo->value);
    *alias = 12;
    foo = alias.as<Foo>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 12);
}

TEST_F(TrackedPtr_Tests, type) {
    tracked_ptr<Bar> bar = make_tracked<Foo>(10);
    EXPECT_EQ(bar.type(), typeid(Foo));
}

TEST_F(TrackedPtr_Tests, metadata) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(12);
    tracked_ptr<Bar> bar = foo;
    EXPECT_EQ(foo.metadata(), nullptr);
    EXPECT_EQ(bar.metadata(), nullptr);
    struct {
        void operator()(void* p) {
            EXPECT_EQ(((Foo*)p)->get_value(), 12);
        }
    } metadata;
    set_metadata<Bar>(&metadata);
    EXPECT_EQ(bar.metadata(), nullptr);
    set_metadata<Foo>(&metadata);
    EXPECT_EQ(foo.metadata(), &metadata);
    EXPECT_EQ(bar.metadata(), &metadata);
    metadata(bar.get_base());
    set_metadata<Bar>(nullptr);
    set_metadata<Foo>(nullptr);
}

TEST_F(TrackedPtr_Tests, is_array) {
    tracked_ptr<Bar> bar = make_tracked<Foo>(14);
    EXPECT_FALSE(bar.is_array());
}

TEST_F(TrackedPtr_Tests, Comparisons) {
    tracked_ptr<Foo> a = make_tracked<Foo>(14);
    tracked_ptr<Foo> b = a;

    EXPECT_FALSE(a == nullptr);
    EXPECT_TRUE(a == b);

    EXPECT_FALSE(nullptr == a);
    EXPECT_TRUE(b == a);

    EXPECT_TRUE(a != nullptr);
    EXPECT_FALSE(a != b);

    EXPECT_TRUE(nullptr != a);
    EXPECT_FALSE(b != a);

    EXPECT_FALSE(a < nullptr);
    EXPECT_FALSE(a < b);

    EXPECT_TRUE(nullptr < a);
    EXPECT_FALSE(b < a);

    EXPECT_FALSE(a <= nullptr);
    EXPECT_TRUE(a <= b);

    EXPECT_TRUE(nullptr <= a);
    EXPECT_TRUE(b <= a);

    EXPECT_TRUE(a > nullptr);
    EXPECT_FALSE(a > b);

    EXPECT_FALSE(nullptr > a);
    EXPECT_FALSE(b > a);

    EXPECT_TRUE(a >= nullptr);
    EXPECT_TRUE(a >= b);

    EXPECT_FALSE(nullptr >= a);
    EXPECT_TRUE(b >= a);
}

TEST_F(TrackedPtr_Tests, Casts) {
    tracked_ptr<Bar> bar = make_tracked<Foo>(1);
    auto foo = static_pointer_cast<Foo>(bar);
    EXPECT_EQ(foo->value, 1);
    auto far = dynamic_pointer_cast<Far>(bar);
    EXPECT_EQ(far->value, 2);
    auto faz = dynamic_pointer_cast<Faz>(bar);
    EXPECT_EQ(faz->value, 3);
    tracked_ptr<const Bar> cbar = make_tracked<Foo>(5);
    auto pbar = const_pointer_cast<Bar>(cbar);
    EXPECT_EQ(pbar->get_value(), 5);
}

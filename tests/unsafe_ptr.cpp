//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

struct UnsafePtr_Tests : ::testing::Test {};

TEST_F(UnsafePtr_Tests, DefaultConstructor) {
    UnsafePtr<int> ptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UnsafePtr_Tests, NullConstructor) {
    UnsafePtr<int> ptr(nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UnsafePtr_Tests, PointerConstructor) {
    StackPtr<Foo> foo = make_tracked<Foo>(7);
    UnsafePtr<Bar> bar = foo;
    ASSERT_NE(foo, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 7);
}

TEST_F(UnsafePtr_Tests, CopyConstructor) {
    StackPtr<Foo> sfoo = make_tracked<Foo>(8);
    UnsafePtr<Foo> foo = sfoo;
    UnsafePtr<Bar> bar = foo;
    ASSERT_NE(foo, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 8);
}

TEST_F(UnsafePtr_Tests, CopyAssignmentOperator) {
    StackPtr<Foo> sfoo = make_tracked<Foo>(9);
    UnsafePtr<Foo> foo = sfoo;
    UnsafePtr<Bar> bar;
    bar = foo;
    ASSERT_NE(foo, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 9);
}

TEST_F(UnsafePtr_Tests, NullAssignmentOperator) {
    StackPtr<int> sptr = make_tracked<int>(8);
    UnsafePtr<int> ptr = sptr;
    EXPECT_NE(ptr, nullptr);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UnsafePtr_Tests, BoolOperator) {
    StackPtr<int> sptr = make_tracked<int>(5);
    UnsafePtr<int> ptr = sptr;
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST_F(UnsafePtr_Tests, IndirectionOperator) {
    StackPtr<int> ptr = make_tracked<int>(15);
    UnsafePtr<int> uptr = ptr;
    ASSERT_NE(uptr, nullptr);
    EXPECT_EQ(*uptr, *uptr.get());
}

TEST_F(UnsafePtr_Tests, StructureDereferenceOperator) {
    StackPtr<Foo> ptr = make_tracked<Foo>(3);
    UnsafePtr<Foo> uptr = ptr;
    ASSERT_NE(uptr, nullptr);
    EXPECT_EQ(uptr->value, uptr.get()->value);
}

TEST_F(UnsafePtr_Tests, reset) {
    StackPtr<int> ptr = make_tracked<int>(9);
    UnsafePtr<int> uptr = ptr;
    uptr.reset();
    EXPECT_EQ(uptr, nullptr);
}

TEST_F(UnsafePtr_Tests, swap) {
    StackPtr<int> ptr1 = make_tracked<int>(2);
    StackPtr<int> ptr2 = make_tracked<int>(5);
    UnsafePtr<int> uptr1 = ptr1;
    UnsafePtr<int> uptr2 = ptr2;
    uptr1.swap(uptr2);
    ASSERT_NE(uptr1, nullptr);
    ASSERT_NE(uptr2, nullptr);
    EXPECT_EQ(*uptr1, 5);
    EXPECT_EQ(*uptr2, 2);
}

TEST_F(UnsafePtr_Tests, clone) {
    StackPtr<int> ptr = make_tracked<int>(4);
    UnsafePtr<int> uptr1 = ptr;
    auto uptr2 = uptr1.clone();
    ASSERT_NE(uptr1, nullptr);
    ASSERT_NE(uptr2, nullptr);
    EXPECT_EQ(*uptr1, *uptr2);
}

TEST_F(UnsafePtr_Tests, is) {
    StackPtr<Foo> foo = make_tracked<Foo>(6);
    UnsafePtr<Foo> ufoo = foo;
    UnsafePtr<Bar> bar = ufoo;
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar>());
}

TEST_F(UnsafePtr_Tests, as) {
    StackPtr<Bar> sbar = make_tracked<Foo>(8);
    UnsafePtr<Bar> bar = sbar;
    auto baz = bar.as<Baz>();
    EXPECT_EQ(baz, nullptr);
    auto foo = bar.as<Foo>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 8);
}

TEST_F(UnsafePtr_Tests, type) {
    StackPtr<Foo> foo = make_tracked<Foo>(10);
    UnsafePtr<Bar> bar = foo;
    EXPECT_EQ(bar.type(), typeid(Foo));
}

TEST_F(UnsafePtr_Tests, metadata) {
    StackPtr<Foo> foo = make_tracked<Foo>(12);
    UnsafePtr<Foo> ufoo = foo;
    UnsafePtr<Bar> bar = ufoo;
    EXPECT_EQ(ufoo.metadata(), nullptr);
    EXPECT_EQ(bar.metadata(), nullptr);
    struct {
        void operator()(void* p) {
            EXPECT_EQ(((Foo*)p)->get_value(), 12);
        }
    } metadata;
    set_metadata<Bar>(&metadata);
    EXPECT_EQ(bar.metadata(), nullptr);
    set_metadata<Foo>(&metadata);
    EXPECT_EQ(ufoo.metadata(), &metadata);
    EXPECT_EQ(bar.metadata(), &metadata);
    metadata(bar.get_base());
    set_metadata<Bar>(nullptr);
    set_metadata<Foo>(nullptr);
}

TEST_F(UnsafePtr_Tests, is_array) {
    StackPtr<Foo> foo = make_tracked<Foo>(12);
    UnsafePtr<Bar> bar = foo;
    EXPECT_FALSE(bar.is_array());
}

TEST_F(UnsafePtr_Tests, Comparisons) {
    StackPtr<Foo> p = make_tracked<Foo>(14);
    UnsafePtr<Foo> a = p;
    UnsafePtr<Foo> b = a;
    UniquePtr<Foo> c;

    EXPECT_FALSE(a == nullptr);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);

    EXPECT_FALSE(nullptr == a);
    EXPECT_TRUE(b == a);
    EXPECT_FALSE(c == a);

    EXPECT_TRUE(a != nullptr);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);

    EXPECT_TRUE(nullptr != a);
    EXPECT_FALSE(b != a);
    EXPECT_TRUE(c != a);

    EXPECT_FALSE(a < nullptr);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a < c);

    EXPECT_TRUE(nullptr < a);
    EXPECT_FALSE(b < a);
    EXPECT_TRUE(c < a);

    EXPECT_FALSE(a <= nullptr);
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(a <= c);

    EXPECT_TRUE(nullptr <= a);
    EXPECT_TRUE(b <= a);
    EXPECT_TRUE(c <= a);

    EXPECT_TRUE(a > nullptr);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a > c);

    EXPECT_FALSE(nullptr > a);
    EXPECT_FALSE(b > a);
    EXPECT_FALSE(c > a);

    EXPECT_TRUE(a >= nullptr);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(a >= c);

    EXPECT_FALSE(nullptr >= a);
    EXPECT_TRUE(b >= a);
    EXPECT_FALSE(c >= a);
}

TEST_F(UnsafePtr_Tests, Casts) {
    StackPtr<Foo> sfoo = make_tracked<Foo>(1);
    UnsafePtr<Bar> bar = sfoo;
    auto foo = static_pointer_cast<Foo>(bar);
    EXPECT_EQ(foo->value, 1);
    auto far = dynamic_pointer_cast<Far>(bar);
    EXPECT_EQ(far->value, 2);
    auto faz = dynamic_pointer_cast<Faz>(bar);
    EXPECT_EQ(faz->value, 3);
    sfoo = make_tracked<Foo>(5);
    UnsafePtr<const Bar> cbar = sfoo;
    auto pbar = const_pointer_cast<Bar>(cbar);
    EXPECT_EQ(pbar->get_value(), 5);
}

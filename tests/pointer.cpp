//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

struct Pointer_Tests : ::testing::Test {};

TEST_F(Pointer_Tests, DefaultConstructor) {
    StackPtr<int> ptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_Tests, NullConstructor) {
    StackPtr<int> ptr(nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_Tests, UniqueConstructor) {
    StackPtr<Bar> ptr = make_tracked<Foo>(3);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->get_value(), 3);
}

TEST_F(Pointer_Tests, RawConstructor) {
    StackPtr<Foo> foo = make_tracked<Foo>(10);
    StackPtr<int> alias(&foo->value);
    ASSERT_NE(alias, nullptr);
    EXPECT_EQ(*alias, 10);
}

TEST_F(Pointer_Tests, TrackedConstructor) {
    auto foo = make_tracked<Foo>(7);
    StackPtr<Bar> bar = foo->ptr;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 7);
}

TEST_F(Pointer_Tests, UnsafeConstructor) {
    auto foo = make_tracked<Foo>(5);
    UnsafePtr<Baz> baz = foo->ptr;
    StackPtr<Bar> bar = baz;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 5);
}

TEST_F(Pointer_Tests, CopyConstructor) {
    StackPtr<int> ptr1 = make_tracked<int>(8);
    StackPtr<int> ptr2 = ptr1;
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 8);
}

TEST_F(Pointer_Tests, CopyCastConstructor) {
    StackPtr<Foo> foo = make_tracked<Foo>(4);
    StackPtr<Bar> bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 4);
}

TEST_F(Pointer_Tests, CopyAssignmentOperator) {
    StackPtr<int> ptr1 = make_tracked<int>(3);
    StackPtr<int> ptr2;
    ptr2 = ptr1;
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 3);
}

TEST_F(Pointer_Tests, CopyCastAssignmentOperator) {
    StackPtr<Foo> foo = make_tracked<Foo>(5);
    StackPtr<Bar> bar;
    bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 5);
}

TEST_F(Pointer_Tests, NullAssignmentOperator) {
    StackPtr<int> ptr = make_tracked<int>(8);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 8);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_Tests, UniqueCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(11);
    StackPtr<Bar> bar;
    bar = std::move(foo);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 11);
}

TEST_F(Pointer_Tests, TrackedCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(4);
    StackPtr<Bar> bar;
    bar = foo->ptr;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 4);
}

TEST_F(Pointer_Tests, UnsafeCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(2);
    UnsafePtr<Baz> baz = foo->ptr;
    StackPtr<Bar> bar;
    bar = baz;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 2);
}

TEST_F(Pointer_Tests, TrackedRefOperator) {
    StackPtr<int> ptr = make_tracked<int>(3);
    TrackedPtr<int>& ref = ptr;
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ref, 3);
}

TEST_F(Pointer_Tests, VoidTrackedRefOperator) {
    StackPtr<int> ptr = make_tracked<int>(3);
    ASSERT_NE(ptr, nullptr);
    TrackedPtr<void>& ref = ptr;
    ASSERT_NE(ref, nullptr);
}

TEST_F(Pointer_Tests, BoolOperator) {
    StackPtr<int> ptr = make_tracked<int>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST_F(Pointer_Tests, IndirectionOperator) {
    StackPtr<int> ptr = make_tracked<int>(15);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, *ptr.get());
}

TEST_F(Pointer_Tests, StructureDereferenceOperator) {
    StackPtr<Foo> ptr = make_tracked<Foo>(3);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, ptr.get()->value);
}

TEST_F(Pointer_Tests, reset) {
    StackPtr<int> ptr = make_tracked<int>(9);
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_Tests, swap) {
    StackPtr<int> ptr1 = make_tracked<int>(2);
    StackPtr<int> ptr2 = make_tracked<int>(5);
    ptr1.swap(ptr2);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr1, 5);
    EXPECT_EQ(*ptr2, 2);
}

TEST_F(Pointer_Tests, clone) {
    StackPtr<int> ptr1 = make_tracked<int>(4);
    auto ptr2 = ptr1.clone();
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr1, *ptr2);
}

TEST_F(Pointer_Tests, is) {
    StackPtr<Foo> foo = make_tracked<Foo>(6);
    StackPtr<Bar> bar = foo;
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar>());
    StackPtr<int> alias(&foo->value);
    EXPECT_TRUE(alias.is<Foo>());
    EXPECT_FALSE(alias.is<int>());
}

TEST_F(Pointer_Tests, as) {
    StackPtr<Bar> bar = make_tracked<Foo>(8);
    StackPtr<Baz> baz = bar.as<Baz>();
    EXPECT_EQ(baz, nullptr);
    StackPtr<Foo> foo = bar.as<Foo>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 8);
    StackPtr<int> alias(&foo->value);
    *alias = 12;
    foo = alias.as<Foo>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 12);
}

TEST_F(Pointer_Tests, type) {
    StackPtr<Bar> bar = make_tracked<Foo>(10);
    EXPECT_EQ(bar.type(), typeid(Foo));
}

TEST_F(Pointer_Tests, metadata) {
    StackPtr<Foo> foo = make_tracked<Foo>(12);
    StackPtr<Bar> bar = foo;
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

TEST_F(Pointer_Tests, is_array) {
    StackPtr<Bar> bar = make_tracked<Foo>(14);
    EXPECT_FALSE(bar.is_array());
}

TEST_F(Pointer_Tests, Comparisons) {
    StackPtr<Foo> a = make_tracked<Foo>(14);
    StackPtr<Foo> b = a;
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

TEST_F(Pointer_Tests, Casts) {
    StackPtr<Bar> bar = make_tracked<Foo>(1);
    auto foo = static_pointer_cast<Foo>(bar);
    EXPECT_EQ(foo->value, 1);
    auto far = dynamic_pointer_cast<Far>(bar);
    EXPECT_EQ(far->value, 2);
    auto faz = dynamic_pointer_cast<Faz>(bar);
    EXPECT_EQ(faz->value, 3);
    StackPtr<const Bar> cbar = make_tracked<Foo>(5);
    auto pbar = const_pointer_cast<Bar>(cbar);
    EXPECT_EQ(pbar->get_value(), 5);
}

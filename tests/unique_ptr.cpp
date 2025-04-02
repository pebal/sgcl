//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

TEST(UniquePtr_Tests, DefaultConstructor) {
    unique_ptr<int> ptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST(UniquePtr_Tests, NullConstructor) {
    unique_ptr<int> ptr(nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST(UniquePtr_Tests, PrivUniqueConstructor) {
    auto ptr = make_tracked<int>(7);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 7);
}

TEST(UniquePtr_Tests, MoveConstructor) {
    auto foo = make_tracked<Foo>(3);
    unique_ptr<Foo> foo2(std::move(foo));
    EXPECT_EQ(foo, nullptr);
    ASSERT_NE(foo2, nullptr);
    EXPECT_EQ(foo2->get_value(), 3);
}

TEST(UniquePtr_Tests, MoveCastConstructor) {
    auto foo = make_tracked<Foo>(4);
    unique_ptr<Bar> bar(std::move(foo));
    EXPECT_EQ(foo, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 4);
}

TEST(UniquePtr_Tests, MoveAssignmentOperator) {
    auto ptr = make_tracked<int>(5);
    unique_ptr<int> ptr2;
    ptr2 = std::move(ptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 5);
}

TEST(UniquePtr_Tests, MoveCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(6);
    unique_ptr<Bar> bar;
    bar = std::move(foo);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 6);
}

TEST(UniquePtr_Tests, NullAssignmentOperator) {
    auto ptr = make_tracked<int>(8);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 8);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST(UniquePtr_Tests, VoidRefOperator) {
    auto foo = make_tracked<Foo>(8);
    unique_ptr<void>& bar = foo;
    ASSERT_NE(bar, nullptr);
}

TEST(UniquePtr_Tests, BoolOperator) {
    auto ptr = make_tracked<int>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST(UniquePtr_Tests, IndirectionOperator) {
    auto ptr = make_tracked<int>(15);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, *ptr.get());
}

TEST(UniquePtr_Tests, StructureDereferenceOperator) {
    auto foo = make_tracked<Foo>(3);
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, foo.get()->value);
}

TEST(UniquePtr_Tests, reset) {
    auto ptr = make_tracked<int>(9);
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

TEST(UniquePtr_Tests, swap) {
    auto ptr1 = make_tracked<int>(2);
    auto ptr2 = make_tracked<int>(5);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ptr1.swap(ptr2);
    EXPECT_EQ(*ptr1, 5);
    EXPECT_EQ(*ptr2, 2);
}

TEST(UniquePtr_Tests, is) {
    unique_ptr<Bar> bar = make_tracked<Foo>(6);
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar>());
}

TEST(UniquePtr_Tests, as) {
    unique_ptr<Bar> bar = make_tracked<Foo>(8);
    auto foo = bar.as<Foo>();
    EXPECT_EQ(bar, nullptr);
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 8);
}

TEST(UniquePtr_Tests, type) {
    auto bar = make_tracked<Foo>(10);
    EXPECT_EQ(bar.type(), typeid(Foo));
}

TEST(UniquePtr_Tests, metadata) {
    auto ptr = make_tracked<int>(12);
    struct {
        void operator()(void* p) {
            EXPECT_EQ(*(int*)p, 12);
        }
    } metadata;
    EXPECT_EQ(ptr.metadata(), nullptr);
    set_metadata<int>(&metadata);
    EXPECT_EQ(ptr.metadata(), &metadata);
    metadata(ptr.get());
    set_metadata<int>(nullptr);
}

TEST(UniquePtr_Tests, is_array) {
    unique_ptr<Bar> bar(make_tracked<Foo>(14));
    EXPECT_FALSE(bar.is_array());
}

TEST(UniquePtr_Tests, Comparisons) {
    auto a = make_tracked<Foo>(14);
    auto b = make_tracked<Foo>(14);

    EXPECT_FALSE(a == nullptr);
    EXPECT_FALSE(a == b);

    EXPECT_FALSE(nullptr == a);
    EXPECT_FALSE(b == a);

    EXPECT_TRUE(a != nullptr);
    EXPECT_TRUE(a != b);

    EXPECT_TRUE(nullptr != a);
    EXPECT_TRUE(b != a);

    EXPECT_FALSE(a < nullptr);
    EXPECT_EQ(a < b, a.get() < b.get());

    EXPECT_TRUE(nullptr < a);
    EXPECT_EQ(b < a, b.get() < a.get());

    EXPECT_FALSE(a <= nullptr);
    EXPECT_EQ(a <= b, a.get() <= b.get());

    EXPECT_TRUE(nullptr <= a);
    EXPECT_EQ(b <= a, b.get() <= a.get());

    EXPECT_TRUE(a > nullptr);
    EXPECT_EQ(a > b, a.get() > b.get());

    EXPECT_FALSE(nullptr > a);
    EXPECT_EQ(b > a, b.get() > a.get());

    EXPECT_TRUE(a >= nullptr);
    EXPECT_EQ(a >= b, a.get() >= b.get());

    EXPECT_FALSE(nullptr >= a);
    EXPECT_EQ(b >= a, b.get() >= a.get());
}

TEST(UniquePtr_Tests, Casts) {
    unique_ptr<Bar> bar = make_tracked<Foo>(1);
    auto foo = static_pointer_cast<Foo>(std::move(bar));
    EXPECT_EQ(bar, nullptr);
    EXPECT_EQ(foo->value, 1);
    auto far = dynamic_pointer_cast<Far>(std::move(foo));
    EXPECT_EQ(foo, nullptr);
    EXPECT_EQ(far->value, 2);
    unique_ptr<Faz> faz = dynamic_pointer_cast<Faz>(std::move(far));
    EXPECT_EQ(far, nullptr);
    EXPECT_EQ(faz->value, 3);
    unique_ptr<const Bar> cbar = make_tracked<const Foo>(5);
    auto pbar = const_pointer_cast<Bar>(std::move(cbar));
    EXPECT_EQ(cbar, nullptr);
    EXPECT_EQ(pbar->get_value(), 5);
}

//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------

#include "types.h"

struct UnsafePtr_ArrayTests : ::testing::Test {};

TEST_F(UnsafePtr_ArrayTests, DefaultConstructor) {
    UnsafePtr<int[]> ptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UnsafePtr_ArrayTests, NullConstructor) {
    UnsafePtr<int[]> ptr(nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UnsafePtr_ArrayTests, PointerConstructor) {
    StackPtr<Foo[]> foo = make_tracked<Foo[]>(3, 2);
    UnsafePtr<Bar[]> bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[2].get_value(), 2);
}

TEST_F(UnsafePtr_ArrayTests, CopyConstructor) {
    StackPtr<Foo[]> sfoo = make_tracked<Foo[]>(3, 8);
    UnsafePtr<Foo[]> foo = sfoo;
    UnsafePtr<Bar[]> bar = foo;
    ASSERT_NE(foo, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[2].get_value(), 8);
}

TEST_F(UnsafePtr_ArrayTests, CopyAssignmentOperator) {
    StackPtr<Foo[]> sfoo = make_tracked<Foo[]>(2, 9);
    UnsafePtr<Foo[]> foo = sfoo;
    UnsafePtr<Bar[]> bar;
    bar = foo;
    ASSERT_NE(foo, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[1].get_value(), 9);
}

TEST_F(UnsafePtr_ArrayTests, NullAssignmentOperator) {
    StackPtr<int[]> sptr = make_tracked<int[]>(3);
    UnsafePtr<int[]> ptr = sptr;
    EXPECT_NE(ptr, nullptr);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UnsafePtr_ArrayTests, BoolOperator) {
    StackPtr<int[]> ptr = make_tracked<int>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST_F(UnsafePtr_ArrayTests, reset) {
    StackPtr<int> sptr = make_tracked<int>(5);
    UnsafePtr<int[]> ptr = sptr;
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST_F(UnsafePtr_ArrayTests, swap) {
    StackPtr<int[]> sptr1 = make_tracked<int[]>(2, 2);
    StackPtr<int[]> sptr2 = make_tracked<int[]>(2, 5);
    UnsafePtr<int[]> ptr1 = sptr1;
    UnsafePtr<int[]> ptr2 = sptr2;
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ptr1.swap(ptr2);
    EXPECT_EQ(ptr1[1], 5);
    EXPECT_EQ(ptr2[1], 2);
}

TEST_F(UnsafePtr_ArrayTests, clone) {
    StackPtr<int[]> sptr1 = make_tracked<int>(4);
    UnsafePtr<int[]> ptr1 = sptr1;
    auto ptr2 = ptr1.clone();
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr1[0], ptr2[0]);
}

TEST_F(UnsafePtr_ArrayTests, is) {
    StackPtr<Foo[]> foo = make_tracked<Foo[]>(6);
    UnsafePtr<Bar[]> bar = foo;
    EXPECT_TRUE(bar.is<Foo[]>());
    EXPECT_FALSE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar[]>());
}

TEST_F(UnsafePtr_ArrayTests, as) {
    StackPtr<Foo[]> sfoo = make_tracked<Foo[]>(1, 8);
    UnsafePtr<Bar[]> bar = sfoo;
    auto foo = bar.as<Foo[]>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo[0].value, 8);
}

TEST_F(UnsafePtr_ArrayTests, type) {
    StackPtr<Foo[]> foo = make_tracked<Foo[]>(10);
    UnsafePtr<Bar[]> bar = foo;
    EXPECT_EQ(bar.type(), typeid(Foo[]));
    EXPECT_NE(bar.type(), typeid(Foo));
}

TEST_F(UnsafePtr_ArrayTests, metadata) {
    StackPtr<Foo[]> foo = make_tracked<Foo[]>(5, 12);
    UnsafePtr<Bar[]> bar = foo;
    struct {
        void operator()(void* p, size_t size) {
            auto begin = (Foo*)p;
            auto end = (Foo*)p + size;
            for(auto i = begin; i != end; ++i) {
                EXPECT_EQ(i->value, 12);
            }
        }
    } metadata;
    set_metadata<Foo[]>(&metadata);
    EXPECT_EQ(bar.metadata(), &metadata);
    metadata(bar.get_base(), bar.size());
    set_metadata<Foo[]>(nullptr);
}

TEST_F(UnsafePtr_ArrayTests, is_array) {
    StackPtr<Bar> sbar1 = make_tracked<Foo[]>(14);
    StackPtr<Bar[]> sbar2 = make_tracked<Foo>(14);
    UnsafePtr<Bar> bar1 = sbar1;
    UnsafePtr<Bar[]> bar2 = sbar2;
    EXPECT_TRUE(bar1.is_array());
    EXPECT_FALSE(bar2.is_array());
}

TEST_F(UnsafePtr_ArrayTests, size) {
    StackPtr<int[]> sarr = make_tracked<int[]>({1, 2, 3});
    StackPtr<int[]> sptr = make_tracked<int>();
    UnsafePtr<int[]> arr = sarr;
    UnsafePtr<int[]> ptr = sptr;
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(ptr.size(), 1);
    ptr.reset();
    arr.reset();
    EXPECT_EQ(ptr.size(), 0);
    EXPECT_EQ(arr.size(), 0);
}

TEST_F(UnsafePtr_ArrayTests, index_operator) {
    StackPtr<int[]> sarr = make_tracked<int[]>({0, 1, 2});
    UnsafePtr<int[]> arr = sarr;
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], i);
    }
}

TEST_F(UnsafePtr_ArrayTests, at) {
    StackPtr<int[]> sarr = make_tracked<int[]>({0, 1, 2, 3, 4});
    UnsafePtr<int[]> arr = sarr;
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr.at(i), i);
    }
}

TEST_F(UnsafePtr_ArrayTests, iterators) {
    StackPtr<int[]> sarr = make_tracked<int[]>({1, 2, 3});
    UnsafePtr<int[]> arr = sarr;
    int c = 0;
    for (auto v : arr) {
        EXPECT_EQ(v, ++c);
    }
    for (auto i = arr.rbegin(); i < arr.rend(); ++i) {
        *i = c--;
    }
    for (auto i = arr.cbegin(); i < arr.cend(); ++i) {
        EXPECT_EQ(*i, ++c);
    }
    for (auto i = arr.begin(); i < arr.end(); ++i) {
        *i = c--;
    }
    for (auto i = arr.crbegin(); i < arr.crend(); ++i) {
        EXPECT_EQ(*i, ++c);
    }
}

TEST_F(UnsafePtr_ArrayTests, Comparisons) {
    StackPtr<Foo[]> p = make_tracked<Foo[]>(3);
    UnsafePtr<Foo[]> a = p;
    UnsafePtr<Foo[]> b = a;
    StackPtr<Foo[]> c;

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

TEST_F(UnsafePtr_ArrayTests, Casts) {
    StackPtr<Foo[]> sfoo = make_tracked<Foo>(1);
    UnsafePtr<Bar[]> bar = sfoo;
    auto foo = static_pointer_cast<Foo[]>(bar);
    EXPECT_EQ(foo[0].value, 1);
    auto far = dynamic_pointer_cast<Far[]>(bar);
    EXPECT_EQ(far[0].value, 2);
    auto faz = dynamic_pointer_cast<Faz[]>(bar);
    EXPECT_EQ(faz[0].value, 3);
    sfoo = make_tracked<Foo>(5);
    UnsafePtr<const Bar[]> cbar = sfoo;
    auto pbar = const_pointer_cast<Bar[]>(cbar);
    EXPECT_EQ(pbar[0].get_value(), 5);
}

//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

struct Pointer_ArrayTests : ::testing::Test {};

TEST_F(Pointer_ArrayTests, DefaultConstructor) {
    StackPtr<int[]> ptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_ArrayTests, NullConstructor) {
    StackPtr<int[]> ptr(nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_ArrayTests, UniqueConstructor) {
    StackPtr<Bar[]> bar = make_tracked<Foo[]>(3, 2);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[2].get_value(), 2);
}

TEST_F(Pointer_ArrayTests, RawConstructor) {
    StackPtr<Foo> foo = make_tracked<Foo>(10);
    StackPtr<int[]> alias(&foo->value);
    ASSERT_NE(alias, nullptr);
    EXPECT_EQ(alias[0], 10);
}

TEST_F(Pointer_ArrayTests, TrackedConstructor) {
    auto foo = make_tracked<Foo>(7);
    StackPtr<Bar[]> bar = foo->ptr;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 7);
}

TEST_F(Pointer_ArrayTests, UnsafeConstructor) {
    auto foo = make_tracked<Foo>(5);
    UnsafePtr<Baz[]> baz = foo->ptr;
    StackPtr<Bar[]> bar = baz;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 5);
}

TEST_F(Pointer_ArrayTests, CopyConstructor) {
    StackPtr<int[]> ptr1 = make_tracked<int[]>(3, 8);
    StackPtr<int[]> ptr2 = ptr1;
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(ptr2[2], 8);
}

TEST_F(Pointer_ArrayTests, CopyCastConstructor) {
    StackPtr<Foo> foo = make_tracked<Foo>(4);
    StackPtr<Bar[]> bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 4);
}

TEST_F(Pointer_ArrayTests, CopyAssignmentOperator) {
    StackPtr<int[]> ptr1 = make_tracked<int>(3);
    StackPtr<int[]> ptr2;
    ptr2 = ptr1;
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2[0], 3);
}

TEST_F(Pointer_ArrayTests, CopyCastAssignmentOperator) {
    StackPtr<Foo[]> foo = make_tracked<Foo>(5);
    StackPtr<Bar[]> bar;
    bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 5);
}

TEST_F(Pointer_ArrayTests, NullAssignmentOperator) {
    StackPtr<int[]> ptr = make_tracked<int>(8);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[0], 8);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_ArrayTests, UniqueCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(11);
    StackPtr<Bar[]> bar;
    bar = std::move(foo);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 11);
}

TEST_F(Pointer_ArrayTests, TrackedCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(4);
    StackPtr<Bar[]> bar;
    bar = foo->ptr;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 4);
}

TEST_F(Pointer_ArrayTests, UnsafeCastAssignmentOperator) {
    auto other = make_tracked<Foo>(2);
    UnsafePtr<Baz[]> baz = other->ptr;
    StackPtr<Bar[]> bar;
    bar = baz;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 2);
}

TEST_F(Pointer_ArrayTests, TrackedRefOperator) {
    StackPtr<int[]> ptr = make_tracked<int>(3);
    TrackedPtr<int[]>& ref = ptr;
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ref[0], 3);
}

TEST_F(Pointer_ArrayTests, VoidTrackedRefOperator) {
    StackPtr<int[]> ptr = make_tracked<int>(3);
    ASSERT_NE(ptr, nullptr);
    TrackedPtr<void>& ref = ptr;
    ASSERT_NE(ref, nullptr);
}

TEST_F(Pointer_ArrayTests, BoolOperator) {
    StackPtr<int[]> ptr = make_tracked<int>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST_F(Pointer_ArrayTests, reset) {
    StackPtr<int[]> ptr = make_tracked<int>(9);
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(Pointer_ArrayTests, swap) {
    StackPtr<int[]> ptr1 = make_tracked<int[]>(2, 2);
    StackPtr<int[]> ptr2 = make_tracked<int[]>(2, 5);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ptr1.swap(ptr2);
    EXPECT_EQ(ptr1[1], 5);
    EXPECT_EQ(ptr2[1], 2);
}

TEST_F(Pointer_ArrayTests, clone) {
    StackPtr<int[]> ptr1 = make_tracked<int>(4);
    auto ptr2 = ptr1.clone();
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr1[0], ptr2[0]);
}

TEST_F(Pointer_ArrayTests, is) {
    StackPtr<Bar[]> bar = make_tracked<Foo[]>(6);
    EXPECT_TRUE(bar.is<Foo[]>());
    EXPECT_FALSE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar[]>());
}

TEST_F(Pointer_ArrayTests, as) {
    StackPtr<Bar[]> bar = make_tracked<Foo[]>(1, 8);
    auto foo = bar.as<Foo[]>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo[0].value, 8);
}

TEST_F(Pointer_ArrayTests, type) {
    StackPtr<Bar[]> bar = make_tracked<Foo[]>(10);
    EXPECT_EQ(bar.type(), typeid(Foo[]));
    EXPECT_NE(bar.type(), typeid(Foo));
}

TEST_F(Pointer_ArrayTests, metadata) {
    StackPtr<Bar[]> bar = make_tracked<Foo[]>(5, 12);
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

TEST_F(Pointer_ArrayTests, is_array) {
    StackPtr<Bar> bar1 = make_tracked<Foo[]>(14);
    StackPtr<Bar[]> bar2 = make_tracked<Foo>(14);
    EXPECT_TRUE(bar1.is_array());
    EXPECT_FALSE(bar2.is_array());
}

TEST_F(Pointer_ArrayTests, size) {
    StackPtr<int[]> arr = make_tracked<int[]>({1, 2, 3});
    StackPtr<int[]> ptr = make_tracked<int>();
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(ptr.size(), 1);
    ptr.reset();
    EXPECT_EQ(ptr.size(), 0);
    arr = make_tracked<int[]>(5);
    EXPECT_EQ(arr.size(), 5);
    arr = make_tracked<int[]>(7, 5);
    EXPECT_EQ(arr.size(), 7);
}

TEST_F(Pointer_ArrayTests, index_operator) {
    StackPtr<int[]> arr = make_tracked<int[]>({0, 1, 2});
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], i);
    }
}

TEST_F(Pointer_ArrayTests, at) {
    StackPtr<int[]> arr = make_tracked<int[]>({0, 1, 2, 3, 4});
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr.at(i), i);
    }
}

TEST_F(Pointer_ArrayTests, iterators) {
    StackPtr<int[]> arr = make_tracked<int[]>({1, 2, 3});
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

TEST_F(Pointer_ArrayTests, Comparisons) {
    StackPtr<Foo[]> a = make_tracked<Foo[]>(3);
    StackPtr<Foo[]> b = a;
    UniquePtr<Foo[]> c;

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

TEST_F(Pointer_ArrayTests, Casts) {
    StackPtr<Bar[]> bar = make_tracked<Foo[]>(3, 3);
    auto foo = static_pointer_cast<Foo[]>(bar);
    EXPECT_EQ(foo[2].value, 3);
    auto far = dynamic_pointer_cast<Far[]>(foo);
    EXPECT_EQ(far[1].value, 4);
    auto faz = dynamic_pointer_cast<Faz[]>(far);
    EXPECT_EQ(faz[2].value, 5);
    StackPtr<const Bar[]> cbar = make_tracked<const Foo[]>(5, 2);
    auto pbar = const_pointer_cast<Bar[]>(cbar);
    EXPECT_EQ(pbar[4].get_value(), 2);
}

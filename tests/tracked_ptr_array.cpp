//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

TEST(Pointer_ArrayTests, DefaultConstructor) {
    tracked_ptr<int[]> ptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST(Pointer_ArrayTests, NullConstructor) {
    tracked_ptr<int[]> ptr(nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST(Pointer_ArrayTests, UniqueConstructor) {
    tracked_ptr<Bar[]> bar = make_tracked<Foo[]>(3, 2);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[2].get_value(), 2);
}

TEST(Pointer_ArrayTests, RawConstructor) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(10);
    tracked_ptr<int[]> alias(&foo->value);
    ASSERT_NE(alias, nullptr);
    EXPECT_EQ(alias[0], 10);
}

TEST(Pointer_ArrayTests, CopyConstructor) {
    tracked_ptr<int[]> ptr1 = make_tracked<int[]>(3, 8);
    tracked_ptr<int[]> ptr2 = ptr1;
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(ptr2[2], 8);
}

TEST(Pointer_ArrayTests, CopyCastConstructor) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(4);
    tracked_ptr<Bar[]> bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 4);
}

TEST(Pointer_ArrayTests, CopyAssignmentOperator) {
    tracked_ptr<int[]> ptr1 = make_tracked<int>(3);
    tracked_ptr<int[]> ptr2;
    ptr2 = ptr1;
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2[0], 3);
}

TEST(Pointer_ArrayTests, CopyCastAssignmentOperator) {
    tracked_ptr<Foo[]> foo = make_tracked<Foo>(5);
    tracked_ptr<Bar[]> bar;
    bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 5);
}

TEST(Pointer_ArrayTests, NullAssignmentOperator) {
    tracked_ptr<int[]> ptr = make_tracked<int>(8);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[0], 8);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST(Pointer_ArrayTests, UniqueCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(11);
    tracked_ptr<Bar[]> bar;
    bar = std::move(foo);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[0].get_value(), 11);
}

TEST(Pointer_ArrayTests, VoidRefOperator) {
    tracked_ptr<int[]> ptr = make_tracked<int>(3);
    ASSERT_NE(ptr, nullptr);
    tracked_ptr<void>& ref = ptr;
    ASSERT_NE(ref, nullptr);
}

TEST(Pointer_ArrayTests, BoolOperator) {
    tracked_ptr<int[]> ptr = make_tracked<int>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST(Pointer_ArrayTests, reset) {
    tracked_ptr<int[]> ptr = make_tracked<int>(9);
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

TEST(Pointer_ArrayTests, swap) {
    tracked_ptr<int[]> ptr1 = make_tracked<int[]>(2, 2);
    tracked_ptr<int[]> ptr2 = make_tracked<int[]>(2, 5);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ptr1.swap(ptr2);
    EXPECT_EQ(ptr1[1], 5);
    EXPECT_EQ(ptr2[1], 2);
}

TEST(Pointer_ArrayTests, is) {
    tracked_ptr<Bar[]> bar = make_tracked<Foo[]>(6);
    EXPECT_TRUE(bar.is<Foo[]>());
    EXPECT_FALSE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar[]>());
}

TEST(Pointer_ArrayTests, as) {
    tracked_ptr<Bar[]> bar = make_tracked<Foo[]>(1, 8);
    auto foo = bar.as<Foo[]>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo[0].value, 8);
}

TEST(Pointer_ArrayTests, type) {
    tracked_ptr<Bar[]> bar = make_tracked<Foo[]>(10);
    EXPECT_EQ(bar.type(), typeid(Foo[]));
    EXPECT_NE(bar.type(), typeid(Foo));
}

TEST(Pointer_ArrayTests, metadata) {
    tracked_ptr<Bar[]> bar = make_tracked<Foo[]>(5, 12);
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

TEST(Pointer_ArrayTests, is_array) {
    tracked_ptr<Bar> bar1 = make_tracked<Foo[]>(14);
    tracked_ptr<Bar[]> bar2 = make_tracked<Foo>(14);
    EXPECT_TRUE(bar1.is_array());
    EXPECT_FALSE(bar2.is_array());
}

TEST(Pointer_ArrayTests, size) {
    tracked_ptr<int[]> arr = make_tracked<int[]>({1, 2, 3});
    tracked_ptr<int[]> ptr = make_tracked<int>();
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(ptr.size(), 1);
    ptr.reset();
    EXPECT_EQ(ptr.size(), 0);
    arr = make_tracked<int[]>(5);
    EXPECT_EQ(arr.size(), 5);
    arr = make_tracked<int[]>(7, 5);
    EXPECT_EQ(arr.size(), 7);
}

TEST(Pointer_ArrayTests, index_operator) {
    tracked_ptr<int[]> arr = make_tracked<int[]>({0, 1, 2});
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], i);
    }
}

TEST(Pointer_ArrayTests, at) {
    tracked_ptr<int[]> arr = make_tracked<int[]>({0, 1, 2, 3, 4});
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr.at(i), i);
    }
    EXPECT_THROW(arr.at(10), std::out_of_range);
}

TEST(Pointer_ArrayTests, iterators) {
    tracked_ptr<int[]> arr = make_tracked<int[]>({1, 2, 3});
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

TEST(Pointer_ArrayTests, Comparisons) {
    tracked_ptr<Foo[]> a = make_tracked<Foo[]>(3);
    tracked_ptr<Foo[]> b = a;

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

TEST(Pointer_ArrayTests, Casts) {
    tracked_ptr<Bar[]> bar = make_tracked<Foo[]>(3, 3);
    auto foo = static_pointer_cast<Foo[]>(bar);
    EXPECT_EQ(foo[2].value, 3);
    auto far = dynamic_pointer_cast<Far[]>(foo);
    EXPECT_EQ(far[1].value, 4);
    auto faz = dynamic_pointer_cast<Faz[]>(far);
    EXPECT_EQ(faz[2].value, 5);
    tracked_ptr<const Bar[]> cbar = make_tracked<const Foo[]>(5, 2);
    auto pbar = const_pointer_cast<Bar[]>(cbar);
    EXPECT_EQ(pbar[4].get_value(), 2);
}

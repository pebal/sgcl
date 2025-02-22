//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

struct UniquePtr_ArrayTests : ::testing::Test {};

TEST_F(UniquePtr_ArrayTests, DefaultConstructor) {
    UniquePtr<int[]> ptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UniquePtr_ArrayTests, NullConstructor) {
    UniquePtr<int[]> ptr(nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UniquePtr_ArrayTests, PrivUniqueConstructor) {
    auto ptr = make_tracked<int[]>({1, 2, 3});
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[0], 1);
}

TEST_F(UniquePtr_ArrayTests, MoveConstructor) {
    auto foo = make_tracked<Foo[]>(2, 3);
    UniquePtr<Foo[]> foo2(std::move(foo));
    EXPECT_EQ(foo, nullptr);
    ASSERT_NE(foo2, nullptr);
    EXPECT_EQ(foo2[1].get_value(), 3);
}

TEST_F(UniquePtr_ArrayTests, MoveAssignmentOperator) {
    auto ptr = make_tracked<int[]>({5, 6, 7});
    UniquePtr<int[]> ptr2;
    ptr2 = std::move(ptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2[2], 7);
}

TEST_F(UniquePtr_ArrayTests, MoveCastAssignmentOperator) {
    auto foo = make_tracked<Foo[]>(5, 6);
    UniquePtr<Bar[]> bar;
    bar = std::move(foo);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar[4].get_value(), 6);
}

TEST_F(UniquePtr_ArrayTests, NullAssignmentOperator) {
    auto ptr = make_tracked<int[]>(3, 8);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[2], 8);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UniquePtr_ArrayTests, VoidRefOperator) {
    auto foo = make_tracked<Foo[]>(2, 8);
    UniquePtr<void>& bar = foo;
    ASSERT_NE(bar, nullptr);
}

TEST_F(UniquePtr_ArrayTests, BoolOperator) {
    auto ptr = make_tracked<int[]>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST_F(UniquePtr_ArrayTests, reset) {
    auto ptr = make_tracked<int[]>(2, 9);
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(UniquePtr_ArrayTests, swap) {
    auto ptr1 = make_tracked<int[]>(3, 2);
    auto ptr2 = make_tracked<int[]>(3, 5);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ptr1.swap(ptr2);
    EXPECT_EQ(ptr1[2], 5);
    EXPECT_EQ(ptr2[2], 2);
}

TEST_F(UniquePtr_ArrayTests, clone) {
    auto ptr1 = make_tracked<int[]>(4, 8);
    auto ptr2 = ptr1.clone();
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2[1], 8);
    EXPECT_EQ(ptr1[3], ptr2[3]);
}

TEST_F(UniquePtr_ArrayTests, is) {
    UniquePtr<Bar[]> bar = make_tracked<Foo[]>(6);
    EXPECT_TRUE(bar.is<Foo[]>());
    EXPECT_FALSE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar[]>());
}

TEST_F(UniquePtr_ArrayTests, as) {
    UniquePtr<Bar[]> bar = make_tracked<Foo[]>(1, 8);
    auto foo = bar.as<Foo[]>();
    EXPECT_EQ(bar, nullptr);
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo[0].value, 8);
}

TEST_F(UniquePtr_ArrayTests, type) {
    auto bar = make_tracked<Foo[]>(10);
    EXPECT_EQ(bar.type(), typeid(Foo[]));
    EXPECT_NE(bar.type(), typeid(Foo));
}

TEST_F(UniquePtr_ArrayTests, metadata) {
    UniquePtr<Bar[]> bar = make_tracked<Foo[]>(5, 12);
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

TEST_F(UniquePtr_ArrayTests, is_array) {
    UniquePtr<Bar> bar1 = make_tracked<Foo[]>(14);
    UniquePtr<Bar[]> bar2 = make_tracked<Foo>(14);
    EXPECT_TRUE(bar1.is_array());
    EXPECT_FALSE(bar2.is_array());
}

TEST_F(UniquePtr_ArrayTests, size) {
    auto arr = make_tracked<int[]>({1, 2, 3});
    UniquePtr<int[]> ptr = make_tracked<int>();
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(ptr.size(), 1);
    ptr.reset();
    EXPECT_EQ(ptr.size(), 0);
    arr = make_tracked<int[]>(5);
    EXPECT_EQ(arr.size(), 5);
    arr = make_tracked<int[]>(7, 5);
    EXPECT_EQ(arr.size(), 7);
}

TEST_F(UniquePtr_ArrayTests, index_operator) {
    auto arr = make_tracked<int[]>({0, 1, 2});
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], i);
    }
}

TEST_F(UniquePtr_ArrayTests, at) {
    auto arr = make_tracked<int[]>({0, 1, 2, 3, 4});
    for (int i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr.at(i), i);
    }
}

TEST_F(UniquePtr_ArrayTests, iterators) {
    auto arr = make_tracked<int[]>({1, 2, 3});
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

TEST_F(UniquePtr_ArrayTests, Comparisons) {
    auto a = make_tracked<Foo[]>(14);
    auto b = a.clone();

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

TEST_F(UniquePtr_ArrayTests, Casts) {
    UniquePtr<Bar[]> bar = make_tracked<Foo[]>(3, 3);
    auto foo = static_pointer_cast<Foo[]>(std::move(bar));
    EXPECT_EQ(bar, nullptr);
    EXPECT_EQ(foo[2].value, 3);
    auto far = dynamic_pointer_cast<Far[]>(std::move(foo));
    EXPECT_EQ(foo, nullptr);
    EXPECT_EQ(far[1].value, 4);
    UniquePtr<Faz[]> faz = dynamic_pointer_cast<Faz[]>(std::move(far));
    EXPECT_EQ(far, nullptr);
    EXPECT_EQ(faz[2].value, 5);
    UniquePtr<const Bar[]> cbar = make_tracked<const Foo[]>(5, 2);
    auto pbar = const_pointer_cast<Bar[]>(std::move(cbar));
    EXPECT_EQ(cbar, nullptr);
    EXPECT_EQ(pbar[4].get_value(), 2);
}

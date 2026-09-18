//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

TEST(Maker_Tests, DefaultConstructor) {
    struct S {
        char value = 2;
    };
    auto ptr = make_tracked<S>();
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, 2);
    auto tr = make_tracked<tracked_ptr<int>>();
    EXPECT_NE(tr, nullptr);
    EXPECT_EQ(*tr, nullptr);
}

TEST(Maker_Tests, ParameterConstructor) {
    auto ptr = make_tracked<int>(3);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 3);
}

// Managed arrays are internal to the containers; their construction paths
// are exercised through sgcl::vector (tests/vector.cpp).
TEST(Maker_Tests, InternalArrayThroughVector) {
    struct S {
        char value = 9;
    };
    sgcl::vector<S> s(3);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s[2].value, 9);
    sgcl::vector<tracked_ptr<int>> tr(3);
    EXPECT_EQ(tr[0], nullptr);
    EXPECT_EQ(tr[2], nullptr);
    sgcl::vector<int> big(7000, 5);
    EXPECT_EQ(big.size(), 7000u);
    for (size_t i = 0; i < big.size(); ++i) {
        if (big[i] != 5) {
            FAIL() << "element " << i;
        }
    }
    sgcl::vector<Foo> foo(3, 7);
    EXPECT_EQ(foo[2].get_value(), 7);
}

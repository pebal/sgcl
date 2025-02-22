//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

struct Maker_Tests : ::testing::Test {};

TEST_F(Maker_Tests, DefaultConstructor) {
    auto ptr = make_tracked<int>();
    EXPECT_NE(ptr, nullptr);
}

TEST_F(Maker_Tests, ParameterConstructor) {
    auto ptr = make_tracked<int>(3);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 3);
}

TEST_F(Maker_Tests, DefaultArrayConstructor) {
    auto ptr = make_tracked<int[]>(3);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(Maker_Tests, ParameterArrayConstructor) {
    auto ptr = make_tracked<int[]>(3, 5);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[2], 5);
}

TEST_F(Maker_Tests, InitializerListArrayConstructor) {
    auto ptr = make_tracked<int[]>({1, 2, 3});
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[0], 1);
    EXPECT_EQ(ptr[1], 2);
    EXPECT_EQ(ptr[2], 3);
}

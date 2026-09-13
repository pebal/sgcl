//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "static_init.h"

int static_init_inner_destroyed = 0;

// Initialized before main: the first StaticInitOuter and StaticInitInner of
// the program. A unique_ptr is the global root (README: The rules); the
// Inner hangs off it through a tracked_ptr the collector follows only
// through Outer's pointer map.
sgcl::unique_ptr<StaticInitOuter> g_static_init_outer = make_static_init_outer();

TEST(StaticInit_Tests, InnerSurvivesACycleInTheInitializer) {
    ASSERT_NE(g_static_init_outer, nullptr);
    EXPECT_EQ(static_init_inner_destroyed, 0);
    ASSERT_NE(g_static_init_outer->inner, nullptr);
    EXPECT_EQ(g_static_init_outer->inner->value, 42);
    sgcl::collector::force_collect(true);
    EXPECT_EQ(static_init_inner_destroyed, 0);
    EXPECT_EQ(sgcl::collector::get_live_object_count(), 2u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

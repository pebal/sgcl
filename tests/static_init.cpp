//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "static_init.h"

// Constructed during C++ static/dynamic initialization, before main() (and
// therefore before any TEST body) runs. This is the very first construction
// of StaticInitOuter (and, nested inside its constructor, the very first
// construction of StaticInitInner) anywhere in the program. This reproduces
// the scenario that used to crash: PageInfo<T>::child_pointers was a plain
// eagerly-initialized `inline static` data member (see sgcl/detail/page_info.h)
// whose own dynamic initialization order relative to *this* variable's
// initializer, across translation units, was unspecified by the standard. If
// this initializer ran before the other translation unit's own static
// initialization got around to constructing `child_pointers`, the nested
// make_tracked<StaticInitInner>() call inside StaticInitOuter's constructor
// would hit `Pointer`'s assertion:
//   Assertion failed: (offset / 8 < pointers.map->size()), function Pointer, ...
// The fix converts `child_pointers` into a lazily-constructed function-local
// static (see page_info.h), which the C++ standard guarantees is initialized
// on first use regardless of static-initialization order.
//
// This is built as its own standalone test binary (see tests/CMakeLists.txt)
// rather than folded into the shared `tests` executable: constructing a
// tracked object during static initialization -- before any SGCL machinery
// has necessarily run, and before the collector has otherwise been touched
// by the process -- is a whole-program concern, and this reproduction should
// not share collector/thread state with the rest of the (much larger) test
// suite, which asserts exact collector::get_live_object_count() baselines
// throughout.
tracked_ptr<StaticInitOuter> g_static_init_outer = make_static_init_outer();

TEST(StaticInit_Tests, GlobalConstructedDuringStaticInitialization) {
    ASSERT_NE(g_static_init_outer, nullptr);
    EXPECT_EQ(g_static_init_outer->value, 7);
    ASSERT_NE(g_static_init_outer->ptr, nullptr);
    EXPECT_EQ(g_static_init_outer->ptr->value, 42);
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/core/static_init.h"

// A translation unit of its own, linked after static_init.cpp (tests/
// CMakeLists.txt): the global that calls this is initialized first, so the
// pointer map of StaticInitOuter, an inline static data member before the
// fix, is not built yet when the cycle below traces the Outer.
sgcl::unique_ptr<StaticInitOuter> make_static_init_outer() {
    auto outer = sgcl::make_tracked<StaticInitOuter>();
    outer->inner = sgcl::make_tracked<StaticInitInner>();
    sgcl::collector::force_collect(true);   // a cycle while the initializer runs
    return outer;
}

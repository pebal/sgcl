//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "sgcl/sgcl.h"

#include <gtest/gtest.h>

// The first object of a type created in a global's initializer, before
// main, through a factory in another translation unit (static_init.cpp,
// static_init_factory.cpp). The metadata of Outer is built then and binds
// the type's pointer map (page_info.h: child_pointers); an inline static
// data member of a class template is initialized in no particular order
// with the globals of other translation units, so a cycle run inside the
// initializer found the map still empty and swept the Inner that only the
// map reaches (reported in pull request #13; reproduced with the factory
// linked after the global). A function-local static is built on first use.
extern int static_init_inner_destroyed;

struct StaticInitInner {
    int value = 42;
    ~StaticInitInner() {
        ++static_init_inner_destroyed;
        value = -1;
    }
};

struct StaticInitOuter {
    sgcl::tracked_ptr<StaticInitInner> inner;
};

// static_init_factory.cpp: where the types' metadata is first instantiated
sgcl::unique_ptr<StaticInitOuter> make_static_init_outer();

//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "sgcl/sgcl.h"

#include <gtest/gtest.h>

using namespace sgcl;

// Types used to reproduce the "first construction of a tracked type happens
// during static/dynamic initialization of a global object, before main()"
// scenario. StaticInitOuter is itself constructed for the first time inside
// another translation unit's global initializer, and its own constructor, in
// turn, triggers the first-ever construction of the nested tracked type
// StaticInitInner via a tracked_ptr field. This mirrors the real-world crash:
// a module-level global variable's initializer calling a factory function
// defined in a different translation unit, whose constructor nested-
// constructs yet another tracked type, all before main() runs.
struct StaticInitInner {
    int value = 42;
};

struct StaticInitOuter {
    int value;
    tracked_ptr<StaticInitInner> ptr;

    StaticInitOuter() {
        value = 7;
        ptr = make_tracked<StaticInitInner>();
    }
};

// Defined in a separate translation unit (static_init_factory.cpp) so that
// the first instantiation/use of PageInfo<StaticInitOuter> and
// PageInfo<StaticInitInner> happens in that other TU, while the call into it
// happens from this TU's own global variable initializer -- reproducing the
// cross translation-unit static-initialization-order dependency that the
// original bug exposed.
tracked_ptr<StaticInitOuter> make_static_init_outer();

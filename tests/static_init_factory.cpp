//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "static_init.h"

// Defined in its own translation unit deliberately: this is where
// PageInfo<StaticInitOuter>/PageInfo<StaticInitInner> get instantiated, kept
// separate from the translation unit that calls this function from a global
// variable's own dynamic initializer (see static_init.cpp).
tracked_ptr<StaticInitOuter> make_static_init_outer() {
    return make_tracked<StaticInitOuter>();
}

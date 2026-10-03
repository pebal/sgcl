//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "collector.h"

namespace sgcl::detail {
    // The deleter of unique_ptr (unique_ptr.h): the object's destructor runs
    // now, on the owner's thread, and the slot is left to the sweep in
    // state Destroyed (collector.h: delete_unique).
    struct UniqueDeleter {
        template<class T>
        void operator()(T* p) noexcept {
            Collector::delete_unique(p);
        }
    };
}

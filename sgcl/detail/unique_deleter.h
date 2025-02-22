//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "collector.h"

namespace sgcl::detail {
    struct UniqueDeleter {
        template<class T>
        void operator()(T* p) {
            Collector::delete_unique(p);
        }
    };
}

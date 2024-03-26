//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

namespace sgcl {
    namespace Priv {
        inline void Delete_unique(const void* p);

        struct Unique_deleter {
            template<class T>
            void operator()(T* p) {
                Delete_unique(p);
            }
        };
    }
}

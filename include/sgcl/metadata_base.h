//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/types.h"
#include "types.h"

namespace sgcl {
    struct metadata_base {
        template<class T>
        inline static void set(metadata* mdata) {
            Priv::Type_info<T>::set_user_metadata(mdata);
        }
    };
}

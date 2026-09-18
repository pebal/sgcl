//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

namespace sgcl::detail {
    // Whether a container's lookups take a key of another type than the
    // key type (a string_view for a string): the hash and the equality,
    // or the comparison, declare is_transparent, as in std. The
    // associative containers (hash_table.h, rb_tree.h) and the concurrent
    // ones (split_list.h, skip_list.h) enable their K overloads on these.
    template<class Hash, class Equal>
    concept TransparentLookup = requires {
        typename Hash::is_transparent;
        typename Equal::is_transparent;
    };

    template<class Compare>
    concept TransparentCompare = requires { typename Compare::is_transparent; };
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <type_traits>

namespace sgcl::detail {
    class Tracked {
    };

    // A tracked pointer word: a tracked_ptr (derived from Tracked), or a
    // type that says so by specializing this, one whose only state is a
    // tracked_ptr at offset 0 (the Ptr of the Sgcl interface). What
    // the containers zero rather than construct and move as words, and
    // what variant and any keep in the pointer word apart from data.
    template<class T>
    inline constexpr bool IsTrackedPointer = std::is_base_of_v<Tracked, T>;
}

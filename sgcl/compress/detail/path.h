//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string_view>

namespace sgcl::compress::detail {
    // Whether a name of an archive may be joined to a directory without
    // leaving it, as Go's filepath.IsLocal has it: not empty, not
    // absolute, no NUL, no backslash (a separator on Windows, and not one
    // in zip or tar), and no ".." that climbs above the start once the
    // path is taken lexically ("a/../b" is local, "a/../.." is not)
    inline bool is_local_path(std::string_view p) noexcept {
        if (p.empty() || p[0] == '/' || p.find('\\') != std::string_view::npos || p.find('\0') != std::string_view::npos) {
            return false;
        }
        int64_t depth = 0;
        while (!p.empty()) {
            auto slash = p.find('/');
            std::string_view part = p.substr(0, slash);
            p.remove_prefix(slash == std::string_view::npos ? p.size() : slash + 1);
            if (part.empty() || part == ".") {
                continue;
            }
            if (part == "..") {
                if (depth == 0) {
                    return false;
                }
                --depth;
            } else {
                ++depth;
            }
        }
        return true;
    }
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sgcl::io::detail {
    // Whether a name (of an archive, of a request) may be joined to a directory without
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

    // Where a symbolic link's target lands, the link's name's directory
    // (the name up to its last '/') joined to it: inside the directory
    // extracted to, by the rule of is_local_path on the joined name; an
    // absolute target and an empty one never are
    inline bool link_stays_inside(std::string_view name, std::string_view target) noexcept {
        if (target.empty() || target[0] == '/') {
            return false;
        }
        const auto slash = name.rfind('/');
        std::string joined(slash == std::string_view::npos ? std::string_view() : name.substr(0, slash + 1));
        joined.append(target);
        return is_local_path(joined);
    }
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The decisions extraction takes on names from an archive, on any bytes:
// is_local_path (whether a name may be joined to the directory: io::path::
// is_local and under, the rule of zip, tar and 7z) and link_stays_inside (whether a symbolic link's
// target, joined to the link's directory, stays in it). Pure functions, no
// disk.
//
// The model resolves the path apart: std::filesystem's lexical
// normalization (the "." and ".." taken, repeated separators joined, as
// Go's filepath.Clean) of a relative path, which is inside exactly when it
// does not begin with ".."; a path that is empty, absolute, or has a NUL or a
// backslash is outside by the module's rule (a backslash is a separator on
// Windows and never one in these formats). Each function must take its
// input exactly when the model's path is inside. A difference aborts.
//
// The input's first byte picks the function (even: is_local_path of the
// rest; odd: link_stays_inside, the name before the first 0xFF byte, the
// target after it).
//
//   tests/fuzz/run.sh tests/io/fuzz/local_path_fuzz.cpp 300
//
// Seeds: seeds/local_path/, the cases of Go's filepath.IsLocal tests and of
// archive/tar's and archive/zip's ErrInsecurePath, as names and as links.
#include "sgcl/io/detail/path.h"
#include "tests/fuzz/input.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace {
    void check(bool ok, const char* what, std::string_view a, std::string_view b = {}) {
        if (!ok) {
            std::fprintf(stderr, "local_path_fuzz: %s: \"%.*s\" \"%.*s\"\n", what, int(a.size()), a.data(), int(b.size()), b.data());
            std::abort();
        }
    }

    // The model: the path resolved lexically, inside or not
    bool inside(std::string_view p) {
        if (p.empty() || p.find('\0') != std::string_view::npos || p.find('\\') != std::string_view::npos || p[0] == '/') {
            return false;
        }
        const std::filesystem::path normal = std::filesystem::path(std::string(p)).lexically_normal();
        if (normal.is_absolute() || normal.has_root_directory()) {
            return false;
        }
        const auto first = normal.begin();
        return first == normal.end() || first->string() != "..";
    }

    // Where a link's target lands: the name's directory (up to its last
    // '/') joined to it; an empty or absolute target is outside
    bool link_inside(std::string_view name, std::string_view target) {
        if (target.empty() || target[0] == '/') {
            return false;
        }
        const auto slash = name.rfind('/');
        std::string joined(slash == std::string_view::npos ? std::string_view() : name.substr(0, slash + 1));
        joined.append(target);
        return inside(joined);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    if ((data[0] & 1) == 0) {
        check(sgcl::io::detail::is_local_path(rest) == inside(rest), "is_local_path disagrees with the resolved path", rest);
    } else {
        const auto cut = rest.find('\xFF');
        // the name in a buffer of its own size: the input's piece ends at
        // the 0xFF before the target, where a read past it goes unseen
        // (tests/fuzz/input.h); the target ends with the input
        const sgcl_fuzz::exact name_bytes(rest.substr(0, cut));
        const std::string_view name = name_bytes.view();
        const std::string_view target = cut == std::string_view::npos ? std::string_view() : rest.substr(cut + 1);
        check(sgcl::io::detail::link_stays_inside(name, target) == link_inside(name, target), "link_stays_inside disagrees with the resolved path",
              name, target);
    }
    return 0;
}

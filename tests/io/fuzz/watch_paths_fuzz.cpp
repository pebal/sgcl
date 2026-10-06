//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::watch's mapping of the system's paths to the program's on any bytes:
// the input split at NUL into the real root, a path the system reported, the
// path the program gave and a flag byte (recursive, a file watched, the ops),
// and what must hold of detail::watch_path_of:
//   - a path is given back under the program's path or not at all;
//   - a watched file: its own real path is the given path, every other none;
//   - a directory: an entry directly under the root comes back as the given
//     path joined with its name; one deeper only when recursive; the root
//     itself only for its removal or rename; a path outside the root (a
//     sibling that shares the root's prefix, "/a/bc" beside "/a/b") never.
// The events themselves come from the system (tests/io/watch.cpp, with real
// files); this is the part the library computes. Built with libFuzzer
// (tests/fuzz/run.sh tests/io/fuzz/watch_paths_fuzz.cpp) or replayed by the
// library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/io/watch.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    namespace io = sgcl::io;
    namespace d = sgcl::io::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    const uint8_t flags = data[0];
    std::string_view in(reinterpret_cast<const char*>(data + 1), size - 1);
    std::string part[3];
    size_t at = 0;
    for (int k = 0; k < 3; ++k) {
        size_t end = k < 2 ? in.find('\0', at) : std::string_view::npos;
        if (end == std::string_view::npos) {
            part[k] = std::string(in.substr(std::min(at, in.size())));
            at = in.size();
        } else {
            part[k] = std::string(in.substr(at, end - at));
            at = end + 1;
        }
    }
    d::WatchSource s;
    s.root = part[0];
    s.recursive = flags & 1;
    const bool file = flags & 2;
    const uint8_t ops = uint8_t((flags >> 2) & 0x1F);
    if (file) {
        s.target = part[0] + "/f";
    }
    const std::string& real = part[1];
    const std::string& given = part[2];
    std::string out = d::watch_path_of(s, real, given, ops);
    if (out.empty()) {
        return 0;
    }
    check(out.compare(0, given.size(), given) == 0);
    if (file) {
        check(real == s.target && out == given);
        return 0;
    }
    if (real == s.root) {
        check(out == given && (ops & uint8_t(io::watch_op::removed | io::watch_op::renamed)));
        return 0;
    }
    check(real.size() > s.root.size() + 1 && real.compare(0, s.root.size(), s.root) == 0 && real[s.root.size()] == '/');
    std::string_view below = std::string_view(real).substr(s.root.size() + 1);
    check(s.recursive || below.find('/') == std::string_view::npos);
    std::string want = given;
    if (want.empty() || want.back() != '/') {
        want += '/';
    }
    want += below;
    check(out == want);
    return 0;
}

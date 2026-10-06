//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::glob_pattern on any bytes: the input split at NUL into two patterns
// and a path, nothing touching the file system. What must hold:
//   - a pattern compiles or is refused (parse and the constructor, which
//     throws parse's error, agree),
//     without a crash, whatever its braces and classes;
//   - without `**`, braces or a hidden name in the path, match() is
//     path::match (Go's filepath.Match, element by element), which the
//     components of a glob are made of;
//   - braces are the union of their alternatives: {p1,p2} matches a path
//     when p1 or p2 does (p1 and p2 without braces, commas, classes or
//     escapes of their own);
//   - `**/` in front lets one more component in front of the path:
//     p matching q makes **/p match x/q;
//   - a pattern without wildcards or braces matches itself, written as a
//     path the way it was written.
// Python's glob is the oracle of tests/io/glob.cpp; this is the matcher's
// consistency on any bytes. Built with libFuzzer (tests/fuzz/run.sh
// tests/io/fuzz/glob_fuzz.cpp) or replayed by the library's own driver
// (tests/fuzz/driver.cpp).
#include "sgcl/io/glob.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace io = sgcl::io;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool has_any(std::string_view s, std::string_view chars) {
        return s.find_first_of(chars) != std::string_view::npos;
    }

    // A path a Go pattern and a glob read alike: no empty component, no
    // trailing "/", no component that begins with "."
    bool plain_path(std::string_view q) {
        if (q.empty() || q.back() == '/' || q.find("//") != std::string_view::npos) {
            return false;
        }
        for (size_t at = 0; at < q.size();) {
            if (q[at] == '/') {
                ++at;
                continue;
            }
            if (q[at] == '.') {
                return false;
            }
            size_t end = q.find('/', at);
            at = end == std::string_view::npos ? q.size() : end;
        }
        return true;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 2048) {
        return 0;
    }
    std::string_view in(reinterpret_cast<const char*>(data), size);
    std::string_view part[3];
    size_t at = 0;
    for (int k = 0; k < 3; ++k) {
        size_t end = k < 2 ? in.find('\0', at) : std::string_view::npos;
        if (end == std::string_view::npos) {
            part[k] = in.substr(std::min(at, in.size()));
            at = in.size();
        } else {
            part[k] = in.substr(at, end - at);
            at = end + 1;
        }
    }
    string p1(part[0]), p2(part[1]), q(part[2]);
    auto g1 = io::glob_pattern::parse(p1);
    bool threw = false;
    try {
        io::glob_pattern again(p1);
        check(g1.has_value());
        check(again.text() == p1);
    } catch (const bad_expected_access<io::error>&) {
        threw = true;
    }
    check(threw == !g1.has_value());
    if (!g1) {
        check(g1.error().code() == io::errc::invalid_pattern);
        return 0;
    }
    const bool m1 = g1->match(q);
    // Go's matcher, where the two read the pattern alike
    // (both rooted or neither: Go's '*' matches the empty element before a
    // leading '/', "*/x" matching "/x", where a glob's pattern and path are
    // rooted or not)
    if (!has_any(p1.view(), "{}") && p1.view().find("**") == std::string_view::npos && plain_path(q.view())
        && !p1.empty() && p1.view().back() != '/' && p1.view().find("//") == std::string_view::npos
        && (p1.view()[0] == '/') == (q.view()[0] == '/')) {
        auto go = io::path::match(p1, q);
        if (go) {
            check(*go == m1);
        }
    }
    // a literal pattern matches itself
    if (g1->is_literal() && !has_any(p1.view(), "\\") && !p1.empty() && p1.view().find("//") == std::string_view::npos
        && !(p1.view().size() > 1 && p1.view().back() == '/')) {
        check(g1->match(p1));
    }
    // **/ in front: one more component may come first
    if (m1 && !p1.empty() && p1.view()[0] != '/' && !q.empty() && q.view()[0] != '/') {
        auto g = io::glob_pattern::parse(string("**/") + p1);
        check(g.has_value());
        check(g->match(string("x/") + q));
    }
    // braces: the union of the alternatives
    if (!has_any(p1.view(), "{},[\\") && !has_any(p2.view(), "{},[\\")) {
        auto g2 = io::glob_pattern::parse(p2);
        auto both = io::glob_pattern::parse(string("{") + p1 + "," + p2 + "}");
        if (g2 && both) {
            check(both->match(q) == (m1 || g2->match(q)));
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::path on any bytes, without an oracle: the lexical functions (clean,
// join, base, dir, ext, stem, split, split_list, rel, match), the parts
// split at NUL bytes. Nothing touches the file system (abs and glob stay
// out). What must hold:
//   - clean is idempotent, its result never empty, without "//", a "."
//     element or a trailing separator (but for "/"), and ".." only at its
//     front; join of the parts is clean of their text joined;
//   - dir(p) + base(p) name p again: join(dir(p), base(p)) is clean(p)
//     for a p without a trailing separator (Go's Dir and Base drop it);
//     split's two halves are p; stem + ext is base (but for "." and "/",
//     and a p with a trailing separator, which ext reads and base drops);
//   - rel(a, b), where it answers, joined to a is clean(b);
//   - split_list gives the non-empty elements between the separators;
//   - match answers or refuses a pattern without a crash, and a name
//     matches itself when its bytes are no pattern's characters.
// Built with libFuzzer (tests/fuzz/run.sh tests/io/fuzz/path_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/io/path.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    namespace path = sgcl::io::path;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void cleaned(const string& p) {
        string c = path::clean(p);
        check(!c.empty());
        check(path::clean(c) == c);
        auto v = c.view();
        check(v.find("//") == std::string_view::npos);
        check(v == "/" || v.back() != '/');
        // elements: no ".", and ".." only before every other
        bool other = false;
        size_t at = v.front() == '/' ? 1 : 0;
        while (at < v.size()) {
            size_t end = v.find('/', at);
            if (end == std::string_view::npos) {
                end = v.size();
            }
            auto e = v.substr(at, end - at);
            check(e != "." || v == ".");
            if (e == "..") {
                check(!other && v.front() != '/');
            } else {
                other = true;
            }
            at = end + 1;
        }
    }

    void parts_of(const string& p) {
        string c = path::clean(p);
        string d = path::dir(p);
        string b = path::base(p);
        if (!p.empty() && p.view().back() != '/' && b.view() != "/" && b.view() != ".") {
            check(path::join(d, b) == c);
        }
        auto [head, tail] = path::split(p);
        check(std::string(head.view()) + std::string(tail.view()) == std::string(p.view()));
        string e = path::ext(p);
        string s = path::stem(p);
        // ext reads the path as written and base drops a trailing
        // separator (Go's Ext and Base): the two meet where there is none
        if (!p.empty() && p.view().back() != '/' && b.view() != "." && b.view() != "/" && b.view() != "..") {
            check(std::string(s.view()) + std::string(e.view()) == std::string(b.view()));
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 4096) {
        return 0;
    }
    std::string_view in(reinterpret_cast<const char*>(data), size);
    vector<string> parts;   // the library's: a string holds a tracked pointer
    size_t at = 0;
    for (;;) {
        size_t nul = in.find('\0', at);
        if (nul == std::string_view::npos || parts.size() == 7) {
            break;
        }
        parts.push_back(string(in.substr(at, nul - at)));
        at = nul + 1;
    }
    parts.push_back(string(in.substr(at)));

    for (auto& p : parts) {
        cleaned(p);
        parts_of(p);
    }
    // join: the text of the non-empty parts joined, cleaned
    std::string joined;
    for (auto& p : parts) {
        if (p.empty()) {
            continue;
        }
        if (!joined.empty()) {
            joined += '/';
        }
        joined += p.view();
    }
    string j = path::join(parts);
    check(joined.empty() ? j.empty() : j == path::clean(string(joined)));
    if (parts.size() >= 2) {
        auto r = path::rel(parts[0], parts[1]);
        if (r) {
            check(path::join(parts[0], *r) == path::clean(parts[1]));
        }
        auto m = path::match(parts[0], parts[1]);
        (void)m;
        bool plain = true;
        for (char c : parts[1].view()) {
            plain = plain && c != '*' && c != '?' && c != '[' && c != '\\' && c != '/';
        }
        if (plain && !parts[1].empty()) {
            auto self = path::match(parts[1], parts[1]);
            check(self.has_value() && *self);
        }
    }
    // split_list: the elements between the separators, none empty
    auto list = path::split_list(string(in));
    size_t total = 0;
    for (auto& e : list) {
        check(!e.empty() && e.view().find(':') == std::string_view::npos);
        total += e.size();
    }
    check(total <= in.size());
    return 0;
}

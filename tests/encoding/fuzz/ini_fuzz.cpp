//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::ini on any text. The first byte picks the options (duplicates, a
// key without a value); the rest is the text. What must hold: sections read
// write a text that reads back to the same sections (an empty section "" is
// not written) and writes the same text again, the writer refusing nothing a
// reading made but a key of a byte order mark first at the text's start; the
// lookups of every entry agree with the entries; a text refused is
// refused at a place within it. Built with libFuzzer (tests/fuzz/run.sh
// tests/encoding/fuzz/ini_fuzz.cpp) or replayed by the library's own driver
// (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    ini::options o;
    o.allow_duplicates = data[0] & 1;
    o.allow_no_value = data[0] & 2;
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    auto v = ini::parse(string(text), o);
    if (!v) {
        check(v.error().offset() <= text.size());
        return 0;
    }
    for (const auto& s : v->sections()) {
        check(v->contains(s.name));
        for (const auto& m : s.members) {
            check(v->get(s.name, m.key) == m.value);
            (void)v->get_int(s.name, m.key);
            (void)v->get_bool(s.name, m.key);
        }
    }
    string written;
    try {
        written = v->to_string();
    } catch (const invalid_argument&) {
        // the one refusal of what a reading made: a key of a byte order mark
        // first, written at the start of the text
        auto first = v->sections()[0].members[0].key.view();
        check(first.substr(0, 3) == "\xEF\xBB\xBF");
        return 0;
    }
    auto back = ini::parse(written);
    if (!back || !(*back == *v)) {
        std::fprintf(stderr, "written:\n%s\n%s\n", written.data(), back ? "differs" : back.error().message().data());
    }
    ini want = *v;
    if (want.contains("") && want.sections()[0].members.empty()) {
        want = want.erase("");
    }
    check(back && *back == want);
    check(back->to_string() == written);
    return 0;
}

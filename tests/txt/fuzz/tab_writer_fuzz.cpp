//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The tab writer on any text with any options. The first three bytes are the
// options and the size of the pieces; the rest is the text, copied into a
// buffer of exactly its size (never a managed copy, where ASan does not see a
// read past the end). What must hold:
//   - the text written in pieces gives what it gives written whole;
//   - but for the padding (and the debug bars, the stripped escapes), the
//     output is the text: its cells in order, a form feed made a line feed.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/tab_writer_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/txt/txt.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    std::string without(std::string_view s, std::string_view drop) {
        std::string out;
        for (char c : s) {
            if (drop.find(c) == std::string_view::npos) {
                out.push_back(c == '\f' ? '\n' : c);
            }
        }
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3 || size > 16384) {
        return 0;
    }
    uint8_t a = data[0], b = data[1], piece = data[2];
    txt::tab_options o;
    o.min_width = a & 7;
    o.tab_width = (a >> 3) & 7;
    o.padding = (a >> 6) & 3;
    static const char pads[] = {' ', ' ', '\t', '.'};
    o.pad_char = pads[b & 3];
    o.align_right = b & 4;
    o.discard_empty_columns = b & 8;
    o.tab_indent = b & 16;
    o.filter_html = b & 32;
    o.strip_escape = b & 64;
    o.debug = b & 128;
    size_t n = size - 3;
    char* text = static_cast<char*>(std::malloc(n ? n : 1));
    std::copy(data + 3, data + size, text);
    std::string_view t(text, n);

    std::string whole;
    {
        txt::detail::TabCore core(o);
        core.write(t, whole);
        core.flush(whole);
    }
    std::string pieces;
    {
        txt::detail::TabCore core(o);
        size_t step = size_t(piece % 17) + 1;
        for (size_t at = 0; at < n; at += step) {
            core.write(t.substr(at, step), pieces);
        }
        core.flush(pieces);
    }
    check(whole == pieces);
    if (!o.debug && !(o.strip_escape && o.filter_html)) {
        // (an escape's own tabs and soft tabs are written as they are; an
        // escape byte inside a tag is the tag's, which the check leaves out)
        std::string pad = {' ', '\t', '\v', o.pad_char};
        std::string in_drop = pad;
        if (o.strip_escape) {
            in_drop.push_back('\xFF');
        }
        check(without(whole, pad) == without(t, in_drop));
    }
    std::free(text);
    return 0;
}

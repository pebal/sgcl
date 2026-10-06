//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The library's side of tools/tab_vectors.py: the questions of
// tools/tab_oracle.go answered by txt::align_tabs, and by a tab_writer fed
// the same pieces (the line says MISMATCH when the two differ). Go's flags
// as tools/tab_oracle.go takes them: FilterHTML 1, StripEscape 2,
// AlignRight 4, DiscardEmptyColumns 8, TabIndent 16, Debug 32.
//
//   clang++ -std=c++20 -O1 -I. tools/tab_driver.cpp -o tab_driver
#include "sgcl/txt/tab_writer.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace sgcl;

namespace {
    std::string unhex(const std::string& h) {
        std::string out;
        for (size_t i = 0; i + 1 < h.size(); i += 2) {
            out.push_back(char(std::stoi(h.substr(i, 2), nullptr, 16)));
        }
        return out;
    }

    std::string hex(std::string_view s) {
        static const char* d = "0123456789abcdef";
        std::string out;
        for (unsigned char c : s) {
            out.push_back(d[c >> 4]);
            out.push_back(d[c & 15]);
        }
        return out;
    }
}

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        size_t min_width, tab_width, padding;
        int pad, flags;
        std::string text_hex, pieces;
        in >> min_width >> tab_width >> padding >> pad >> flags >> text_hex >> pieces;
        txt::tab_options o{.min_width = min_width, .tab_width = tab_width, .padding = padding, .pad_char = char(pad),
                           .align_right = (flags & 4) != 0, .discard_empty_columns = (flags & 8) != 0,
                           .tab_indent = (flags & 16) != 0, .filter_html = (flags & 1) != 0,
                           .strip_escape = (flags & 2) != 0, .debug = (flags & 32) != 0};
        std::string text = unhex(text_hex);
        string whole = txt::align_tabs(string(std::string_view(text)), o);
        txt::tab_writer w(o);
        std::string streamed;
        size_t at = 0;
        if (pieces != "-") {
            std::istringstream ps(pieces);
            std::string k;
            while (std::getline(ps, k, ',')) {
                size_t n = std::stoul(k);
                streamed += std::string(w.write(string(std::string_view(text).substr(at, n))).view());
                at += n;
            }
        }
        streamed += std::string(w.write(string(std::string_view(text).substr(at))).view());
        streamed += std::string(w.flush().view());
        if (streamed != std::string(whole.view())) {
            std::printf("MISMATCH\n");
        } else {
            std::printf("%s\n", hex(whole.view()).c_str());
        }
    }
}

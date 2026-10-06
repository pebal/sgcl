//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The tab writer over a table of 10,000 lines of four cells (a listing of
// files: a name, a size, permissions, a date):
//   tab_writer <whole|lines>
//   whole: align_tabs of the whole table
//   lines: a tab_writer fed the lines one by one, then flushed
// Prints milliseconds per table; the reference is Go's text/tabwriter over
// the same table (benchmarks/go/tab_writer).
#include "benchmarks/common.h"
#include "sgcl/txt.h"

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    using namespace sgcl;
    const char* variant = argc > 1 ? argv[1] : "whole";
    if (!bench::has_variant(variant, {"whole", "lines"})) {
        std::fprintf(stderr, "usage: tab_writer <whole|lines>\n");
        return 2;
    }
    vector<string> lines;
    std::string whole;
    for (int i = 0; i < 10000; ++i) {
        char day[8];
        std::snprintf(day, sizeof day, "%02d", 1 + i % 28);
        std::string perm;
        for (int k = 0; k <= i % 3; ++k) {
            perm += "rw-";
        }
        std::string l = "file_" + std::to_string(i) + ".txt\t" + std::to_string(i * 37 % 100000) + "\t" + perm +
                        "\t2026-10-" + day + "\n";
        whole += l;
        lines.push_back(string(std::string_view(l)));
    }
    string text{std::string_view(whole)};
    size_t sink = 0;
    int rounds = 50;
    auto t0 = bench::Clock::now();
    for (int r = 0; r < rounds; ++r) {
        if (!std::strcmp(variant, "whole")) {
            sink += txt::align_tabs(text).size();
        } else {
            txt::tab_writer w;
            for (const auto& l : lines) {
                sink += w.write(l).size();
            }
            sink += w.flush().size();
        }
    }
    double s = bench::seconds_since(t0);
    std::printf("%s bytes=%zu ms=%.3f (%zu)\n", variant, whole.size(), s * 1000 / rounds, sink % 7);
    return 0;
}

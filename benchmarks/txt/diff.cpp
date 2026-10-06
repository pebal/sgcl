//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Diff, patch and merge over large texts:
//   diff <unified|patience|lines|patch|merge> [old new [theirs]]
//   diff write <dir>
//   unified:  unified_diff of old and new (Myers)
//   patience: unified_diff by patience diff
//   lines:    diff_lines, the edit script only
//   patch:    apply_patch of the unified diff of old and new to old
//   merge:    merge3 of old (the base), new (ours) and theirs
// Prints milliseconds per operation. Without files the texts are made here:
// 100,000 lines of source-like text, new with 1,000 scattered changes,
// theirs with 1,000 others; `write` saves them as old.txt, new.txt and
// theirs.txt, for the references: diff -u, git diff --no-index, patch and
// git merge-file over the same files (process time, the files being large
// enough to make the start of a process negligible).
#include "benchmarks/common.h"
#include "sgcl/txt.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    std::string read(const char* path) {
        std::ifstream f(path, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    std::vector<std::string> made_lines(std::mt19937& g, size_t n) {
        static const char* const words[] = {"int", "return", "value", "count", "if", "for", "size", "data", "node",
                                            "next", "result", "index", "{", "}", "(", ")", ";", "=", "+", "0", "1"};
        std::vector<std::string> out;
        for (size_t i = 0; i < n; ++i) {
            std::string l(size_t(g() % 4) * 4, ' ');
            for (int w = int(g() % 7); w > 0; --w) {
                l += words[g() % 21];
                l += ' ';
            }
            out.push_back(l + "\n");
        }
        return out;
    }

    std::string join(const std::vector<std::string>& ls) {
        std::string out;
        for (const auto& l : ls) {
            out += l;
        }
        return out;
    }

    // a copy with `changes` scattered edits (a replaced, inserted or removed line)
    std::vector<std::string> changed(std::mt19937& g, std::vector<std::string> ls, size_t changes) {
        for (size_t k = 0; k < changes; ++k) {
            size_t at = g() % ls.size();
            switch (g() % 3) {
                case 0:
                    ls[at] = "changed " + std::to_string(k) + "\n";
                    break;
                case 1:
                    ls.insert(ls.begin() + long(at), "inserted " + std::to_string(k) + "\n");
                    break;
                default:
                    ls.erase(ls.begin() + long(at));
            }
        }
        return ls;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "unified";
    if (!bench::has_variant(variant, {"unified", "patience", "lines", "patch", "merge", "write"})) {
        std::fprintf(stderr, "usage: diff <unified|patience|lines|patch|merge> [old new [theirs]] | diff write <dir>\n");
        return 2;
    }
    std::string a, b, c;
    if (argc > 3) {
        a = read(argv[2]);
        b = read(argv[3]);
        c = argc > 4 ? read(argv[4]) : a;
    } else {
        std::mt19937 g(2026);
        std::vector<std::string> base = made_lines(g, 100000);
        a = join(base);
        std::mt19937 g1(1), g2(2);
        b = join(changed(g1, base, 1000));
        c = join(changed(g2, base, 1000));
    }
    if (!std::strcmp(variant, "write")) {
        std::string dir = argc > 2 ? argv[2] : ".";
        std::ofstream(dir + "/old.txt", std::ios::binary) << a;
        std::ofstream(dir + "/new.txt", std::ios::binary) << b;
        std::ofstream(dir + "/theirs.txt", std::ios::binary) << c;
        return 0;
    }
    string sa{std::string_view(a)}, sb{std::string_view(b)}, sc{std::string_view(c)};
    string patch = txt::unified_diff(sa, sb);
    size_t sink = 0;
    int rounds = 20;
    auto t0 = bench::Clock::now();
    for (int r = 0; r < rounds; ++r) {
        if (!std::strcmp(variant, "unified")) {
            sink += txt::unified_diff(sa, sb).size();
        } else if (!std::strcmp(variant, "patience")) {
            sink += txt::unified_diff(sa, sb, {.algorithm = txt::diff_algorithm::patience}).size();
        } else if (!std::strcmp(variant, "lines")) {
            sink += txt::diff_lines(sa, sb).size();
        } else if (!std::strcmp(variant, "patch")) {
            sink += txt::apply_patch(sa, patch)->size();
        } else {
            sink += txt::merge3(sa, sb, sc).text.size();
        }
    }
    double s = bench::seconds_since(t0);
    std::printf("%s bytes=%zu ms=%.2f (%zu)\n", variant, a.size(), s * 1000 / rounds, sink % 7);
    return 0;
}

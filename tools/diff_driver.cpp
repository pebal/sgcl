// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// txt's diff, patch and merge asked by tools/diff_vectors.py, one question a
// line: a mode and its fields separated by tabs, \\ \t \n escaped; the answer
// one line, escaped the same way.
//   E a b alg    the line edits: kind,old_begin,old_end,new_begin,new_end;...
//   W a b        the word edits      C a b   the code point edits
//   U a b alg    unified_diff
//   P text patch fuzz reverse   apply_patch, or ERROR <hunk>
//   M base ours theirs          merge3 with diff3 markers: <conflicts>|<text>
//   N base ours theirs          merge3 with the default markers
//
//	clang++ -std=c++20 -O1 -I. tools/diff_driver.cpp -o /tmp/diff_driver
#include "sgcl/txt/diff.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {
    std::string unescape(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char c = s[++i];
                out.push_back(c == 't' ? '\t' : c == 'n' ? '\n' : c);
            } else {
                out.push_back(s[i]);
            }
        }
        return out;
    }

    void print(const std::string& s) {
        for (char c : s) {
            if (c == '\\') std::fputs("\\\\", stdout);
            else if (c == '\t') std::fputs("\\t", stdout);
            else if (c == '\n') std::fputs("\\n", stdout);
            else std::putchar(c);
        }
        std::putchar('\n');
    }

    sgcl::string s(const std::string& t) {
        return sgcl::string(std::string_view(t));
    }
}

int main() {
    using namespace sgcl;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::vector<std::string> f;
        size_t b = 0;
        while (true) {
            size_t e = line.find('\t', b);
            f.push_back(unescape(line.substr(b, e == std::string::npos ? std::string::npos : e - b)));
            if (e == std::string::npos) break;
            b = e + 1;
        }
        while (f.size() < 5) f.emplace_back();
        const std::string& mode = f[0];
        if (mode == "E" || mode == "W" || mode == "C") {
            txt::diff_options o{f[3] == "patience" ? txt::diff_algorithm::patience : txt::diff_algorithm::myers};
            auto edits = mode == "E" ? txt::diff_lines(s(f[1]), s(f[2]), o) : mode == "W" ? txt::diff_words(s(f[1]), s(f[2]), o)
                                                                                           : txt::diff_chars(s(f[1]), s(f[2]), o);
            std::string out;
            for (const auto& e : edits) {
                out += std::to_string(int(e.kind)) + "," + std::to_string(e.old_begin) + "," + std::to_string(e.old_end) + "," +
                       std::to_string(e.new_begin) + "," + std::to_string(e.new_end) + ";";
            }
            print(out);
        } else if (mode == "U") {
            txt::unified_options o;
            o.algorithm = f[3] == "patience" ? txt::diff_algorithm::patience : txt::diff_algorithm::myers;
            print(std::string(txt::unified_diff(s(f[1]), s(f[2]), o).view()));
        } else if (mode == "P") {
            txt::patch_options o{size_t(std::stoul(f[3].empty() ? "2" : f[3])), f[4] == "1"};
            auto r = txt::apply_patch(s(f[1]), s(f[2]), o);
            print(r ? std::string(r->view()) : "ERROR " + std::to_string(r.error().hunk()));
        } else if (mode == "M" || mode == "N") {
            txt::merge_options o;
            o.diff3 = mode == "M";
            auto r = txt::merge3(s(f[1]), s(f[2]), s(f[3]), o);
            print(std::to_string(r.conflicts) + "|" + std::string(r.text.view()));
        }
        std::fflush(stdout);
    }
    return 0;
}

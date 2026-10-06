// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// Documents the writer of sgcl/encoding/ini.h writes of 300 random sets of
// sections, in the corpus form ("#%%% written N" before each) that
// tools/ini_oracle.py reads (its header says how the two are run): so that
// configparser reads what the writer writes as the sections written.
#include "sgcl/encoding/ini.h"

#include <cstdio>
#include <random>
#include <string>

using sgcl::encoding::ini;

int main() {
    static const char* names[] = {"server", "a b", " lead", "x.y", "UPPER", "a]b", "[x", "zażółć", "s;c", "#h", "k=v"};
    static const char* keys[] = {"host", "port", "my key", "k.e.y", "Key", "KEY", "x-y", "日本", "a]", "b[c", "q?"};
    static const char* values[] = {"", "1", "two words", "a=b", "x:y", "; not a comment", "# hash", "[x]", "http://h/?a=b",
                                   "line one\nline two", "\nafter empty", "zażółć", "a\nb\nc", "trailing;"};
    std::mt19937_64 r(20261006);
    for (int i = 0; i < 300; ++i) {
        ini v;
        int sections = int(r() % 4);
        for (int s = 0; s < sections; ++s) {
            sgcl::string name(r() % 5 == 0 ? "" : names[r() % (sizeof names / sizeof *names)]);
            int n = int(r() % 4) + (name.empty() ? 1 : 0);
            for (int k = 0; k < n; ++k) {
                v = v.set(name, sgcl::string(keys[r() % (sizeof keys / sizeof *keys)]), sgcl::string(values[r() % (sizeof values / sizeof *values)]));
            }
            if (n == 0) {
                v = v.set(name, "only", "1").erase(name, "only");
            }
        }
        std::printf("#%%%%%% written %d\n%s", i, std::string(v.to_string().view()).c_str());
    }
    std::printf("#%%%%%% end\n");
}

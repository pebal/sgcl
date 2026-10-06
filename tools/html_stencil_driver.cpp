// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// txt::html_stencil asked the questions of tools/html_stencil_oracle.go, the
// answers in the same form, for tools/html_stencil_vectors.py:
//
//	clang++ -std=c++20 -O1 -I. tools/html_stencil_driver.cpp -o /tmp/html_stencil_driver
#include "sgcl/txt.h"
#include "tests/txt/html_stencil_data.h"

#include <cstdio>
#include <iostream>
#include <string>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        auto f = html_stencil_data::split(line, '\t');
        while (f.size() < 2) {
            f.emplace_back();
        }
        std::string source = html_stencil_data::unescape(f[0]);
        auto t = sgcl::txt::html_stencil::parse(sgcl::string(std::string_view(source)));
        if (!t) {
            std::puts("ERROR");
            continue;
        }
        sgcl::string page = t->render(html_stencil_data::parse(f[1]));
        for (char c : page.view()) {
            if (c == '\\') std::fputs("\\\\", stdout);
            else if (c == '\t') std::fputs("\\t", stdout);
            else if (c == '\n') std::fputs("\\n", stdout);
            else std::putchar(c);
        }
        std::putchar('\n');
    }
    return 0;
}

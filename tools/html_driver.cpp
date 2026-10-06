// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The library's parser asked the questions of tools/html_oracle.c, the trees
// in the same form, for tools/html_vectors.py:
//
//	clang++ -std=c++20 -O1 -I. tools/html_driver.cpp -o /tmp/html_driver
#include "tests/txt/html_dump.h"

#include <cstdio>
#include <iostream>
#include <string>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string doc;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '\\' && i + 1 < line.size()) {
                char c = line[++i];
                doc.push_back(c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c == '0' ? '\0' : c);
            } else {
                doc.push_back(line[i]);
            }
        }
        std::string input = sgcl::txt::detail::html::preprocess(doc);
        sgcl::txt::detail::html::TreeBuilder b(input, false, false);   // scripting off, as gumbo parses
        auto tree = b.parse();
        std::fputs(html_dump::tree(tree).c_str(), stdout);
        std::puts("#end");
    }
    return 0;
}

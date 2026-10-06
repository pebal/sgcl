//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The library's side of tools/markdown_vectors.py: the questions of
// tools/markdown_oracle.c answered by txt::markdown_to_html, raw HTML kept
// and URLs unfiltered as md4c writes them (c: CommonMark alone, g: GitHub's
// extensions too). The tree of markdown_document writes the same HTML, or the
// answer says it does not.
//
//   clang++ -std=c++20 -O1 -I. tools/markdown_driver.cpp -o markdown_driver
#include "sgcl/txt/markdown.h"

#include <cstdio>
#include <iostream>
#include <string>

using namespace sgcl;

int main() {
    std::string line;
    static const char* digits = "0123456789abcdef";
    while (std::getline(std::cin, line)) {
        if (line.size() < 2) {
            continue;
        }
        bool github = line[0] == 'g';
        std::string text;
        for (size_t i = 2; i + 1 < line.size(); i += 2) {
            text.push_back(char(std::stoi(line.substr(i, 2), nullptr, 16)));
        }
        txt::markdown_options o{.tables = github, .strikethrough = github, .autolinks = github,
                                .task_lists = github, .raw_html = true, .tag_filter = false, .safe_urls = false};
        std::string html(txt::markdown_to_html(string(std::string_view(text)), o).view());
        // the tree's HTML is the same
        if (std::string(txt::markdown_document::parse(string(std::string_view(text)), o).to_html().view()) != html) {
            html = "MISMATCH of markdown_document::to_html";
        }
        std::string out;
        for (unsigned char c : html) {
            out.push_back(digits[c >> 4]);
            out.push_back(digits[c & 15]);
        }
        std::printf("%s\n", out.c_str());
    }
}

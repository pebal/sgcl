//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Markdown to HTML over a document, whole:
//   markdown <html|commonmark|tree> [file.md]
//   html:       markdown_to_html with GitHub's extensions (the defaults)
//   commonmark: markdown_to_html of CommonMark alone
//   tree:       markdown_document::parse, the managed tree
// Prints megabytes of Markdown per second. Without a file the document is
// made here (headings, lists, code, tables, links, emphasis); the reference
// is md4c, a C program over the same file, in a scratch directory: it is not
// in the tree.
#include "benchmarks/common.h"
#include "sgcl/txt.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {
    std::string made_document() {
        std::string d;
        for (int i = 0; i < 400; ++i) {
            std::string n = std::to_string(i);
            d += "## Section " + n + "\n\nSome *emphasis*, **strong** text, `code " + n + "` and a [link](/page/" + n +
                 " \"title\") with an ![image](/i/" + n + ".png). A line\nthat goes on, &amp; an entity.\n\n"
                 "- item one\n- item **two**\n  1. nested\n  2. list\n\n> A quote with _style_.\n\n"
                 "```cpp\nint main() { return " + n + "; }\n```\n\n| a | b |\n|:-|-:|\n| " + n + " | x |\n\n"
                 "Visit www.example.com or mail me@example.com ~~now~~.\n\n";
        }
        return d;
    }
}

int main(int argc, char** argv) {
    using namespace sgcl;
    const char* variant = argc > 1 ? argv[1] : "html";
    if (!bench::has_variant(variant, {"html", "commonmark", "tree"})) {
        std::fprintf(stderr, "usage: markdown <html|commonmark|tree> [file.md]\n");
        return 2;
    }
    std::string text;
    if (argc > 2) {
        std::ifstream f(argv[2], std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        text = ss.str();
    } else {
        text = made_document();
    }
    string doc{std::string_view(text)};
    txt::markdown_options plain{.tables = false, .strikethrough = false, .autolinks = false, .task_lists = false};
    size_t sink = 0;
    int rounds = int(200000000 / (text.size() + 1));
    if (rounds < 5) {
        rounds = 5;
    }
    auto t0 = bench::Clock::now();
    for (int r = 0; r < rounds; ++r) {
        if (!std::strcmp(variant, "html")) {
            sink += txt::markdown_to_html(doc).size();
        } else if (!std::strcmp(variant, "commonmark")) {
            sink += txt::markdown_to_html(doc, plain).size();
        } else {
            sink += txt::markdown_document::parse(doc).root().size();
        }
    }
    double s = bench::seconds_since(t0);
    std::printf("%s bytes=%zu MB/s=%.1f (%zu)\n", variant, text.size(), double(text.size()) * rounds / s / 1e6, sink % 7);
    return 0;
}

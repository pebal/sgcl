//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The HTML parser over a page, whole:
//   html <parse|serialize|sanitize> [page.html]
//   parse:     html_document::parse of the page
//   serialize: to_string of the parsed page
//   sanitize:  sanitize_html of the page
// Prints megabytes of the page per second. Without a file the page is made
// here (articles, links, a table, entities, a script); the reference is
// gumbo-parser, a C program over the same file, in a scratch directory: it
// is not in the tree.
#include "benchmarks/common.h"
#include "sgcl/txt.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {
    using namespace sgcl;

    std::string made_page() {
        std::string p = "<!DOCTYPE html><html><head><title>Page</title><meta charset=utf-8>"
                        "<style>body{margin:0}</style></head><body><nav><ul>";
        for (int i = 0; i < 50; ++i) {
            p += "<li><a href=\"/section/" + std::to_string(i) + "\" class=nav>Section " + std::to_string(i) + "</a>";
        }
        p += "</ul></nav>";
        for (int i = 0; i < 300; ++i) {
            p += "<article id=a" + std::to_string(i) + "><h2>Title &amp; more " + std::to_string(i) +
                 "</h2><p>Lorem ipsum <b>dolor</b> sit amet, <i>consectetur</i> adipiscing elit &mdash; sed do "
                 "<a href='https://example.com/?q=" + std::to_string(i) + "&amp;x=1'>eiusmod</a> tempor.<p>Second "
                 "paragraph with <code>x &lt; y</code> and <img src=/i.png alt=\"image\"> here.</article>";
        }
        p += "<table><thead><tr><th>a<th>b<th>c<tbody>";
        for (int i = 0; i < 200; ++i) {
            p += "<tr><td>" + std::to_string(i) + "<td>value<td><span title=x>cell</span>";
        }
        p += "</table><script>var x = 1 < 2 && 3 > 1; document.write('<b>');</script></body></html>";
        return p;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "parse";
    if (!bench::has_variant(variant, {"parse", "serialize", "sanitize"})) {
        std::fprintf(stderr, "usage: html <parse|serialize|sanitize> [page.html]\n");
        return 2;
    }
    std::string text;
    if (argc > 2) {
        std::ifstream f(argv[2], std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        text = ss.str();
    } else {
        text = made_page();
    }
    string page{std::string_view(text)};
    size_t sink = 0;
    int rounds = int(200000000 / (text.size() + 1));
    if (rounds < 5) {
        rounds = 5;
    }
    txt::html_document doc = txt::html_document::parse(page);
    auto t0 = bench::Clock::now();
    for (int r = 0; r < rounds; ++r) {
        if (!std::strcmp(variant, "parse")) {
            sink += txt::html_document::parse(page).root().size();
        } else if (!std::strcmp(variant, "serialize")) {
            sink += doc.to_string().size();
        } else {
            sink += txt::sanitize_html(page).size();
        }
    }
    double s = bench::seconds_since(t0);
    std::printf("%s bytes=%zu MB/s=%.1f (%zu)\n", variant, text.size(), double(text.size()) * rounds / s / 1e6, sink % 7);
    return 0;
}

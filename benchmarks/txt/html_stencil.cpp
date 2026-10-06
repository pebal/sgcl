//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// html_stencil: a template read once and written many times over a stream of
// different values with characters to escape; the shapes of
// benchmarks/go/html_stencil/main.go, which runs Go's html/template:
//   html_stencil [op=link] [count]
// ops: link (a link, its title and text), rows (a table of ten rows, an
// attribute, a URL and text in each), script (an object and a string in a
// script), parse (the rows template read, its page written once, as Go's
// first execution escapes). Prints nanoseconds per operation.
#include "benchmarks/common.h"
#include "sgcl/txt.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    using namespace sgcl;

    const char* source_for(const char* op) {
        if (!std::strcmp(op, "link")) {
            return R"(<a href="/u/{{ .id }}" title="{{ .name }}">{{ .name }}</a>)";
        }
        if (!std::strcmp(op, "script")) {
            return R"(<script>var cfg = {{ .cfg }}; var s = "{{ .name }}";</script>)";
        }
        return R"(<table>{{ range .rows }}<tr><td title="{{ .t }}">{{ .t }}</td><td><a href="{{ .u }}">x</a></td></tr>{{ end }}</table>)";
    }

    string word(int i) {
        return string("Ada & <Bob> \"" + std::to_string(i) + "\" o'neil");
    }
}

int main(int argc, char** argv) {
    const char* op = argc > 1 ? argv[1] : "link";
    if (!bench::has_variant(op, {"link", "rows", "script", "parse"})) {
        std::fprintf(stderr, "usage: html_stencil <link|rows|script|parse> [count]\n");
        return 2;
    }
    long count = argc > 2 ? std::atol(argv[2]) : 400000;
    constexpr int Values = 1024;
    vector<txt::value> data;
    for (int i = 0; i < Values; ++i) {
        txt::list rows;
        vector<txt::value> items;
        for (int r = 0; r < 10; ++r) {
            items.push_back(txt::object{{"t", word(i + r)}, {"u", string("/p?q=") + word(i + r)}});
        }
        data.push_back(txt::object{{"id", i}, {"name", word(i)}, {"cfg", txt::object{{"id", i}, {"name", word(i)}}},
                                   {"rows", txt::list(items)}});
    }
    size_t sink = 0;
    double ns;
    if (!std::strcmp(op, "parse")) {
        string source(source_for("rows"));
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            auto t = txt::html_stencil::parse(source);
            sink += t->render(data[size_t(i % Values)]).size();
        }
        ns = bench::seconds_since(t0) * 1e9 / double(count);
    } else {
        txt::html_stencil t{string(source_for(op))};
        sink += t.render(data[0]).size();
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            sink += t.render(data[size_t(i % Values)]).size();
        }
        ns = bench::seconds_since(t0) * 1e9 / double(count);
    }
    std::printf("sgcl op=%s ns/op=%.1f (%zu)\n", op, ns, sink % 7);
    return 0;
}

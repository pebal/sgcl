//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What it costs to write a page from a shape read beforehand.
//   stencil <sgcl|twopass|parse> [op=simple] [count]
//
// The `sgcl` variant renders a template read once outside the loop,
// which is the way a template is used: read at startup, written per
// request. The `parse` variant reads the same source inside the loop
// instead and renders nothing, so the two columns of a row say what the
// reading cost and what the writing costs, and whether reading once was
// worth the trouble of a compiled form at all.
//
// The `twopass` variant is the road render() took before its room could
// grow: the page written once into a buffer on the stack to be measured
// and, when it did not fit, written over again into one sized from that.
// It is spelled out here with render_to, which still has exactly that
// contract, so that the two roads can be told apart in one binary on one
// machine rather than by building the library twice. The rows where they
// differ at all are page1k, page10k and page100k; `rows` fits the stack
// room and takes the same road either way.
//
// Which is also the one thing this variant cannot be asked. Both roads
// go through the same walk and pay the same for asking after each step
// whether it fitted, so a page that fits reads the same on both and
// says nothing about what the asking costs. That question is only
// answerable against a binary built from the commit before the room
// could grow, the two alternated in one window; asked here instead, it
// answered "no difference" while the short page was paying fourteen per
// cent. What a row of this table means is the second walk and nothing
// else.
//
//   empty     a page with no action in it, forty-five characters: what
//             a step costs when there is only one
//   simple    one value in a line of text
//   five      five values, which is more than a format pattern keeps
//             steps for
//   spec      one value with a specification after the colon,
//             {{ n:>8.2f }} — the road through format.h's writers
//   branch    an if with an else over a truth
//   rows      a walk over ten rows of two fields each, an if inside it:
//             the shape of an actual page
//   rows100   the same over a hundred rows, where the page no longer
//             fits the room render keeps on the stack
//   page1k    the same walk over as many rows as come to a page of about
//             a kilobyte, which is where the stack room is first left
//             behind
//   page10k   ten kilobytes of it
//   page100k  a hundred: the size of a page somebody actually serves,
//             and where walking the whole of it a second time is the
//             thing worth not doing
//   pipe      one value through two functions of the pipeline
//   deep      a path four names long
//   miss      a name the data does not carry, which walks the mapping
//             and finds nothing
//   to        rows, written into a buffer the caller lends: no string is
//             handed back and nothing is allocated
//
// Every op runs over a stream of different values and not one repeated,
// because half of what writing a number costs is that its length cannot
// be predicted, and a repeated value hides exactly that.
//
// What to read it against. There is no second implementation to compare
// with here: Go's text/template is the oracle the tests use and it is
// another language, and the standard has nothing of the kind. So the
// row that matters is `simple` against what txt::format costs for the
// same line — the template's overhead over writing the same text with
// the pattern known at compile time — and that number is printed beside
// it by the `format` benchmark. Prints nanoseconds per call.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"
#include "sgcl/txt/stencil.h"

#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {
    const long Values = 1024;

    long count_for(const char* op, long given) {
        if (given) {
            return given;
        }
        if (!std::strcmp(op, "page100k")) {
            return 5'000;
        }
        if (!std::strcmp(op, "page10k")) {
            return 50'000;
        }
        if (!std::strcmp(op, "page1k")) {
            return 300'000;
        }
        if (!std::strcmp(op, "rows100")) {
            return 20'000;
        }
        if (!std::strcmp(op, "rows") || !std::strcmp(op, "to")) {
            return 200'000;
        }
        if (!std::strcmp(op, "pipe")) {
            return 500'000;
        }
        return 2'000'000;
    }

    // How many rows an op walks. The three page sizes are counted from
    // what a row comes to — "<li>user123456: off</li>" is twenty-four
    // characters and the list's own tags are nine — so the names mean
    // what they say; what the page came to is printed beside the number
    // rather than trusted.
    long rows_for(const char* op) {
        if (!std::strcmp(op, "page100k")) {
            return 4200;
        }
        if (!std::strcmp(op, "page10k")) {
            return 420;
        }
        if (!std::strcmp(op, "page1k")) {
            // Forty-two rows come to 996 characters, which still fits
            // the kilobyte on the stack and would have measured nothing:
            // this is meant to be the smallest page that leaves it
            return 48;
        }
        if (!std::strcmp(op, "rows100")) {
            return 100;
        }
        return 10;
    }

    // How many pages of data to make beforehand. A thousand of them is
    // what keeps the length of a number from being guessed twice running
    // and is what the small ops use; a hundred thousand characters of
    // rows is another matter, and eight of those are still eight
    // different pages with a different number in every row of them. A
    // power of two, because the walk picks one with a mask.
    long values_for(const char* op) {
        if (!std::strcmp(op, "page100k")) {
            return 8;
        }
        if (!std::strcmp(op, "page10k")) {
            return 32;
        }
        if (!std::strcmp(op, "page1k")) {
            return 256;
        }
        return Values;
    }

    bool walks_rows(const char* op) {
        return !std::strcmp(op, "rows") || !std::strcmp(op, "rows100")
            || !std::strcmp(op, "to") || !std::strcmp(op, "page1k")
            || !std::strcmp(op, "page10k") || !std::strcmp(op, "page100k");
    }

    const char* source_for(const char* op) {
        if (!std::strcmp(op, "empty")) {
            return "nothing happens here and it takes forty-five ch";
        }
        if (!std::strcmp(op, "simple")) {
            return "{{ n }} left";
        }
        if (!std::strcmp(op, "five")) {
            return "{{ a }} {{ b }} {{ c }} {{ d }} {{ e }}";
        }
        if (!std::strcmp(op, "spec")) {
            return "{{ d:>8.2f }}";
        }
        if (!std::strcmp(op, "branch")) {
            return "{{ if on }}yes{{ else }}no{{ end }}";
        }
        if (!std::strcmp(op, "pipe")) {
            return "{{ name | upper | trim }}";
        }
        if (!std::strcmp(op, "deep")) {
            return "{{ a.b.c.d }}";
        }
        if (!std::strcmp(op, "miss")) {
            return "{{ nosuchname }}";
        }
        // rows, rows100, the three page sizes and to
        return "<ul>{{ range rows }}<li>{{ who }}: "
               "{{ if on }}on{{ else }}off{{ end }}</li>{{ end }}</ul>";
    }

    // The values a stream of pages is written from. A thousand of each,
    // so that no number's length is guessed twice running and the whole
    // stays in the first level of cache.
    struct Stream {
        // The library's vector and not the standard's: a value holds
        // tracked pointers, and those must live on the stack or inside a
        // managed object — the buffer of a std::vector is neither, and
        // what the collector cannot see it collects
        sgcl::vector<sgcl::txt::value> data;
        long count = 0;

        Stream(const char* op, long rows, long values)
        : count(values) {
            using namespace sgcl;
            std::mt19937_64 rng(1);
            for (long i = 0; i < values; ++i) {
                int n = int(rng() % 1000000);
                double d = double(rng() % 100000) + 0.5;
                if (walks_rows(op)) {
                    vector<txt::value> made;
                    for (long r = 0; r < rows; ++r) {
                        made.push_back(txt::object{
                            {"who", txt::format("user{}", n + int(r))},
                            {"on", ((n + r) & 1) != 0},
                        });
                    }
                    data.push_back(txt::object{{"rows", txt::list(made)}});
                } else if (!std::strcmp(op, "deep")) {
                    data.push_back(txt::object{{"a", txt::object{{"b", txt::object{
                        {"c", txt::object{{"d", n}}}}}}}});
                } else {
                    data.push_back(txt::object{
                        {"n", n},
                        {"d", d},
                        {"on", (n & 1) != 0},
                        {"name", "  ada lovelace  "},
                        {"a", n}, {"b", n + 1}, {"c", n + 2}, {"e", n + 3},
                    });
                }
            }
        }
    };

    // The first case in a process reads about twice high, so the road is
    // walked before it is timed
    // Never more rounds than are timed: twenty thousand pages of a
    // hundred kilobytes are six seconds of warming for one and a half of
    // measuring, and a page that big is warm long before then.
    template<class F>
    void warm(long count, F&& f) {
        long n = count < 20000 ? count : 20000;
        for (long i = 0; i < n; ++i) {
            f(i);
        }
    }

    template<class F>
    double timed(long count, F&& f) {
        warm(count, f);
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            f(i);
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }

    volatile size_t sink = 0;

    // What a page came to, printed beside the number so that page10k is
    // seen to be ten kilobytes and not taken on trust
    size_t page = 0;

    double run_render(const char* op, long count) {
        using namespace sgcl;
        Stream in(op, rows_for(op), values_for(op));
        auto t = txt::stencil::parse(string(source_for(op)));
        if (!t) {
            return -1;
        }
        long mask = in.count - 1;
        auto at = [&](long i) { return size_t(i) & mask; };
        page = t->render(in.data[0]).size();
        if (!std::strcmp(op, "to")) {
            char room[8192];
            auto buffer = slice<char>(room, room + sizeof room);
            return timed(count, [&](long i) {
                sink += t->render_to(buffer, in.data[at(i)]);
            });
        }
        return timed(count, [&](long i) {
            sink += t->render(in.data[at(i)]).size();
        });
    }

    // The road render() took before its room could grow, written out in
    // terms of render_to, which still keeps that contract: the page
    // measured into a buffer on the stack, and written over again into
    // one sized from it when it did not fit. The same string is handed
    // back at the end of both, so what is being compared is the walk and
    // the room and nothing else.
    double run_twopass(const char* op, long count) {
        using namespace sgcl;
        Stream in(op, rows_for(op), values_for(op));
        auto t = txt::stencil::parse(string(source_for(op)));
        if (!t) {
            return -1;
        }
        long mask = in.count - 1;
        auto at = [&](long i) { return size_t(i) & mask; };
        page = t->render(in.data[0]).size();
        return timed(count, [&](long i) {
            char room[1024];
            const auto& data = in.data[at(i)];
            size_t n = t->render_to(slice<char>(room, room + sizeof room), data);
            if (n <= sizeof room) {
                sink += string(room, n).size();
                return;
            }
            // A std::string and not the library's: this is the buffer
            // the old road sized, and it holds characters and no pointer
            // the collector has to find
            std::string wider(n, '\0');
            t->render_to(slice<char>(wider.data(), wider.data() + n), data);
            sink += string(wider.data(), n).size();
        });
    }

    // The same source read over and over and nothing rendered: what the
    // compiled form saves every time a page is written
    double run_parse(const char* op, long count) {
        using namespace sgcl;
        string source(source_for(op));
        return timed(count, [&](long) {
            auto t = txt::stencil::parse(source);
            sink += t ? t->steps() : 0;
        });
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "twopass", "parse"})) {
        std::fprintf(stderr, "usage: stencil <sgcl|twopass|parse> [empty|simple|five|spec|"
                             "branch|rows|rows100|page1k|page10k|page100k|pipe|deep|miss|to] "
                             "[count]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "simple";
    // An op nobody wrote would otherwise fall through every comparison
    // and print a nought, which in a table reads as a measurement
    if (!bench::has_variant(op, {"empty", "simple", "five", "spec", "branch", "rows",
                                 "rows100", "page1k", "page10k", "page100k", "pipe",
                                 "deep", "miss", "to"})) {
        std::fprintf(stderr, "stencil: no op called %s\n", op);
        return 2;
    }
    long count = count_for(op, argc > 3 ? std::atol(argv[3]) : 0);
    double ns = !std::strcmp(variant, "parse") ? run_parse(op, count)
              : !std::strcmp(variant, "twopass") ? run_twopass(op, count)
              : run_render(op, count);
    if (ns < 0) {
        std::printf("%s op=%s count=%ld ns/op=n/a\n", variant, op, count);
        return 0;
    }
    std::printf("%s op=%s count=%ld page=%zu ns/op=%.2f\n", variant, op, count, page, ns);
    return 0;
}

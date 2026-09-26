//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JSON and CSV of the encoding module against Go's (benchmarks/go/json,
// the same ops over the same texts).
//   json sgcl [op=parse] [corpus=twitter] [seconds=2]
//
//   parse     json::parse of the text, a tree made each time
//             (Go: json/v2 Unmarshal into any)
//   write     to_string of the tree parsed once (Go: json/v2 Marshal of the any)
//   pretty    to_string(json::pretty) (Go: Marshal with an indent of two)
//   tokens    json::reader::next to the end (Go: jsontext.Decoder.ReadToken)
//   skip      json::reader::skip: the text checked (Go: jsontext.Value.IsValid)
//   typed     json::parse<records> of `corpus` records (Go: Unmarshal into
//             a slice of structs); corpus is the count
//   stringify json::stringify of the same records (Go: Marshal)
//   csv       csv::reader::next over `corpus` rows (Go: csv.Reader, ReuseRecord off)
//   csvtyped  csv::reader::read<T> of the same rows (Go: csv.Reader and strconv per field)
//   csvwrite  csv::writer of the same rows (Go: csv.Writer)
//
// A corpus of nativejson-benchmark is read from
// ~/Programming/oracles/nativejson/<corpus>.json (twitter, citm_catalog,
// canada); "strings" is made here: 2000 long ASCII strings, an escape in each. Prints nanoseconds per call and megabytes of text per second.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
using encoding::json;

namespace {
    volatile size_t sink = 0;

    // the records of typed, stringify, csv and csvwrite: the same on the Go side
    struct record {
        int64_t id = 0;
        string name;
        string email;
        double score = 0;
        bool active = false;
        vector<string> tags;

        void describe(encoding::field_list& f) {
            f.add("id", id);
            f.add("name", name);
            f.add("email", email);
            f.add("score", score);
            f.add("active", active);
            f.add("tags", tags);
        }
    };

    struct row_record {
        int64_t id = 0;
        string name;
        string email;
        double score = 0;
        bool active = false;

        void describe(encoding::field_list& f) {
            f.add("id", id);
            f.add("name", name);
            f.add("email", email);
            f.add("score", score);
            f.add("active", active);
        }
    };

    vector<record> records(size_t n) {
        vector<record> out;
        for (size_t i = 0; i < n; ++i) {
            record r;
            r.id = int64_t(i * 7919);
            r.name = string("user " + std::to_string(i));
            r.email = string("user" + std::to_string(i) + "@example.com");
            r.score = double(i % 1000) / 8.0;
            r.active = i % 3 == 0;
            r.tags = {string("t" + std::to_string(i % 5)), string("group, " + std::to_string(i % 11))};
            out.push_back(r);
        }
        return out;
    }

    std::string corpus_text(const char* name) {
        if (!std::strcmp(name, "strings")) {
            // long ASCII strings with an escape now and then: where the
            // search of the plain part of a string is all the work
            std::string t = "[";
            for (int i = 0; i < 2000; ++i) {
                t += (i ? ",\"" : "\"") + std::string(size_t(200 + i % 800), char('a' + i % 26)) + "\\n" + std::string(100, 'q') + "\"";
            }
            return t + "]";
        }
        const char* home = std::getenv("HOME");
        std::string path = std::string(home ? home : "") + "/Programming/oracles/nativejson/" + name + ".json";
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "json: no corpus %s\n", path.c_str());
            std::exit(2);
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    // Calls of f for about `seconds`, after one call not timed (the first
    // case in a process reads high: the road is walked before it is timed)
    template<class F>
    double timed(double seconds, long& count, F&& f) {
        f();
        auto t0 = bench::Clock::now();
        count = 0;
        do {
            f();
            ++count;
        } while (bench::seconds_since(t0) < seconds);
        return bench::seconds_since(t0) / double(count) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl"})) {
        std::fprintf(stderr, "usage: json sgcl [parse|write|pretty|tokens|skip] [corpus] [seconds]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "parse";
    const char* corpus = argc > 3 ? argv[3] : "twitter";
    double seconds = argc > 4 ? std::atof(argv[4]) : 2.0;
    long count = 0;
    double ns = 0;
    size_t bytes = 0;
    if (!std::strcmp(op, "parse") || !std::strcmp(op, "write") || !std::strcmp(op, "pretty") || !std::strcmp(op, "tokens") || !std::strcmp(op, "skip")) {
        string text(corpus_text(corpus));
        bytes = text.size();
        if (!std::strcmp(op, "parse")) {
            ns = timed(seconds, count, [&] { sink += json::parse(text)->size(); });
        } else if (!std::strcmp(op, "write") || !std::strcmp(op, "pretty")) {
            json tree = json::parse(text).value();
            auto& style = !std::strcmp(op, "pretty") ? json::pretty : json::compact;
            ns = timed(seconds, count, [&] { sink += tree.to_string(style).size(); });
        } else if (!std::strcmp(op, "tokens")) {
            ns = timed(seconds, count, [&] {
                json::reader r(text);
                size_t n = 0;
                while (auto t = r.next()) {
                    n += t->text().size();
                }
                sink += n;
            });
        } else {
            ns = timed(seconds, count, [&] {
                json::reader r(text);
                sink += r.skip();
            });
        }
    } else if (!std::strcmp(op, "typed") || !std::strcmp(op, "stringify")) {
        size_t n = size_t(std::atol(corpus));
        auto rs = records(n ? n : 10000);
        string text = encoding::json::stringify(rs).value();
        bytes = text.size();
        if (!std::strcmp(op, "typed")) {
            ns = timed(seconds, count, [&] { sink += encoding::json::parse<vector<record>>(text)->size(); });
        } else {
            ns = timed(seconds, count, [&] { sink += encoding::json::stringify(rs)->size(); });
        }
    } else if (!std::strcmp(op, "csv") || !std::strcmp(op, "csvwrite") || !std::strcmp(op, "csvtyped")) {
        size_t n = size_t(std::atol(corpus));
        auto rs = records(n ? n : 10000);
        std::string t = "id,name,email,score,active\n";
        for (auto& r : rs) {
            t += std::to_string(r.id) + "," + std::string(r.name.view()) + "," + std::string(r.email.view()) + "," + std::string(encoding::json(r.score).to_string().view()) + "," + (r.active ? "true" : "false") + "\n";
        }
        string text(t);
        bytes = text.size();
        if (!std::strcmp(op, "csv")) {
            ns = timed(seconds, count, [&] {
                encoding::csv::reader r(text);
                size_t fields = 0;
                while (auto row = r.next()) {
                    fields += row->size();
                }
                sink += fields;
            });
        } else if (!std::strcmp(op, "csvtyped")) {
            ns = timed(seconds, count, [&] {
                encoding::csv::reader r(text);
                size_t k = 0;
                while (auto row = r.read<row_record>()) {
                    k += size_t(row->id);
                }
                sink += k;
            });
        } else {
            vector<vector<string>> rows;
            for (auto& r : rs) {
                rows.push_back({to_string(r.id), r.name, r.email, encoding::json(r.score).to_string(), string(r.active ? "true" : "false")});
            }
            ns = timed(seconds, count, [&] {
                tracked_ptr<io::buffer> out = make_tracked<io::buffer>();
                io::writer w = out;
                encoding::csv::writer cw(w);
                for (auto& r : rows) {
                    cw.write(r);
                }
                (void)cw.flush();
                sink += out->size();
            });
        }
    } else {
        std::fprintf(stderr, "json: no op called %s\n", op);
        return 2;
    }
    std::printf("%s op=%s corpus=%s count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, corpus, count, ns, double(bytes) / ns * 1e3);
    return 0;
}

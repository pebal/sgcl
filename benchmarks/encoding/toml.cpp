//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// TOML of the encoding module. Go's standard library has no TOML and no C or
// C++ library is on the machine: the reference is Python's tomllib (the
// standard library's reader, benchmarks/python/toml.py, the same document),
// and the JSON of the same data read by json::parse.
//   toml sgcl|json [op=parse] [corpus=twitter] [seconds=2]
//
//   parse   toml::parse of the text, the tree made each time (json: json::parse
//           of the corpus's JSON with its nulls left out, the same data)
//   write   to_string of the tree parsed once
//
// The document: a corpus of nativejson-benchmark
// (~/Programming/oracles/nativejson/<corpus>.json) read as JSON and written
// as TOML by toml.h's writer (nulls have no TOML and are left out). Prints
// nanoseconds per document and megabytes of it per second.
#include "benchmarks/common.h"
#include "sgcl/encoding/encoding.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
using encoding::json;
using encoding::toml;

namespace {
    volatile size_t sink = 0;

    toml corpus(const char* name) {
        std::string path = std::string(std::getenv("HOME")) + "/Programming/oracles/nativejson/" + name + ".json";
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "toml: no corpus %s\n", path.c_str());
            std::exit(1);
        }
        std::stringstream s;
        s << f.rdbuf();
        return toml::from_json(json::parse(string(s.str())).value());
    }

    template<class F>
    double timed(double seconds, long& count, F&& f) {
        for (int i = 0; i < 5; ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        count = 0;
        while (bench::seconds_since(t0) < seconds) {
            f();
            ++count;
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    const char* op = argc > 2 ? argv[2] : "parse";
    const char* name = argc > 3 ? argv[3] : "twitter";
    double seconds = argc > 4 ? std::atof(argv[4]) : 2.0;
    toml value = corpus(name);
    string text = value.to_string();
    if (argc > 5 && !std::strcmp(argv[5], "dump")) {
        std::fwrite(text.data(), 1, text.size(), stdout);   // the document, for benchmarks/python/toml.py
        return 0;
    }
    long count = 0;
    double ns = 0;
    size_t bytes = text.size();
    if (!std::strcmp(variant, "sgcl") && !std::strcmp(op, "parse")) {
        ns = timed(seconds, count, [&] { sink += toml::parse(text)->size(); });
    } else if (!std::strcmp(variant, "sgcl") && !std::strcmp(op, "write")) {
        ns = timed(seconds, count, [&] { sink += value.to_string().size(); });
    } else if (!std::strcmp(variant, "json") && !std::strcmp(op, "parse")) {
        string j = value.to_json().to_string();
        bytes = j.size();
        ns = timed(seconds, count, [&] { sink += json::parse(j)->size(); });
    } else {
        std::fprintf(stderr, "usage: toml sgcl|json [parse|write] [corpus] [seconds]\n");
        return 2;
    }
    std::printf("%s op=%s corpus=%s bytes=%zu count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, name, bytes, count, ns, double(bytes) / ns * 1e3);
}

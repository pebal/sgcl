//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// CBOR and MessagePack of the encoding module. Go's standard library has
// neither and no reference library is on the machine, so the numbers stand
// beside the module's own JSON over the same document (the bench_json case
// of the same corpus) and Go's json/v2 (benchmarks/go/json).
//   cbor sgcl [op=parse] [corpus=twitter] [seconds=2]
//
//   parse          cbor::parse of the CBOR of the corpus, a value made each time
//   write          to_bytes of the value parsed once (preferred serialization)
//   deterministic  to_bytes(cbor::deterministic): every map's keys sorted
//   diag           to_string, the diagnostic notation
//   mp_parse       msgpack::parse of the MessagePack of the corpus
//   mp_write       msgpack::encode of the value
//
// The document: a corpus of nativejson-benchmark
// (~/Programming/oracles/nativejson/<corpus>.json) read as JSON and turned
// into CBOR by cbor::from_json. Prints nanoseconds per call and megabytes of
// the encoding per second.
#include "benchmarks/common.h"
#include "sgcl/encoding/encoding.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
using encoding::cbor;

namespace {
    volatile size_t sink = 0;

    std::string corpus_text(const char* name) {
        std::string path = std::string(std::getenv("HOME")) + "/Programming/oracles/nativejson/" + name + ".json";
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "cbor: no corpus %s\n", path.c_str());
            std::exit(1);
        }
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

    template<class F>
    double timed(double seconds, long& count, F&& f) {
        for (int i = 0; i < 20; ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        count = 0;
        while (bench::seconds_since(t0) < seconds) {
            for (int i = 0; i < 10; ++i) {
                f();
            }
            count += 10;
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    const char* op = argc > 2 ? argv[2] : "parse";
    const char* corpus = argc > 3 ? argv[3] : "twitter";
    double seconds = argc > 4 ? std::atof(argv[4]) : 2.0;
    if (std::strcmp(variant, "sgcl") != 0) {
        std::fprintf(stderr, "usage: cbor sgcl [parse|write|deterministic|diag|mp_parse|mp_write] [corpus] [seconds]\n");
        return 2;
    }
    auto j = encoding::json::parse(string(corpus_text(corpus))).value();
    cbor value = cbor::from_json(j);
    vector<byte> bytes = value.to_bytes();
    size_t size = bytes.size();
    long count = 0;
    double ns;
    if (!std::strcmp(op, "parse")) {
        ns = timed(seconds, count, [&] { sink += cbor::parse(bytes)->size(); });
    } else if (!std::strcmp(op, "write")) {
        ns = timed(seconds, count, [&] { sink += value.to_bytes().size(); });
    } else if (!std::strcmp(op, "deterministic")) {
        ns = timed(seconds, count, [&] { sink += value.to_bytes(cbor::deterministic).size(); });
    } else if (!std::strcmp(op, "diag")) {
        ns = timed(seconds, count, [&] { sink += value.to_string().size(); });
    } else if (!std::strcmp(op, "mp_parse")) {
        auto mp = encoding::msgpack::encode(value);
        size = mp.size();
        ns = timed(seconds, count, [&] { sink += encoding::msgpack::parse(mp)->size(); });
    } else if (!std::strcmp(op, "mp_write")) {
        size = encoding::msgpack::encode(value).size();
        ns = timed(seconds, count, [&] { sink += encoding::msgpack::encode(value).size(); });
    } else {
        std::fprintf(stderr, "cbor: no op called %s\n", op);
        return 2;
    }
    std::printf("%s op=%s corpus=%s bytes=%zu count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, corpus, size, count, ns,
                double(size) / ns * 1e3);
}

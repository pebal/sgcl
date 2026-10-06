//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// YAML of the encoding module against libyaml (Homebrew's; Go's standard
// library has no YAML), when the build finds it (SGCL_BENCH_LIBYAML).
//   yaml sgcl|libyaml [op=parse] [corpus=twitter] [seconds=2]
//
//   parse   yaml::parse of the text, the tree made each time (libyaml:
//           yaml_parser_load, its document's nodes, then deleted)
//   write   to_string of the tree parsed once (libyaml: yaml_emitter_dump of
//           the document loaded once, to memory)
//   scan    libyaml alone: yaml_parser_parse's events and nothing kept, the
//           floor of a reading
//
// The document: a corpus of nativejson-benchmark
// (~/Programming/oracles/nativejson/<corpus>.json) read as JSON and written
// as YAML by yaml.h's writer, the same bytes on both sides. Prints
// nanoseconds per document and megabytes of it per second.
#include "benchmarks/common.h"
#include "sgcl/encoding/encoding.h"

#if defined(SGCL_BENCH_LIBYAML)
#include <yaml.h>
#endif

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
using encoding::yaml;

namespace {
    volatile size_t sink = 0;

    std::string corpus_yaml(const char* name) {
        std::string path = std::string(std::getenv("HOME")) + "/Programming/oracles/nativejson/" + name + ".json";
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "yaml: no corpus %s\n", path.c_str());
            std::exit(1);
        }
        std::stringstream s;
        s << f.rdbuf();
        auto j = encoding::json::parse(string(s.str())).value();
        return std::string(yaml::from_json(j).to_string().view());
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

#if defined(SGCL_BENCH_LIBYAML)
    int write_to(void* data, unsigned char* buffer, size_t size) {
        static_cast<std::string*>(data)->append(reinterpret_cast<char*>(buffer), size);
        return 1;
    }
#endif
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    const char* op = argc > 2 ? argv[2] : "parse";
    const char* corpus = argc > 3 ? argv[3] : "twitter";
    double seconds = argc > 4 ? std::atof(argv[4]) : 2.0;
    std::string text = corpus_yaml(corpus);
    string doc(text);
    long count = 0;
    double ns = 0;
    if (!std::strcmp(variant, "sgcl")) {
        if (!std::strcmp(op, "parse")) {
            ns = timed(seconds, count, [&] { sink += yaml::parse(doc)->size(); });
        } else if (!std::strcmp(op, "write")) {
            yaml v = yaml::parse(doc).value();
            ns = timed(seconds, count, [&] { sink += v.to_string().size(); });
        } else {
            std::fprintf(stderr, "yaml: no op called %s for sgcl\n", op);
            return 2;
        }
    }
#if defined(SGCL_BENCH_LIBYAML)
    else if (!std::strcmp(variant, "libyaml")) {
        auto load = [&](yaml_document_t& d) {
            yaml_parser_t p;
            yaml_parser_initialize(&p);
            yaml_parser_set_input_string(&p, reinterpret_cast<const unsigned char*>(text.data()), text.size());
            if (!yaml_parser_load(&p, &d)) {
                std::fprintf(stderr, "libyaml: %s\n", p.problem);
                std::exit(1);
            }
            yaml_parser_delete(&p);
        };
        if (!std::strcmp(op, "parse")) {
            ns = timed(seconds, count, [&] {
                yaml_document_t d;
                load(d);
                sink += size_t(d.nodes.top - d.nodes.start);
                yaml_document_delete(&d);
            });
        } else if (!std::strcmp(op, "write")) {
            ns = timed(seconds, count, [&] {
                yaml_document_t d;
                load(d);   // the emitter consumes the document: loaded each time, and the load timed apart below
                std::string out;
                yaml_emitter_t e;
                yaml_emitter_initialize(&e);
                yaml_emitter_set_output(&e, write_to, &out);
                yaml_emitter_open(&e);
                yaml_emitter_dump(&e, &d);
                yaml_emitter_close(&e);
                yaml_emitter_delete(&e);
                sink += out.size();
            });
            long c2 = 0;
            double load_ns = timed(seconds / 2, c2, [&] {
                yaml_document_t d;
                load(d);
                yaml_document_delete(&d);
            });
            ns -= load_ns;
        } else if (!std::strcmp(op, "scan")) {
            ns = timed(seconds, count, [&] {
                yaml_parser_t p;
                yaml_parser_initialize(&p);
                yaml_parser_set_input_string(&p, reinterpret_cast<const unsigned char*>(text.data()), text.size());
                for (;;) {
                    yaml_event_t ev;
                    if (!yaml_parser_parse(&p, &ev)) {
                        std::exit(1);
                    }
                    bool end = ev.type == YAML_STREAM_END_EVENT;
                    yaml_event_delete(&ev);
                    if (end) {
                        break;
                    }
                }
                yaml_parser_delete(&p);
                sink += 1;
            });
        } else {
            std::fprintf(stderr, "yaml: no op called %s for libyaml\n", op);
            return 2;
        }
    }
#endif
    else {
        std::fprintf(stderr, "usage: yaml sgcl|libyaml [parse|write|scan] [corpus] [seconds]\n");
        return 2;
    }
    std::printf("%s op=%s corpus=%s bytes=%zu count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, corpus, text.size(), count, ns,
                double(text.size()) / ns * 1e3);
}

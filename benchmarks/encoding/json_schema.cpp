//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JSON Schema of the encoding module. Go's and Python's standard libraries
// have no validator and none is on the machine (no Python jsonschema): the
// numbers stand alone.
//   json_schema sgcl [op=valid] [seconds=2]
//
//   compile    json_schema::compile of a schema of twitter.json (a $defs of the
//              user, $refs, patterns, formats)
//   valid      valid() of nativejson-benchmark's twitter.json against it
//   validate   validate() of the same: every keyword walked, no failure
//   annotated  valid() with unevaluatedProperties at every object, the
//              annotations of each kept
//
// Prints nanoseconds per operation and megabytes of the instance a second.
#include "benchmarks/common.h"
#include "sgcl/encoding/encoding.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
using encoding::json;
using encoding::json_schema;

namespace {
    volatile size_t sink = 0;

    const char* schema_text(bool annotated) {
        return annotated ? R"({
          "$id": "https://bench.invalid/twitter.json", "type": "object", "required": ["statuses"],
          "properties": {
            "statuses": {"type": "array", "items": {"$ref": "#/$defs/status"}},
            "search_metadata": {"type": "object"}
          },
          "unevaluatedProperties": false,
          "$defs": {
            "status": {"type": "object", "required": ["id", "text", "user"],
              "allOf": [{"properties": {"id": {"type": "integer", "minimum": 0}, "id_str": {"type": "string", "pattern": "^[0-9]+$"}}}],
              "properties": {"text": {"type": "string", "maxLength": 1000}, "user": {"$ref": "#/$defs/user"},
                             "retweet_count": {"type": "integer", "minimum": 0}, "lang": {"type": "string"},
                             "retweeted_status": {"$ref": "#/$defs/status"}},
              "unevaluatedProperties": true},
            "user": {"type": "object", "required": ["id", "screen_name"],
              "properties": {"id": {"type": "integer"}, "screen_name": {"type": "string", "minLength": 1}, "followers_count": {"type": "integer", "minimum": 0},
                             "profile_image_url": {"type": "string", "format": "uri"}},
              "unevaluatedProperties": true}
          }})"
                         : R"({
          "$id": "https://bench.invalid/twitter.json", "type": "object", "required": ["statuses"],
          "properties": {
            "statuses": {"type": "array", "items": {"$ref": "#/$defs/status"}},
            "search_metadata": {"type": "object"}
          },
          "$defs": {
            "status": {"type": "object", "required": ["id", "text", "user"],
              "properties": {"id": {"type": "integer", "minimum": 0}, "id_str": {"type": "string", "pattern": "^[0-9]+$"},
                             "text": {"type": "string", "maxLength": 1000}, "user": {"$ref": "#/$defs/user"},
                             "retweet_count": {"type": "integer", "minimum": 0}, "lang": {"type": "string"},
                             "retweeted_status": {"$ref": "#/$defs/status"}}},
            "user": {"type": "object", "required": ["id", "screen_name"],
              "properties": {"id": {"type": "integer"}, "screen_name": {"type": "string", "minLength": 1}, "followers_count": {"type": "integer", "minimum": 0},
                             "profile_image_url": {"type": "string", "format": "uri"}}}
          }})";
    }

    template<class F>
    double timed(double seconds, long& count, F&& f) {
        for (int i = 0; i < 3; ++i) {
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
    const char* op = argc > 2 ? argv[2] : "valid";
    double seconds = argc > 3 ? std::atof(argv[3]) : 2.0;
    if (std::strcmp(variant, "sgcl")) {
        std::fprintf(stderr, "usage: json_schema sgcl [compile|valid|validate|annotated] [seconds]\n");
        return 2;
    }
    std::string path = std::string(std::getenv("HOME")) + "/Programming/oracles/nativejson/twitter.json";
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "json_schema: no corpus %s\n", path.c_str());
        return 1;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    json instance = json::parse(string(text)).value();
    bool annotated = !std::strcmp(op, "annotated");
    json schema = json::parse(string(schema_text(annotated))).value();
    json_schema::options o;
    o.format_assertion = true;
    json_schema s = json_schema::compile(schema, o).value();
    if (!s.valid(instance)) {
        std::fprintf(stderr, "json_schema: the corpus does not validate\n");
        return 1;
    }
    long count = 0;
    double ns = 0;
    size_t bytes = text.size();
    if (!std::strcmp(op, "compile")) {
        bytes = 0;
        ns = timed(seconds, count, [&] { sink += bool(json_schema::compile(schema, o)); });
    } else if (!std::strcmp(op, "valid") || annotated) {
        ns = timed(seconds, count, [&] { sink += s.valid(instance); });
    } else if (!std::strcmp(op, "validate")) {
        ns = timed(seconds, count, [&] { sink += s.validate(instance).size(); });
    } else {
        std::fprintf(stderr, "json_schema: no op called %s\n", op);
        return 2;
    }
    if (bytes) {
        std::printf("%s op=%s bytes=%zu count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, bytes, count, ns, double(bytes) / ns * 1e3);
    } else {
        std::printf("%s op=%s count=%ld ns/op=%.0f\n", variant, op, count, ns);
    }
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::toml on any text. The first byte picks the depth limit; the rest
// is the text. What must hold: a document read writes a text that reads back
// to an equal value with the same hash and writes the same text again; its
// JSON is made without a fault; every date and time reads through sgcl::time;
// a text refused is refused at a place within it. The corpus is compared with
// Python's tomllib after a campaign (tools/toml_oracle.py has the form).
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/toml_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void touch(const toml& t, size_t depth) {
        if (depth > 600) {
            return;
        }
        (void)t.as_datetime();
        (void)t.as_date();
        (void)t.as_time();
        (void)t.as_datetime(time::zone::fixed(duration(std::chrono::hours(5))));
        (void)t.as_int();
        (void)t.as_double();
        for (auto& e : t.elements()) {
            touch(e, depth + 1);
        }
        for (auto& m : t.members()) {
            check(t.contains(m.key));
            touch(m.value, depth + 1);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    toml::options o;
    o.max_depth = (data[0] & 1) ? 8 : 512;
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    auto v = toml::parse(string(text), o);
    if (!v) {
        check(v.error().offset() <= text.size());
        return 0;
    }
    touch(*v, 0);
    auto written = v->to_string();
    auto back = toml::parse(written);
    if (!back || !(*back == *v)) {
        std::fprintf(stderr, "written:\n%s\n%s\n", written.data(), back ? "differs" : back.error().message().data());
    }
    check(back && *back == *v && back->hash() == v->hash());
    check(back->to_string() == written);
    (void)v->to_json();
    (void)toml::from_json(v->to_json()).to_string();
    return 0;
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::dotenv on any text. The first byte picks the options (expansion,
// the environment, the size of the values); the rest is the text. What must hold: a text read writes
// a text that reads back (expansion on: the writer escapes every '$') to the
// same entries and writes the same text again; the typed lookups of every
// entry read without a fault; a text refused is refused at a place within it.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/dotenv_fuzz.cpp)
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
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    dotenv::options o;
    o.expand = !(data[0] & 1);
    o.use_environment = data[0] & 2;
    o.max_size = (data[0] & 4) ? 4096 : size_t(1) << 20;   // doubling expansions stop soon
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    auto v = dotenv::parse(string(text), o);
    if (!v) {
        check(v.error().offset() <= text.size());
        return 0;
    }
    for (const auto& m : v->members()) {
        check(v->contains(m.key));
        (void)v->get_int(m.key);
        (void)v->get_double(m.key);
        (void)v->get_bool(m.key);
    }
    auto written = v->to_string();
    auto back = dotenv::parse(written);
    if (!back || !(*back == *v)) {
        std::fprintf(stderr, "written:\n%s\n%s\n", written.data(), back ? "differs" : back.error().message().data());
    }
    check(back && *back == *v);
    check(back->to_string() == written);
    return 0;
}

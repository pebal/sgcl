//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::json_schema on any schema and any instance. The input is the
// schema's text, a zero byte, the instance's text; the first byte picks the
// options. What must hold: a schema compiled answers valid exactly when
// validate gives nothing, every violation's locations are a JSON Pointer
// (empty or starting with '/') and an absolute URI with a fragment, and
// nothing faults; a schema refused is refused with an error. The depth is
// kept small: nested applicators multiply the work (a schema is the
// program's to trust). Built with libFuzzer (tests/fuzz/run.sh
// tests/encoding/fuzz/json_schema_fuzz.cpp) or replayed by the library's own
// driver (tests/fuzz/driver.cpp).
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
    if (size < 1 || size > 8192) {
        return 0;
    }
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    size_t zero = text.find('\0');
    std::string schema_text = zero == std::string::npos ? text : text.substr(0, zero);
    std::string instance_text = zero == std::string::npos ? std::string("null") : text.substr(zero + 1);
    auto schema = json::parse(string(schema_text));
    auto instance = json::parse(string(instance_text));
    if (!schema || !instance) {
        return 0;
    }
    json_schema::options o;
    o.format_assertion = data[0] & 1;
    o.max_depth = 12;
    auto s = json_schema::compile(*schema, o);
    if (!s) {
        check(!s.error().message().empty());
        return 0;
    }
    bool ok = s->valid(*instance);
    auto errors = s->validate(*instance);
    if (ok != errors.empty()) {
        std::fprintf(stderr, "valid %d, %zu violations\n", int(ok), errors.size());
    }
    check(ok == errors.empty());
    for (const auto& v : errors) {
        check(v.instance_location.empty() || v.instance_location.view()[0] == '/');
        check(v.keyword_location.empty() || v.keyword_location.view()[0] == '/');
        check(v.absolute_location.view().find('#') != std::string_view::npos);
    }
    return 0;
}

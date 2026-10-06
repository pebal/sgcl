//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// json::patch, merge_patch, diff, erase_path on any texts. The input is two
// JSON texts split at the first zero byte: a document and a patch. What must
// hold: a patch applied gives a value or an error at an operation of the
// patch, without a fault, the document unchanged; the diff of the document
// and the value it gave, applied to the document, gives that value again; a
// merge patch and an erase at every pointer the patch names run without a
// fault, and diff turns the document into the merged value too.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/json_patch_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstring>
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
    if (size > 65536) {
        return 0;
    }
    std::string in(reinterpret_cast<const char*>(data), size);
    size_t cut = in.find('\0');
    if (cut == std::string::npos) {
        return 0;
    }
    json::options o;
    o.max_depth = 64;
    auto doc = json::parse(string(in.substr(0, cut)), o);
    auto ops = json::parse(string(in.substr(cut + 1)), o);
    if (!doc || !ops) {
        return 0;
    }
    json before = *doc;
    auto r = doc->patch(*ops);
    check(*doc == before);
    if (r) {
        auto again = doc->patch(json::diff(*doc, *r));
        check(again && *again == *r);
    } else {
        check(!r.error().path().empty() || !ops->is_array());
    }
    json merged = doc->merge_patch(*ops);
    auto back = doc->patch(json::diff(*doc, merged));
    check(back && *back == merged);
    for (const auto& op : ops->elements()) {
        if (auto p = op["path"].as_string()) {
            (void)doc->erase_path(*p);
            (void)doc->at_path(*p);
            (void)doc->set_path(*p, op["value"]);
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::jsonrpc on any bytes; the first byte picks the framing (even:
// Content-Length, odd: lines) and the rest is a stream: each message cut
// out of it (on libFuzzer's own bytes, their end the input's) and handled by
// a table of methods; every answer is JSON of the specification's shape,
// and a response read back gives a result or an error object.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/jsonrpc_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/jsonrpc.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace rpc = sgcl::net::jsonrpc;
    namespace rd = sgcl::net::jsonrpc::detail;
    using encoding::json;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    rpc::methods table() {
        rpc::methods m;
        m.add("echo", [](const json& p) -> expected<json, rpc::error> { return p; });
        m.add("fail", [](const json& p) -> expected<json, rpc::error> { return unexpected(rpc::error(-32000, "fail", p)); });
        m.add_notification("note", [](const json&) {});
        return m;
    }

    // A response object: jsonrpc 2.0, an id, a result or an error of a code and a message
    void check_response(const json& r) {
        check(r.is_object() && r["jsonrpc"].as_string(string()) == "2.0" && r.contains(string("id")));
        check(r.contains(string("result")) != r.contains(string("error")));
        if (r.contains(string("error"))) {
            check(r["error"]["code"].is_integer() && r["error"]["message"].is_string());
        }
        (void)rd::rpc_outcome(r, string("x"));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    rpc::framing f = data[0] % 2 ? rpc::framing::line : rpc::framing::content_length;
    std::string_view bytes(reinterpret_cast<const char*>(data + 1), size - 1);
    rpc::methods m = table();
    for (int i = 0; i < 16; ++i) {
        size_t from = 0, length = 0, used = 0;
        const char* why = nullptr;
        int r = rd::rpc_frame(bytes, f, 1 << 20, from, length, used, why);
        if (r <= 0) {
            check(r == 0 || why != nullptr);
            break;
        }
        check(used <= bytes.size() && from + length <= used);
        std::string_view text = bytes.substr(from, length);
        bytes.remove_prefix(used);
        auto answer = m.handle(string(text));
        if (!answer) {
            continue;
        }
        auto j = json::parse(*answer);
        check(bool(j));
        if (j->is_array()) {
            check(!j->empty());
            for (auto& e : j->elements()) {
                check_response(e);
            }
        } else {
            check_response(*j);
        }
    }
    return 0;
}

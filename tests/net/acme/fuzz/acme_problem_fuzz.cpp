//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the client reads of an error answer, on any bytes: the problem
// document (RFC 7807, RFC 8555 §6.7) and the error made of it, Retry-After
// (seconds or an HTTP date) and the Link fields (RFC 8288) of the answer's
// head. The input is the document, a NUL, the Retry-After value, a NUL, and
// the Link fields one a line. What must hold: nothing crashes or hangs; a
// problem read has a type ("about:blank" when the document had none) and a
// status within HTTP's; the error of it is of the acme category with its
// detail and subproblems in the text; Retry-After is never negative and at
// most a year; every link found is a non-empty URL.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/acme/fuzz/acme_problem_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/acme/client.h"

#include <cstdint>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace d = sgcl::net::acme::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view all(reinterpret_cast<const char*>(data), size);
    size_t a = all.find('\0');
    std::string_view doc = all.substr(0, a);
    std::string_view rest = a == std::string_view::npos ? std::string_view() : all.substr(a + 1);
    size_t b = rest.find('\0');
    std::string_view retry = rest.substr(0, b);
    std::string_view links = b == std::string_view::npos ? std::string_view() : rest.substr(b + 1);
    if (auto j = encoding::json::parse(string(doc))) {
        auto p = d::parse_problem(*j);
        if (p) {
            check(!p->type.empty());
            check(p->status >= 0 && p->status <= 999);
            auto e = d::problem_error(*p, "acme fuzz", 3 * second);
            check(&e.code().category() == &net::acme::category());
            check(e.code().value() == int(p->code()));
            auto text = e.message();
            check(text.view().find("retry after 3 s") != std::string_view::npos);
        }
    }
    net::http::headers h;
    {
        h.set(string("Retry-After"), string(retry));
        auto r = d::retry_after_of(h);
        check(r >= duration::zero() && r <= duration(366 * 24 * hour));
        size_t at = 0;
        while (at <= links.size()) {
            size_t nl = links.find('\n', at);
            std::string_view line = links.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at);
            h.add(string("Link"), string(line));
            if (nl == std::string_view::npos) {
                break;
            }
            at = nl + 1;
        }
        for (const char* rel : {"up", "alternate", "index"}) {
            for (auto& u : d::links_of(h, rel, string("https://ca.test/acme/cert/1"))) {
                (void)u;
            }
        }
    }
    return 0;
}

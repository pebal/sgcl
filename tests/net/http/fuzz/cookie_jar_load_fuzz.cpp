//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::cookie_jar::load_json on any bytes, into a jar that holds a cookie
// already and has small limits (3 a domain, 8 in all). What must hold:
//   - a text refused leaves the jar as it was (the one cookie, sent as
//     before);
//   - a text taken leaves the jar within its limits, and the jar written
//     again (session cookies too) loads into a new jar as it is: the same
//     text written once more, the same cookies sent to the URLs of the
//     cookies' own domains.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/cookie_jar_load_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/cookie_jar.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    net::http::cookie_jar::options o;
    o.max_cookies = 8;
    o.max_cookies_per_domain = 3;
    net::http::cookie_jar jar(o);
    net::url home("http://example.com/");
    jar.set_cookies(home, {net::http::cookie("keep", "1")});
    std::string_view input(reinterpret_cast<const char*>(data), size);
    auto r = jar.load_json(string(input));
    if (!r) {
        check(jar.size() == 1 && jar.header(home) == "keep=1");
        return 0;
    }
    check(jar.size() <= o.max_cookies);
    auto text = jar.to_json(true);
    net::http::cookie_jar back(o);
    check(bool(back.load_json(text)));
    check(back.to_json(true) == text);
    for (auto& c : jar.all()) {
        auto d = c.domain.view();
        if (!d.empty() && d.front() == '.') {
            d.remove_prefix(1);
        }
        bool v6 = d.find(':') != std::string_view::npos;
        auto u = net::url::parse(string::concat("https://", v6 ? "[" : "", d, v6 ? "]" : "", c.path));
        if (u) {
            check(back.header(*u) == jar.header(*u));
        }
    }
    return 0;
}

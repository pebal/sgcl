//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::url on any bytes, without an oracle: the properties the WHATWG
// parser and serializer promise. The first byte picks the path, the rest is
// split at NUL bytes into the input and, where the path takes them, a base
// or a setter's value. What must hold:
//   - a URL parsed has an href of ASCII alone, without a tab or a line
//     break, and that href parsed again is the same URL, part by part
//     (parse(to_string(u)) == u);
//   - the same with a base, and resolve(reference) is parse(reference, base);
//   - a setter's result is a URL whose href parses to itself, and the
//     setter applied a second time with the same value changes nothing;
//   - query_params' serialization, parsed and written again, is the same.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/url_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/url.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void same(const net::url& a, const net::url& b) {
        check(a == b);
        check(a.to_string() == b.to_string());
        check(a.scheme() == b.scheme());
        check(a.username() == b.username());
        check(a.password() == b.password());
        check(a.host() == b.host());
        check(a.hostname() == b.hostname());
        check(a.port() == b.port());
        check(a.path() == b.path());
        check(a.query() == b.query());
        check(a.fragment() == b.fragment());
        check(a.has_query() == b.has_query());
        check(a.has_fragment() == b.has_fragment());
        check(a.has_opaque_path() == b.has_opaque_path());
        check(a.has_host() == b.has_host());
        check(a.origin() == b.origin());
    }

    // href: ASCII, no tab or line break; parsed again, the same URL
    void stable(const net::url& u) {
        string href = u.to_string();
        for (unsigned char c : href.view()) {
            check(c < 0x80 && c != '\t' && c != '\n' && c != '\r');
        }
        auto again = net::url::parse(href);
        check(again.has_value());
        same(u, *again);
        // the query's pairs: written, parsed and written again, the same
        string q = u.query_params().to_string();
        check(net::query_params::parse(q).to_string() == q);
    }

    template<class F>
    void setter(const net::url& u, F set) {
        auto once = set(u);
        if (!once) {
            return;
        }
        stable(*once);
        auto twice = set(*once);
        check(twice.has_value());
        same(*once, *twice);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    std::vector<std::string_view> parts;
    size_t at = 0;
    while (parts.size() < 2) {
        size_t nul = rest.find('\0', at);
        if (nul == std::string_view::npos) {
            break;
        }
        parts.push_back(rest.substr(at, nul - at));
        at = nul + 1;
    }
    parts.push_back(rest.substr(at));
    string input(parts[0]);
    string other(parts.size() > 1 ? parts[1] : std::string_view());

    auto u = net::url::parse(input);
    if (u) {
        stable(*u);
    }
    switch (mode % 4) {
        case 0: {
            // the input a reference against a base (the second part)
            auto base = net::url::parse(other);
            if (!base) {
                break;
            }
            auto r = net::url::parse(input, *base);
            auto r2 = base->resolve(input);
            check(r.has_value() == r2.has_value());
            if (r) {
                same(*r, *r2);
                stable(*r);
            }
            break;
        }
        case 1:
        case 2: {
            // a setter on the input's URL with the second part as the value
            if (!u) {
                break;
            }
            switch ((mode >> 2) % 10) {
                case 0: setter(*u, [&](const net::url& x) { return x.with_scheme(other); }); break;
                case 1: setter(*u, [&](const net::url& x) { return x.with_username(other); }); break;
                case 2: setter(*u, [&](const net::url& x) { return x.with_password(other); }); break;
                case 3: setter(*u, [&](const net::url& x) { return x.with_host(other); }); break;
                case 4: setter(*u, [&](const net::url& x) { return x.with_hostname(other); }); break;
                case 5: {
                    optional<uint16_t> port;
                    if (other.size() >= 2) {
                        port = uint16_t(uint8_t(other[0]) | uint8_t(other[1]) << 8);
                    }
                    setter(*u, [&](const net::url& x) { return x.with_port(port); });
                    break;
                }
                case 6: setter(*u, [&](const net::url& x) { return x.with_path(other); }); break;
                case 7: setter(*u, [&](const net::url& x) { return expected<net::url, io::error>(x.with_query(other)); }); break;
                case 8: setter(*u, [&](const net::url& x) { return expected<net::url, io::error>(x.with_fragment(other)); }); break;
                case 9: {
                    auto params = net::query_params::parse(other);
                    setter(*u, [&](const net::url& x) { return expected<net::url, io::error>(x.with_query(params)); });
                    setter(*u, [&](const net::url& x) { return expected<net::url, io::error>(x.without_fragment()); });
                    break;
                }
            }
            break;
        }
        case 3: {
            // the input as a query alone
            auto parsed = net::query_params::parse(input);
            string q = parsed.to_string();
            check(net::query_params::parse(q).to_string() == q);
            // first(text, name) is parse(text).get(name): for every name the
            // text holds, and for one it does not
            for (auto& p : parsed) {
                check(net::query_params::first(input, p.first) == parsed.get(p.first));
            }
            check(net::query_params::first(input, string("\x01no such name")) == parsed.get(string("\x01no such name")));
            break;
        }
    }
    return 0;
}

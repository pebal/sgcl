//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::cookie_jar fed Set-Cookie values and URLs: the input is lines, each
// an operation picked by its first byte — a Set-Cookie value (the rest of
// the line) from one of a table of URLs, a request to one of them, a URL of
// the input's own setting and asking, a removal — on a jar of small limits
// (4 a domain, 12 in all), so that eviction runs. What must hold after
// every request:
//   - the Cookie field is the cookies() of the URL, name=value joined by
//     "; ", a value with a space or a comma in quotes;
//   - each of them goes to that URL: its host is the cookie's domain (or
//     under it, for a domain cookie, never an address), its path matches,
//     a Secure one only over a secure origin, none expired; the longer
//     paths come first;
//   - no cookie's name and value pass 4096 bytes or hold a control, no
//     domain cookie is of a public suffix, and the limits hold.
// At the end the jar written as JSON (session cookies too) loads into a new
// jar, which then sends what the first sends.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/cookie_jar_fuzz.cpp)
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

    const char* const Urls[] = {
        "http://example.com/", "https://www.example.com/a/b", "http://a.b.example.com/a/", "https://example.co.uk/x",
        "http://www.example.co.uk/", "http://foo.github.io/p/q", "http://127.0.0.1:8080/", "https://[::1]/z",
        "http://localhost/", "ws://example.com/a", "http://xn--bcher-kva.de/", "http://co.uk/",
    };
    constexpr size_t UrlCount = sizeof(Urls) / sizeof(Urls[0]);

    void request(const net::http::cookie_jar& jar, const net::url& u) {
        auto list = jar.cookies(u);
        auto field = jar.header(u);
        std::string want;
        for (auto& c : list) {
            if (!want.empty()) {
                want += "; ";
            }
            want += c.name.view();
            want += '=';
            bool quote = c.value.view().find_first_of(" ,") != std::string_view::npos;
            want += quote ? "\"" : "";
            want += c.value.view();
            want += quote ? "\"" : "";
        }
        check(field.view() == want);
        auto t = net::http::detail::jar_target(u);
        if (!t) {
            check(list.empty());
            return;
        }
        size_t last_path = SIZE_MAX;
        for (auto& c : list) {
            auto d = c.domain.view();
            bool host_only = d.empty() || d.front() != '.';
            if (!host_only) {
                d.remove_prefix(1);
                check(!t->is_ip && net::http::detail::public_suffix_length(d) < d.size());
            }
            check(host_only ? d == t->host : net::http::detail::domain_match(t->host, d, false));
            check(net::http::detail::path_match(t->path.view(), c.path.view()));
            check(!c.secure || t->secure);
            check(!c.expires || c.expires->unix_nano() > time::detail::now_nanos() - 1000000000);
            check(c.name.size() + c.value.size() <= 4096 && !net::http::detail::jar_has_control(c.name.view())
                  && !net::http::detail::jar_has_control(c.value.view()));
            check(c.path.size() <= last_path);
            last_path = c.path.size();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    net::http::cookie_jar::options o;
    o.max_cookies = 12;
    o.max_cookies_per_domain = 4;
    net::http::cookie_jar jar(o);
    std::string_view rest(reinterpret_cast<const char*>(data), size);
    int ops = 0;
    while (!rest.empty() && ops++ < 64) {
        size_t nl = rest.find('\n');
        std::string_view line = rest.substr(0, nl);
        rest = nl == std::string_view::npos ? std::string_view() : rest.substr(nl + 1);
        if (line.empty()) {
            continue;
        }
        uint8_t op = uint8_t(line[0]);
        std::string_view arg = line.substr(1);
        net::url u(Urls[(op >> 3) % UrlCount]);
        switch (op & 7) {
            case 0:
            case 1:
            case 2:
                if (auto c = net::http::cookie::parse(string(arg))) {
                    jar.set_cookies(u, {*c});
                }
                break;
            case 3:
                request(jar, u);
                break;
            case 4:
                // a URL of the input's own
                if (auto own = net::url::parse(string(arg))) {
                    jar.set_cookies(*own, {net::http::cookie("own", "1")});
                    request(jar, *own);
                }
                break;
            case 5:
                if (auto c = net::http::cookie::parse(string(arg))) {
                    (void)jar.remove(c->domain, c->path, c->name);
                    (void)jar.remove(c->domain);
                }
                break;
            case 6:
                (void)jar.clear_expired();
                (void)jar.clear_session();
                break;
            default: {
                // a cookie whose fields are the input's, not parse's
                net::http::cookie c("n", string(arg));
                c.domain = string(arg.substr(0, arg.size() / 2));
                c.path = string(arg.substr(arg.size() / 2));
                c.secure = op & 0x40;
                jar.set_cookies(u, {c});
                break;
            }
        }
        check(jar.size() <= o.max_cookies);
    }
    for (size_t i = 0; i < UrlCount; ++i) {
        request(jar, net::url(Urls[i]));
    }
    auto text = jar.to_json(true);
    net::http::cookie_jar back(o);
    check(bool(back.load_json(text)));
    check(back.all().size() == jar.all().size());
    for (size_t i = 0; i < UrlCount; ++i) {
        net::url u(Urls[i]);
        check(back.header(u) == jar.header(u));
    }
    return 0;
}

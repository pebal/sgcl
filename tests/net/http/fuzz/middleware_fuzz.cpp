//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The middlewares on any request (cors, rate_limit, sessions in a sealed
// cookie and in memory, csrf with double-submit tokens, body_limit,
// recovery), through a
// response recorder: the input is a byte for the method and lines "name:
// value" of the request's fields (Origin, Sec-Fetch-Site, Cookie,
// X-CSRF-Token, Access-Control-Request-*, Content-Type, or any), then, after
// an empty line, the body. What must hold:
//   - never a crash, a status of 200, 204, 403, 404, 405, 413 or 500, and
//     the handler's 500 only for its own throw (a route that throws);
//   - a preflight (OPTIONS with Origin and Access-Control-Request-Method)
//     never reaches a handler;
//   - an unsafe request the origin check refuses (Sec-Fetch-Site cross-site
//     or same-site from an origin not trusted) never reaches a handler;
//   - a session a handler reads comes from a cookie the server sealed
//     (the handler sees "secret" only after the fuzzer replayed a cookie
//     of the login route, which this harness never hands it).
// And the cookie store's plaintext reader on the raw bytes: a session read
// back is packed again to the same bytes. Built with libFuzzer
// (tests/fuzz/run.sh tests/net/http/fuzz/middleware_fuzz.cpp) or the
// library's own driver.
#include "sgcl/net/http/http.h"
#include "sgcl/slog/memory.h"

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace http = sgcl::net::http;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
    // A copy of exactly the bytes in malloc memory, for a reader to parse:
    // ASan sees a read past its end, which in a managed copy (or a string's
    // spare capacity) it would not
    struct Exact {
        char* p;
        size_t n;

        explicit Exact(std::string_view v)
        : p(static_cast<char*>(std::malloc(v.size() ? v.size() : 1))), n(v.size()) {
            std::copy(v.begin(), v.end(), p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(p);
        }

        std::string_view view() const noexcept {
            return std::string_view(p, n);
        }
    };


    std::atomic<int> reached = {0};

    struct Servers {
        http::server cookie_store;
        http::server memory_store;

        Servers() {
            for (int k : {0, 1}) {
                http::server s;
                s.use(http::recovery({.log = slog::logger(slog::memory())}));
                s.use(http::cors({.origins = {string("https://a.example")}, .credentials = true}));
                s.use(http::rate_limit(1e9, 1000000, {.header = "X-Key", .max_keys = 64}));
                if (k == 0) {
                    s.use(http::sessions::in_cookie(crypto::random::secret(32)));
                } else {
                    s.use(http::sessions::in_memory({.idle_timeout = std::chrono::seconds(1)}));
                }
                s.use(http::csrf({.kind = http::csrf::tokens::double_submit, .trusted_origins = {string("https://t.example")}}));
                s.route("/me", [](http::request r, http::response_writer w) {
                    ++reached;
                    http::session ses(r);
                    check(ses.get("user") != "secret");   // only a login could have set it
                    w.write(ses.get("user") + http::csrf::token(r));
                });
                s.route("/set", [](http::request r, http::response_writer w) -> async::task<> {
                    ++reached;
                    auto t = co_await r.async_text();
                    http::session(r).set("user", t ? *t : string());
                    w.write("set");
                });
                s.route("POST /up", http::body_limit(16).wrap([](http::request r, http::response_writer w) -> async::task<> {
                    ++reached;
                    auto t = co_await r.async_bytes();
                    w.write(t ? "ok" : "no");
                }));
                s.route("/throw", [](http::request, http::response_writer) {
                    ++reached;
                    throw std::runtime_error("thrown");
                });
                (k == 0 ? cookie_store : memory_store) = s;
            }
        }
    };

    Servers& servers() {
        static root_ptr<Servers> s = make_tracked<Servers>();
        return *s;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    static const char* methods[] = {"GET", "POST", "OPTIONS", "PUT", "DELETE", "HEAD", "PATCH", "POST"};
    static const char* targets[] = {"/me", "/set", "/up", "/throw", "/none", "/me", "/set", "/up"};
    const char* method = methods[data[0] & 7];
    const char* target = targets[(data[0] >> 3) & 7];
    const bool memory = data[0] & 0x40;
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);

    // the plaintext reader: what it reads back packs to the same bytes
    {
        net::http::detail::SessionState st;
        int64_t touched = 0;
        Exact exact(rest);
        if (net::http::detail::unpack_session(exact.view(), st, touched)) {
            check(net::http::detail::pack_session(st, touched) == rest);
        }
    }

    std::string body;
    auto blank = rest.find("\n\n");
    std::string_view head = rest.substr(0, blank);
    if (blank != std::string_view::npos) {
        body.assign(rest.substr(blank + 2));
    }
    auto req = http::test_request(method, target, string(body));
    bool preflight_shape = false, origin = false, cross = false, trusted = false;
    std::string site;
    while (!head.empty()) {
        auto nl = head.find('\n');
        std::string_view line = head.substr(0, nl);
        head = nl == std::string_view::npos ? std::string_view() : head.substr(nl + 1);
        auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            continue;
        }
        std::string_view name = line.substr(0, colon);
        std::string_view value = line.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') {
            value.remove_prefix(1);
        }
        req.headers().add(string(name), string(value));
    }
    const http::headers& in = req.headers();
    auto first = [&](std::string_view n) { return net::http::detail::HeadersAccess::find(in, n); };
    origin = bool(first("origin"));
    preflight_shape = origin && std::string_view(method) == "OPTIONS" && first("access-control-request-method");
    if (auto s = first("sec-fetch-site")) {
        site = std::string(net::http::detail::trim_ows(*s));
    }
    if (auto o = first("origin")) {
        trusted = net::http::detail::trim_ows(*o) == "https://t.example";
    }
    cross = (site == "cross-site" || site == "same-site") && !trusted;

    const int before = reached.load();
    http::response_recorder rec;
    rec.serve(memory ? servers().memory_store : servers().cookie_store, req);
    const bool ran = reached.load() != before;
    const int st = rec.status();
    check(st == 200 || st == 204 || st == 403 || st == 404 || st == 405 || st == 413 || st == 429 || st == 500);
    if (preflight_shape && st != 429) {
        check(!ran);
    }
    const std::string_view m = method;
    const bool safe = m == "GET" || m == "HEAD" || m == "OPTIONS";
    if (!safe && cross) {
        check(!ran);
    }
    if (st == 500) {
        check(std::string_view(target) == "/throw" || !ran);
    }
    return 0;
}

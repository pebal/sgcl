//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's cache on any bytes (the readers parse the input's own bytes,
// or a malloc copy of exactly a piece: ASan sees past neither, as it would
// not past a managed copy): the input as a Cache-Control value (its
// directives read, the verdicts of every age taken), as a head file of a
// cache in a directory (read back, written again, read again the same),
// and as the server's answers (split at the first 0xFF: the first answer,
// then the answer to every later request) to a client with a cache asking
// the same URL three times and once with HEAD. What must hold:
//   - never a crash, never a hang;
//   - delta-seconds are -1 or within [0, 2^31]; a verdict's age is the one
//     given;
//   - a head that reads back writes the same bytes again;
//   - a response the cache served as a hit sent no request; an empty cache
//     holds no bytes.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/cache_fuzz.cpp)
// or the library's own driver.
#include "sgcl/net/http/http.h"

#include <atomic>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>

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

    struct Server {
        std::string first, rest;
        std::atomic<int> dials{0};
        std::atomic<int> ended{0};
    };

    async::task<> serve(net::connection c, Server* s, int n) {
        std::string got;
        byte buf[4096];
        while (got.find("\r\n\r\n") == std::string::npos) {
            auto r = co_await c.async_read(slice<byte>(buf, sizeof buf));
            if (!r || *r == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *r);
        }
        (void)co_await c.async_write(string(n == 1 ? s->first : s->rest));
        (void)co_await c.async_close();
        ++s->ended;
    }

    async::task<expected<net::connection, io::error>> dial(Server* s) {
        auto [a, b] = net::connection::in_memory();
        async::go(serve(b, s, ++s->dials));
        co_return a;
    }

    void check_delta(int64_t v) {
        check(v == -1 || (v >= 0 && v <= 2147483648));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view in(reinterpret_cast<const char*>(data), size);

    // the directives
    // the directives, read from the input's own bytes (exactly its size, not a managed copy)
    http::detail::CacheControl cc;
    http::detail::cache_control_value(in, cc);
    for (int64_t v : {cc.max_age, cc.s_maxage, cc.min_fresh, cc.stale_while_revalidate, cc.stale_if_error}) {
        check_delta(v);
    }
    check(cc.max_stale == -1 || cc.max_stale >= 0);
    for (int64_t age : {int64_t(0), int64_t(1), int64_t(59), int64_t(60), int64_t(86400)}) {
        check(http::detail::cache_verdict(cc, cc, 60, age).age == age);
    }

    // a head file
    if (auto e = http::detail::CacheState::entry_of(in)) {
        http::detail::CacheState st;
        const std::string again = st.head_of(*e);
        Exact exact(again);
        auto back = http::detail::CacheState::entry_of(exact.view());
        check(back && st.head_of(*back) == again);
    }

    // the server's answers
    Server s;
    const size_t cut = in.find('\xff');
    s.first.assign(in.substr(0, cut));
    s.rest.assign(cut == std::string_view::npos ? in : in.substr(cut + 1));
    {
        http::client c;
        Server* sp = &s;
        c.dial = [sp](const net::url&, async::stop_token) { return dial(sp); };
        c.max_redirects = 2;
        c.timeout = std::chrono::seconds(5);
        c.cache = http::cache();
        for (int k = 0; k < 4; ++k) {
            const int before = s.dials.load();
            auto r = c.send(http::request(k == 3 ? "HEAD" : "GET", "http://cache.test/a"));
            if (r) {
                (void)r->text();
                if (r->from_cache() == http::response::cache_status::hit) {
                    check(s.dials.load() == before);
                }
            }
        }
        if (c.cache->size() == 0) {
            check(c.cache->bytes() == 0);
        }
        c.close_idle_connections();   // an idle connection stays its idle_timeout (90 s) after its client is gone
    }
    // the connections' tasks (and a background revalidation's) ended before the server goes
    for (int i = 0; i < 5000; ++i) {
        if (s.ended.load() >= s.dials.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (s.ended.load() >= s.dials.load()) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
}

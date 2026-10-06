//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The OAuth 2.0 client on any answer of the server: the first byte picks
// the call (the code exchanged, client_credentials, a refresh, the device
// authorization, the device's polling, revocation, introspection, a token
// source's client asking a resource), the rest is what the server sends
// back to every request, bytes of HTTP/1.1 as they come (a head and a
// body, or anything else). The connections are in memory. What must hold:
//   - never a crash, never a hang (the polling stopped after 300 ms or a
//     few requests);
//   - a token read has an access token; an error read from a JSON object
//     has its code, an error of the transport none;
//   - a device authorization read has its device code, user code and
//     verification URI, and an interval of a second or more;
//   - the resource asked through the token source's client is asked at
//     most twice (a 401 invalid_token answered by one refresh).
// Built with libFuzzer (tests/fuzz/run.sh tests/net/oauth2/fuzz/oauth2_fuzz.cpp)
// or the library's own driver.
#include "sgcl/net/oauth2.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>

namespace {
    using namespace sgcl;
    namespace http = sgcl::net::http;
    namespace oauth2 = sgcl::net::oauth2;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // What every connection answers, and how many were made
    struct Server {
        std::string answer;
        std::atomic<int> dials{0};
        std::atomic<int> api{0};
        std::atomic<int> ended{0};
        std::atomic<bool> watched{false};
        async::stop_source stop;
    };

    // The device's polling stopped after 300 ms whatever the server said (a
    // slow_down adds 5 s to the interval)
    async::task<> watchdog(Server* s) {
        co_await async::sleep(std::chrono::milliseconds(300));
        s->stop.request_stop();
        s->watched = true;
    }

    async::task<> serve(net::connection c, Server* s) {
        // the request read up to the end of its head, its body after it (a form), then the answer
        std::string got;
        byte buf[4096];
        while (got.find("\r\n\r\n") == std::string::npos) {
            auto n = co_await c.async_read(slice<byte>(buf, sizeof buf));
            if (!n || *n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
        }
        if (got.compare(0, 8, "GET /api") == 0) {
            ++s->api;
        }
        (void)co_await c.async_write(string(s->answer));
        (void)co_await c.async_close();
        ++s->ended;
    }

    async::task<expected<net::connection, io::error>> dial(Server* s) {
        auto [a, b] = net::connection::in_memory();
        if (++s->dials >= 4) {
            s->stop.request_stop();   // the device's polling ends here
        }
        async::go(serve(b, s));
        co_return a;
    }

    oauth2::config config_of(Server* s) {
        oauth2::config c;
        c.client_id = "web";
        c.client_secret = "web-secret";
        c.endpoints.authorization = "http://as.test/authorize";
        c.endpoints.token = "http://as.test/token";
        c.endpoints.device_authorization = "http://as.test/device_authorization";
        c.endpoints.revocation = "http://as.test/revoke";
        c.endpoints.introspection = "http://as.test/introspect";
        c.redirect_url = "http://app.test/callback";
        c.scopes = {string("read")};
        c.http.dial = [s](const net::url&, async::stop_token) { return dial(s); };
        c.http.max_redirects = 2;
        c.http.timeout = std::chrono::seconds(5);
        return c;
    }

    void check_token(const expected<oauth2::token, oauth2::error>& t) {
        if (t) {
            check(!t->access_token.empty());
        } else if (!t.error().transport()) {
            check(!t.error().code().empty());
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    Server s;
    s.answer.assign(reinterpret_cast<const char*>(data) + 1, size - 1);
    auto c = config_of(&s);
    oauth2::token held;
    held.access_token = "at-1";
    held.refresh_token = "rt-1";
    held.token_type = "Bearer";
    switch (data[0] % 8) {
        case 0:
            check_token(c.exchange("code-1", oauth2::pkce::generate()));
            break;
        case 1:
            check_token(c.client_credentials());
            break;
        case 2:
            check_token(c.refresh(held));
            break;
        case 3: {
            auto d = c.device_authorize();
            if (d) {
                check(!d->device_code.empty() && !d->user_code.empty() && !d->verification_uri.empty());
                check(d->interval >= duration(std::chrono::seconds(1)));
            } else if (!d.error().transport()) {
                check(!d.error().code().empty());
            }
            break;
        }
        case 4: {
            oauth2::device_authorization d;
            d.device_code = "dc-1";
            d.user_code = "UC-1";
            d.verification_uri = "http://as.test/device";
            d.expiry = time::datetime::from_unix_milli(time::now().unix_milli() + 60000, time::zone::utc());
            d.interval = std::chrono::milliseconds(1);
            async::go(watchdog(&s));
            check_token(c.device_token(d, s.stop.token()));
            check(s.dials <= 4);
            break;
        }
        case 5: {
            auto r = c.revoke("at-1", "access_token");
            if (!r && !r.error().transport()) {
                check(!r.error().code().empty());
            }
            break;
        }
        case 6: {
            auto i = c.introspect("at-1");
            if (!i && !i.error().transport()) {
                check(!i.error().code().empty());
            }
            break;
        }
        case 7: {
            auto web = c.source(held).client();
            auto r = web.get("http://as.test/api");
            if (r) {
                (void)r->text();
            }
            check(s.api <= 2);
            break;
        }
    }
    c.http.close_idle_connections();   // an idle connection stays its idle_timeout (90 s) after its client is gone
    // the connections' tasks ended before their server goes (a few seconds at most)
    for (int i = 0; i < 5000 && (s.ended.load() < s.dials.load() || ((data[0] % 8) == 4 && !s.watched.load())); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
}

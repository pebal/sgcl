//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// OpenID Connect on any bytes, over connections in memory: the first byte
// picks what the input is — an ID token given to verify (the provider's
// discovery document and key set sound), the provider's discovery document,
// its key set (then a token of a key the harness holds verified), or the
// userinfo answer. What must hold:
//   - never a crash, never a hang;
//   - an ID token of the input never verifies: its signature would need
//     the harness's private key, which the input does not have;
//   - a provider discovered names the issuer asked for;
//   - userinfo held to a subject gives that subject or an error.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/oauth2/fuzz/oidc_fuzz.cpp)
// or the library's own driver.
#include "sgcl/net/oidc.h"
#include "sgcl/crypto/jose.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>

namespace {
    using namespace sgcl;
    namespace http = sgcl::net::http;
    namespace jose = sgcl::crypto::jose;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Keys {
        jose::jwk key = jose::jwk::generate(jose::algorithm::es256, {.kid = "k1"});
        string jwks = jose::jwk_set{key.public_key()}.to_json();
    };

    Keys& keys() {
        static root_ptr<Keys> k = make_tracked<Keys>();
        return *k;
    }

    struct Server {
        std::string discovery, jwks, userinfo;
        std::atomic<int> dials{0};
        std::atomic<int> ended{0};
    };

    std::string answer(std::string_view body) {
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
    }

    async::task<> serve(net::connection c, Server* s) {
        std::string got;
        byte buf[4096];
        while (got.find("\r\n\r\n") == std::string::npos) {
            auto r = co_await c.async_read(slice<byte>(buf, sizeof buf));
            if (!r || *r == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *r);
        }
        const std::string& body = got.find(" /.well-known/") != std::string::npos ? s->discovery
                                : got.find(" /keys ") != std::string::npos      ? s->jwks
                                                                                : s->userinfo;
        (void)co_await c.async_write(string(answer(body)));
        (void)co_await c.async_close();
        ++s->ended;
    }

    async::task<expected<net::connection, io::error>> dial(Server* s) {
        auto [a, b] = net::connection::in_memory();
        ++s->dials;
        async::go(serve(b, s));
        co_return a;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    Keys& k = keys();
    std::string_view in(reinterpret_cast<const char*>(data) + 1, size - 1);
    Server s;
    s.discovery = R"({"issuer":"http://op.test","jwks_uri":"http://op.test/keys","userinfo_endpoint":"http://op.test/userinfo"})";
    s.jwks = std::string(k.jwks.view());
    s.userinfo = R"({"sub":"ann"})";
    const int mode = data[0] % 4;
    if (mode == 1) {
        s.discovery.assign(in);
    } else if (mode == 2) {
        s.jwks.assign(in);
    } else if (mode == 3) {
        s.userinfo.assign(in);
    }
    {
        http::client c;
        Server* sp = &s;
        c.dial = [sp](const net::url&, async::stop_token) { return dial(sp); };
        c.timeout = std::chrono::seconds(5);
        auto p = net::oidc::provider::discover("http://op.test", c);
        if (p) {
            check(p->issuer() == "http://op.test");
            if (mode == 0) {
                check(!p->verify(string(in), {.client_id = "web"}));
            } else {
                auto claims = encoding::json::object({{"iss", "http://op.test"}, {"sub", "ann"}, {"aud", "web"},
                                                      {"exp", time::now().unix() + 600}, {"iat", time::now().unix()}});
                (void)p->verify(jose::jwt::sign(claims, k.key), {.client_id = "web"});
                net::oauth2::token t;
                t.access_token = "at";
                auto info = p->userinfo(t, "ann");
                if (info) {
                    check((*info)["sub"].as_string("") == "ann");
                }
            }
        }
        c.close_idle_connections();   // an idle connection stays its idle_timeout (90 s) after its client is gone
    }
    for (int i = 0; i < 5000 && s.ended.load() < s.dials.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
}

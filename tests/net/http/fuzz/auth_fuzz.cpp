//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The authentications on any bytes: the reader of challenges and
// credentials (parse_challenges), Basic's reader, RFC 8187's ext-value
// reader, and the server's middlewares (basic_auth, digest_auth with every
// algorithm and auth-int) answering a request whose Authorization is the
// input, through a response recorder. What must hold:
//   - never a crash;
//   - a challenge read has a scheme that is a token, parameter names in
//     lower case, and what a quoted value read back writes the same;
//   - Basic's credentials read back make the same field again;
//   - the middlewares answer 200 only for the one user and password they
//     know (the input never holds a Digest response of the password "pw",
//     which the harness does not give it), else 401 with challenges.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/auth_fuzz.cpp)
// or the library's own driver.
#include "sgcl/net/http/http.h"

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


    struct Servers {
        http::server basic;
        http::server digest;

        Servers() {
            basic.use(http::basic_auth("b", [](const string& u, const string& p) { return u == "ann" && p == "s3cret-pw!"; }));
            basic.route("/", [](http::request r, http::response_writer w) { w.write(r.authenticated_user()); });
            http::digest_auth::options o;
            o.algorithms = {http::digest_auth::algorithm::sha256, http::digest_auth::algorithm::md5, http::digest_auth::algorithm::sha512_256_sess,
                            http::digest_auth::algorithm::md5_sess};
            o.auth_int = true;
            digest.use(http::digest_auth("d", [](const string& u) -> optional<string> {
                if (u == "ann") {
                    return string("s3cret-pw!");
                }
                return nullopt;
            }, o));
            digest.route("/", [](http::request r, http::response_writer w) -> async::task<> {
                auto t = co_await r.async_text();
                w.write(r.authenticated_user());
            });
        }
    };

    Servers& servers() {
        static root_ptr<Servers> s = make_tracked<Servers>();
        return *s;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view in(reinterpret_cast<const char*>(data), size);
    for (auto& c : http::detail::parse_challenges(in)) {
        check(!c.scheme.empty() && http::detail::is_token(c.scheme));
        for (auto& p : c.params) {
            for (char ch : p.first) {
                check(!(ch >= 'A' && ch <= 'Z'));
            }
            std::string quoted;
            http::detail::append_quoted(quoted, p.second);
            size_t used = 0;
            Exact exact(quoted);
            auto back = http::detail::read_quoted(exact.view(), used);
            check(back && *back == p.second && used == quoted.size());
        }
    }
    if (auto up = http::detail::read_basic(in)) {
        Exact exact(http::detail::basic_credentials(up->first, up->second).view());
        auto again = http::detail::read_basic(exact.view());
        check(again && again->first == up->first && again->second == up->second);
    }
    if (auto v = http::detail::decode_ext_value(in)) {
        const std::string encoded = http::detail::encode_ext_value(*v);
        Exact exact(encoded);
        auto back = http::detail::decode_ext_value(exact.view());
        check(back && *back == *v);
    }
    for (int k : {0, 1}) {
        auto req = http::test_request("POST", "/", string("the body"));
        req.headers().add("Authorization", string(in));
        http::response_recorder rec;
        rec.serve(k == 0 ? servers().basic : servers().digest, req);
        const int st = rec.status();
        check(st == 200 || st == 401 || st == 400 || st == 413);
        if (st == 200) {
            check(rec.body() == "ann");
            check(k == 0);   // a Digest response of the password is not among the inputs
        } else if (st == 401) {
            check(!rec.headers().get_all("WWW-Authenticate").empty());
        }
    }
    return 0;
}

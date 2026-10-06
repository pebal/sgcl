//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The JSON Web Signatures of ACME on any bytes (acme/detail/jws_verify.h,
// key.h): the input read as a JWS in the flattened form as a server reads
// a request, its JWK made a key, its signature checked; and the input used
// as the payload and the URL of a request the client signs, read back (the URL
// as JSON carries it: invalid UTF-8 as U+FFFD). What
// must hold: nothing crashes or hangs; a JWK read has its canonical text and a thumbprint of 43 characters; every
// request the client signs reads back with its payload and URL and verifies
// under the key's own JWK.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/acme/fuzz/acme_jws_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/acme/detail/jws_verify.h"

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
    std::string_view input(reinterpret_cast<const char*>(data), size);
    // a server's read of a request
    auto p = d::parse_jws(string(input));
    if (p) {
        string signing = p->signing_input();
        auto sig_input = slice<const byte>(reinterpret_cast<const byte*>(signing.data()), signing.size());
        if (!p->jwk.is_null()) {
            if (auto k = d::jwk_key(p->jwk)) {
                check(!k->canonical.empty() && k->thumbprint.size() == 43);
                (void)d::jws_verify(*k, p->alg.view(), sig_input, p->signature.as_slice());
            }
        }
    }
    // the client's own request of this payload and URL, read back
    net::acme::account_key key(size % 3 == 0 ? net::acme::key_algorithm::eddsa : net::acme::key_algorithm::es256);
    const auto& st = d::KeyAccess::state(key);
    string payload(input);
    encoding::json header = d::header(st, size % 2 ? string() : string("https://ca.test/acme/account/1"), string("nonce"), payload);
    string body = d::jws(st, header, payload);
    auto back = d::parse_jws(body);
    check(back.has_value());
    check(back->payload == payload && back->nonce == "nonce");
    // the URL as JSON carries it: itself when it is UTF-8, U+FFFD in place of what is not
    auto as_json = encoding::json::parse(encoding::json(payload).to_string());
    check(as_json && back->url == as_json->as_string(string()));
    string signing = back->signing_input();
    auto mine = d::jwk_key(*encoding::json::parse(key.jwk()));
    check(mine.has_value());
    check(d::jws_verify(*mine, back->alg.view(), slice<const byte>(reinterpret_cast<const byte*>(signing.data()), signing.size()), back->signature.as_slice()));
    return 0;
}

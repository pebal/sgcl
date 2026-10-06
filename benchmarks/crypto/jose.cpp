//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::jose against Go (benchmarks/go/jose: JOSE written by hand over Go's
// standard library, which has none): one case a run, the keys made before
// the clock starts. Prints one line with ns/op.
//
//   jose <case> sgcl
//
//   jwt_hs256_sign, jwt_es256_sign, jwt_es512_sign, jwt_rs256_sign, jwt_eddsa_sign
//       a JWT of five claims signed, compact
//   jwt_hs256_verify, jwt_es256_verify, jwt_es512_verify, jwt_rs256_verify, jwt_eddsa_verify
//       that token verified, its claims read and exp, nbf and aud checked
//   jwe_dir_a256gcm, jwe_ecdh_es_a128cbc
//       1 KB encrypted to the key and decrypted again (dir A256GCM; ECDH-ES
//       on P-256 with A128CBC-HS256)
//   jwk_parse_ec
//       a public JWK of P-256 read into a key
//   jwk_parse_rsa_verify
//       a public JWK of RSA 2048 read into a key and an RS256 JWT verified
//       with it: what a key set's new key costs (the module readies the
//       modulus when the key is read, Go when it verifies)
//
// About two seconds a run after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    using namespace sgcl;
    namespace jose = sgcl::crypto::jose;

    volatile uint64_t sink;

    template<class F>
    std::pair<uint64_t, double> run_for(F&& f, double seconds) {
        uint64_t calls = 0;
        uint64_t acc = 0;
        auto t0 = bench::Clock::now();
        double wall = 0;
        do {
            for (int i = 0; i < 16; ++i) {
                acc += f();
            }
            calls += 16;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }

    template<class F>
    void measure(const char* what, F&& f) {
        run_for(f, 0.25);
        auto [calls, wall] = run_for(f, 2.0);
        std::printf("jose %s ns/op=%.1f wall=%.2fs\n", what, wall * 1e9 / double(calls), wall);
    }

    encoding::json claims() {
        return encoding::json::object({{"iss", "https://id.example"}, {"sub", "alice"}, {"aud", "api"}, {"iat", 1700000000}, {"exp", 2000000000}});
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: jose <case> sgcl\n");
        return 2;
    }
    const std::string what = argv[1];
    auto key_of = [&](const std::string& c) {
        if (c.find("hs256") != std::string::npos) {
            return jose::jwk::generate(jose::algorithm::hs256);
        }
        if (c.find("es256") != std::string::npos) {
            return jose::jwk::generate(jose::algorithm::es256);
        }
        if (c.find("rs256") != std::string::npos) {
            return jose::jwk::generate(jose::algorithm::rs256);
        }
        if (c.find("es512") != std::string::npos) {
            return jose::jwk::generate(jose::algorithm::es512);
        }
        return jose::jwk::generate(jose::algorithm::eddsa);
    };
    const jose::jwt::verify_options at{.audience = "api", .at = time::datetime::from_unix(1800000000, time::zone::utc())};
    if (what.rfind("jwt_", 0) == 0 && what.find("_sign") != std::string::npos) {
        auto key = key_of(what);
        auto c = claims();
        measure(what.c_str(), [&] { return jose::jwt::sign(c, key).size(); });
    } else if (what.rfind("jwt_", 0) == 0) {
        auto key = key_of(what);
        auto pub = key.type() == jose::key_type::oct ? key : key.public_key();
        auto token = jose::jwt::sign(claims(), key);
        measure(what.c_str(), [&] {
            auto t = jose::jwt::verify(token, pub, at);
            return t ? t->subject().size() : 0;
        });
    } else if (what == "jwe_dir_a256gcm" || what == "jwe_ecdh_es_a128cbc") {
        const bool dir = what == "jwe_dir_a256gcm";
        auto key = dir ? jose::jwk::symmetric(crypto::random::secret(32), {.alg = jose::algorithm::dir}) : jose::jwk::generate(jose::algorithm::ecdh_es);
        auto to = dir ? key : key.public_key();
        std::string data(1024, 'x');
        const jose::encrypt_options o{.enc = dir ? jose::encryption::a256gcm : jose::encryption::a128cbc_hs256};
        const slice<const byte> pt(reinterpret_cast<const byte*>(data.data()), data.size());
        measure(what.c_str(), [&] {
            auto c = jose::jwe::encrypt(pt, to, o);
            auto p = jose::jwe::decrypt(c, key);
            return p ? p->size() : 0;
        });
    } else if (what == "jwk_parse_ec") {
        auto key = jose::jwk::generate(jose::algorithm::es256, {.kid = "k1"});
        auto text = key.to_json();
        measure(what.c_str(), [&] {
            auto k = jose::jwk::parse(text);
            return k ? k->kid().size() : 0;
        });
    } else if (what == "jwk_parse_rsa_verify") {
        auto key = jose::jwk::generate(jose::algorithm::rs256, {.kid = "k1"});
        auto text = key.to_json();
        auto token = jose::jwt::sign(claims(), key);
        measure(what.c_str(), [&] {
            auto k = jose::jwk::parse(text);
            auto t = jose::jwt::verify(token, *k, at);
            return t ? t->subject().size() : 0;
        });
    } else {
        std::fprintf(stderr, "jose: unknown case %s\n", what.c_str());
        return 2;
    }
    return 0;
}
